/*
 * $Id: plugin.c,v 1.59 2003/10/30 09:20:41 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "giftd.h"

#ifdef USE_LTDL
# include <ltdl.h>
#endif /* USE_LTDL */

#include "plugin/protocol.h"
#include "plugin/share.h"

#include "lib/event.h"
#include "lib/file.h"

#include "plugin.h"

/* protocol functbl */
#include "if_event.h"
#include "share_local.h"
#include "if_search.h"
#include "if_message.h"
#include "download.h"
#include "upload.h"
#include "transfer.h"

/*****************************************************************************/

#ifndef USE_LTDL
# error libltdl support is temporarily required, sorry!
#endif /* !USE_LTDL */

/*****************************************************************************/

/* index all currently loaded plugins */
static Dataset *plugins = NULL;

/*****************************************************************************/

static Protocol *plugin_new (const char *pname)
{
	Protocol *p;

	/* check to make sure we're linked against a compatible
	 * libgiftproto version */
	if (protocol_compat_ex (NULL, LIBGIFTPROTO_VERSION, 0) != 0)
	{
		GIFT_FATAL (("runtime incompatibility detected!"));
		return NULL;
	}

	/* construct the Protocol object */
	if (!(p = MALLOC (sizeof (Protocol))))
		return NULL;

	if (!(p->name = STRDUP (pname)))
	{
		free (p);
		return NULL;
	}

	return p;
}

static void plugin_free (Protocol *p)
{
	if (!p)
		return;

	free (p->name);
	free (p->version_str);
	free (p);
}

static void plugin_add (Protocol *p)
{
	assert (p != NULL);

	/* this better not happen */
	assert (dataset_lookupstr (plugins, p->name) == NULL);

	if (!plugins)
	{
		plugins = dataset_new (DATASET_LIST);
		assert (plugins != NULL);
	}

	dataset_insert (&plugins, p->name, STRLEN_0(p->name), p, 0);
}

static void plugin_remove (Protocol *p)
{
	assert (p != NULL);

	dataset_removestr (plugins, p->name);
}

/*****************************************************************************/

/* see setup_functbl */
static BOOL  dummy_bool_false   (/*< void >*/) { return FALSE; }
static BOOL  dummy_bool_true    (/*< void >*/) { return TRUE; }
static int   dummy_int_zero     (/*< void >*/) { return 0; }
static void  dummy_void         (/*< void >*/) { /* ... */ }
static void *dummy_voidptr_null (/*< void >*/) { return NULL; }

/*****************************************************************************/

/*
 * This is just terrible.  Please make me fix this.
 */
#define APPENDMSG msg + msgwr, sizeof (msg) - msgwr - 1
#define LOGMSG(fmt,pfx1,pfx2)                                              \
	char    msg[4096];                                                     \
	size_t  msgwr = 0;                                                     \
	va_list args;                                                          \
	if (pfx1)                                                              \
		msgwr += snprintf (APPENDMSG, "%s: ", STRING_NOTNULL(pfx1));       \
	if (pfx2)                                                              \
		msgwr += snprintf (APPENDMSG, "[%s]: ", STRING_NOTNULL(pfx2));     \
	va_start (args, fmt);                                                  \
	vsnprintf (APPENDMSG, fmt, args);                                      \
	va_end (args);

static int wrapper_dbg (Protocol *p, char *fmt, ...)
{
	LOGMSG (fmt, p->name, NULL);
	log_print (LOG_DEBUG, msg);

	return TRUE;
}

static int wrapper_dbgsock (Protocol *p, TCPC *c, char *fmt, ...)
{
	char hoststr[32];

	net_ip_strbuf (c->host, hoststr, sizeof (hoststr));

	{ /* ugh */
		LOGMSG (fmt, p->name, hoststr);
		log_print (LOG_DEBUG, msg);
	}

	return TRUE;
}

static int wrapper_trace (Protocol *p, char *file, int line, char *func,
						  char *fmt, ...)
{
	LOGMSG (fmt, NULL, NULL);

	return wrapper_dbg (p, "%s:%i(%s): %s", file, line, func, msg);
}

static int wrapper_tracesock (Protocol *p, TCPC *c, char *file, int line, char *func, char *fmt, ...)
{
	LOGMSG (fmt, NULL, NULL);

	return wrapper_dbgsock (p, c, "%s:%i(%s): %s", file, line, func, msg);
}

static int wrapper_warn (Protocol *p, char *fmt, ...)
{
	LOGMSG (fmt, p->name, NULL);
	log_warn ("%s", msg);

	return TRUE;
}

static int wrapper_err (Protocol *p, char *fmt, ...)
{
	LOGMSG (fmt, p->name, NULL);
	log_error ("%s", msg);

	return TRUE;
}

/*****************************************************************************/

/* default hash visualization is to simply convert to a NUL terminated hex
 * string */
static char *hash_dspfn_default (unsigned char *hash, size_t len)
{
	char *result, *result_ptr;
    static char hex_table[16] =
	{
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'a', 'b', 'c', 'd', 'e', 'f'
	};

	assert (hash != NULL);
	assert (len > 0);

	/* allocate enough storage to expand to ascii */
	if (!(result = malloc ((len * 2) + 1)))
		return NULL;

	result_ptr = result;

	while (len-- > 0)
	{
		unsigned char c = *hash++;

		*result_ptr++ = hex_table[(c & 0xf0) >> 4];
		*result_ptr++ = hex_table[(c & 0x0f)];
	}

	*result_ptr++ = 0;

	return result;
}

static int wrapper_hash_handler (Protocol *p, const char *type, int opt,
                                 HashFn algofn, HashDspFn dspfn)
{
	/* provide a default human hashing interface */
	if (!dspfn)
		dspfn = hash_dspfn_default;

	return hash_algo_register (p, type, opt, algofn, dspfn);
}

static void wrapper_support (Protocol *p, const char *feature, BOOL spt)
{
	dataset_insert (&p->features, feature, STRLEN_0(feature),
	                &spt, sizeof (spt));
}

BOOL plugin_support (Protocol *p, const char *feature)
{
	BOOL *spt;

	if ((spt = dataset_lookup (p->features, feature, STRLEN_0(feature))))
		return *spt;

	return FALSE;
}

static void wrapper_search_result (Protocol *p, IFEvent *event, char *user,
                                   char *node, char *href, unsigned int avail,
                                   FileShare *file)
{
	if_search_result (event, user, node, href, avail, file);
}

static void wrapper_search_complete (Protocol *p, IFEvent *event)
{
	if_search_remove (event, p);
}

static int wrapper_upload_auth (Protocol *p, const char *user, Share *share,
                                upload_auth_t *auth_info)
{
	return upload_auth (p, user, share, auth_info);
}

static BOOL find_primary_hash (ds_data_t *key, ds_data_t *value, void *udata)
{
	HashAlgo *algo = value->data;

	return BOOL_EXPR (algo->opt & HASH_PRIMARY);
}

static char *get_primary_hash_algo (Protocol *p)
{
	HashAlgo *algo;

	if (!(algo = dataset_find (p->hashes, DS_FIND(find_primary_hash), NULL)))
		return NULL;

	return ((char *)algo->type);
}

static Transfer *wrapper_upload_start (Protocol *p, Chunk **chunk,
                                       const char *user, Share *share,
                                       off_t start, off_t stop)
{
	Transfer *t;
	char     *hash_algo;
	char     *hash_dsp;

	hash_algo = get_primary_hash_algo (p);
	assert (hash_algo != NULL);

	/* share_dsp_hash will use the TYPE:HASH hack for us */
	hash_dsp = share_dsp_hash (share, hash_algo);
	assert (hash_dsp != NULL);

	/* and now for the ugly internal interface */
	t = upload_new (p, (char *)user, hash_dsp,
	                file_basename (share->path), share->path,
	                start, stop, TRUE, TRUE);

	/* there should only be one chunk...check that later */
	if (t && chunk)
		*chunk = list_nth_data (t->chunks, 0);

	free (hash_dsp);

	return t;
}

static void wrapper_chunk_write (Protocol *p, Transfer *transfer,
                                 Chunk *chunk, Source *source,
                                 unsigned char *data, size_t len)
{
	chunk_write (chunk, data, len);
}

static void wrapper_source_abort (Protocol *p, Transfer *transfer,
                                  Source *source)
{
	Chunk *chunk;

	assert (transfer != NULL);
	assert (source != NULL);

	chunk = source->chunk;

	download_remove_source (transfer, source);

	/*
	 * Unreference the source and mark this chunk as inactive.
	 *
	 * TODO: I am uncertain if this should be before download_remove_source,
	 * I am only copying what OpenFT has done as a hack in the past.  Someone
	 * should check this out...
	 */
	if (chunk)
		chunk->source = NULL;
}

static void wrapper_message (Protocol *p, char *message)
{
	if_message_send (NULL, p, message);
}

static Share *wrapper_share_lookup (Protocol *p, int command, ...)
{
	Share  *share;
	va_list args;

	va_start (args, command);
	share = share_local_lookupv (command, args);
	va_end (args);

	return share;
}

/*****************************************************************************/

static void default_source_status (Protocol *p, Source *source,
                                   enum source_status klass, const char *disptxt)
{
	source_status_set (source, (SourceStatus)klass, (char *)disptxt);
}

static int default_user_cmp (Protocol *p, const char *a, const char *b)
{
	return STRCMP (a, b);
}

static int default_source_cmp (Protocol *p, Source *a, Source *b)
{
	int ret;

	assert (a != NULL);
	assert (b != NULL);

	if ((ret = STRCMP (a->url, b->url)))
		return ret;

	if ((ret = STRCMP (a->hash, b->hash)))
		return ret;

	return default_user_cmp (p, a->user, b->user);
}

/*****************************************************************************/

#ifdef _MSC_VER
/*
 * Shuts up:
 * warning C4113: 'int (__cdecl *)()' differs in parameter lists from
 * 'int (__cdecl *)(struct _protocol *,char *,unsigned char *(__cdecl *)(char *,
 * char *,int *),char *(__cdecl *)(unsigned char *,int *))'
 */
#pragma warning(disable: 4113)
#endif /* _MSC_VER */

static void functbl_gift (Protocol *p)
{
	/* logging */
	p->trace           = wrapper_trace;
	p->tracesock       = wrapper_tracesock;
	p->dbg             = wrapper_dbg;
	p->dbgsock         = wrapper_dbgsock;
	p->warn            = wrapper_warn;
	p->err             = wrapper_err;

	/* hashing */
	p->hash_handler    = wrapper_hash_handler;

	/* plugin features */
	p->support         = wrapper_support;

	/* share cache interaction */
	p->share_lookup    = wrapper_share_lookup;

	/* transfers */
	p->upload_auth     = wrapper_upload_auth;
	p->upload_start    = wrapper_upload_start;
	p->chunk_write     = wrapper_chunk_write;
	p->source_abort    = wrapper_source_abort;
	p->source_status   = default_source_status;

	/* searching */
	p->search_result   = wrapper_search_result;
	p->search_complete = wrapper_search_complete;

	/* deprecated */
	p->message         = wrapper_message;
}

static void functbl_plugin (Protocol *p)
{
	/* we don't have a value to set here, but we know it must have been
	 * set by now */
	assert (p->init != NULL);

	/* initialization and destruction */
	p->start           = dummy_bool_false;
	p->destroy         = dummy_void;

	/* transfers */
	p->download_start  = dummy_bool_false;
	p->download_stop   = dummy_void;
    p->upload_stop     = dummy_void;
	p->upload_avail    = dummy_void;
	p->chunk_suspend   = dummy_bool_false;
	p->chunk_resume    = dummy_bool_false;
	p->source_add      = dummy_bool_true;
	p->source_remove   = dummy_void;

	/* searching */
	p->search          = dummy_bool_false;
	p->browse          = dummy_bool_false;
	p->locate          = dummy_bool_false;
	p->search_cancel   = dummy_void;

	/* share cache event broadcasts */
	p->share_new       = dummy_voidptr_null;
	p->share_free      = dummy_void;
	p->share_add       = dummy_bool_true;
	p->share_remove    = dummy_bool_true;
	p->share_sync      = dummy_void;
	p->share_hide      = dummy_void;
	p->share_show      = dummy_void;

	/* network statistics */
	p->stats           = dummy_int_zero;

	/* comparison routines */
	p->source_cmp      = default_source_cmp;
	p->user_cmp        = default_user_cmp;
}

static void setup_functbl (Protocol *p, BOOL (*init) (Protocol *p))
{
	/*
	 * Assign the core initialization function that we parsed out of the file
	 * path.  This will always be pluginname_init.
	 */
	p->init = init;

	/*
	 * Setup all methods that are intended to follow the plugin-to-gift
	 * communication model.  That is, plugins will call these generally
	 * unprovoked to interact with the daemon.
	 */
	functbl_gift (p);

	/*
	 * Setup the remaining plugin methods using default values (guarantees
	 * all methods non-null so the plugin doesn't have to provide dummy
	 * implementations.  These methods will be called from within giFT space
	 * and plugins are expected to assign proper values (over the dummy
	 * defaults defined here) to handle various giFT requests.
	 */
	functbl_plugin (p);
}

static Protocol *init_plugin (const char *file, const char *pname)
{
	Protocol    *p;
	int        (*init) (Protocol *p);
#ifdef USE_LTDL
	lt_dlhandle  handle;
#endif /* USE_LTDL */

	init = NULL;

#ifdef USE_LTDL
	if (!(handle = lt_dlopen (file)))
	{
		GIFT_FATAL (("couldn't load protocol in file %s: %s",
		             STRING_NOTNULL(file), lt_dlerror ()));
		return NULL;
	}

	init = lt_dlsym (handle, stringf ("%s_init", pname));
#endif /* USE_LTDL */

	if (!init)
	{
		char *error;

#ifdef USE_LTDL
		error = (char *)lt_dlerror ();
#else
		error = "binary does not support this protocol, consider ltdl support";
#endif

		GIFT_FATAL (("couldn't load protocol %s in file %s: %s",
		             pname, STRING_NOTNULL(file), error));

		return NULL;
	}

	/* create the protocol structure and store this data */
	if (!(p = plugin_new (pname)))
		return NULL;

#ifdef USE_LTDL
	p->handle = handle;
#endif /* USE_LTDL */

	/* make sure all communication is defined */
	setup_functbl (p, init);

	/* initialize the plugin */
	if (!(p->init (p)))
	{
		GIFT_FATAL (("%s (%s): init func failed", pname, file));

#ifdef USE_LTDL
		lt_dlclose (handle);
#endif /* LTDL */

		plugin_free (p);
		return NULL;
	}

	return p;
}

Protocol *plugin_init (const char *file, const char *pname)
{
	Protocol *p;

	if (!pname)
	{
		GIFT_ERROR (("cannot load unnamed protocol from file %s",
		             STRING_NOTNULL(file)));
		return NULL;
	}

	/*
	 * Dynamically load the library and run the _init function.  Actually, it
	 * may use dynamic loading, if the plugin support was compiled without
	 * ltdl, Gnutella and OpenFT will still be able to load as they were
	 * linked directly with the binary.
	 */
	if ((p = init_plugin (file, pname)))
		plugin_add (p);

	return p;
}

BOOL plugin_start (Protocol *p)
{
	BOOL ret;

	assert (p != NULL);

	/* attempt to start the protocol's connections */
	if (!(ret = p->start (p)))
	{
		GIFT_FATAL (("unable to start '%s', no idea why (hopefully "
		             "the protocol provided some info)", p->name));
		return FALSE;
	}

	/* deliver the initial upload availability */
	p->upload_avail (p, upload_availability());

	return ret;
}

/*****************************************************************************/

static void plugin_close (Protocol *p)
{
	if (!p)
		return;

	/* get the protocol to kill itself */
	p->destroy (p);

#if 0
	/* remove all inputs from this protocol */
	input_remove_full (p, NULL, 0, NULL);
#endif

#ifdef USE_LTDL
	if (p->handle)
		lt_dlclose (p->handle);
#endif /* USE_LTDL */
}

void plugin_unload (Protocol *p)
{
	if (!p)
		return;

	plugin_close (p);
	plugin_remove (p);
	plugin_free (p);
}

/*****************************************************************************/

static int unload_plugin (ds_data_t *key, ds_data_t *value, void *udata)
{
	Protocol *p = value->data;

	/* make sure the protocol matches its own entry...just for S&G */
	assert (strcmp (key->data, p->name) == 0);

	/* destroy all the allocated memory */
	plugin_close (p);
	plugin_free (p);

	/* this is suitable for plugin_remove */
	return DS_CONTINUE | DS_REMOVE;
}

void plugin_unloadall (void)
{
	dataset_foreach_ex (plugins, DS_FOREACH_EX(unload_plugin), NULL);
	dataset_clear (plugins);
}

/*****************************************************************************/

static int lookup_casecmp (ds_data_t *key, ds_data_t *value, const char *pname)
{
	char *keystr;

	/* store as a char* and check for NUL termination */
	keystr = key->data;
	assert (keystr[key->len - 1] == 0);

	if (strcasecmp (keystr, pname) == 0)
		return DS_FOUND;

	return DS_NOTFOUND;
}

Protocol *plugin_lookup (const char *pname)
{
	if (!pname)
		return NULL;

	/* apply a case-insensitive lookup, mainly to comply with RFC1738's
	 * specification of the URL scheme */
	return dataset_find (plugins, DS_FIND(lookup_casecmp), (char *)pname);
}

void plugin_foreach (DatasetForeachFn foreachfn, void *udata)
{
	dataset_foreach (plugins, DS_FOREACH(foreachfn), udata);
}
