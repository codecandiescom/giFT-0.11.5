/*
 * $Id: download_state.c,v 1.6 2003/11/03 11:30:29 jasta Exp $
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

#include "lib/file.h"

#include "transfer.h"
#include "download.h"

#include "download_state.h"

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

static String *buffer_state (Transfer *t)
{
	String *buf;
	List   *ptr;
	int     i = 0;

	if (!(buf = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	string_append  (buf, "[transfer]\n");
	string_appendf (buf, "uniq = %s\n",      DOWNLOAD(t)->uniq);
	string_appendf (buf, "filename = %s\n",  t->filename);

	if (t->hash)
		string_appendf (buf, "hash = %s\n",  t->hash);

	if (t->path)
		string_appendf (buf, "path = %s\n",  t->path);

	string_appendf (buf, "transmit = %lu\n",  (unsigned long)t->transmit);
	string_appendf (buf, "total = %lu\n",     (unsigned long)t->total);
	string_appendf (buf, "max_seek = %lu\n",  (unsigned long)t->max_seek);

	/*
	 * Add all the chunks on one line in the form:
	 *
	 * chunks = start1-stop1 start2-stop2 startN-stopN ...
	 */
	string_append (buf, "chunks = ");

	for (ptr = t->chunks; ptr; ptr = list_next (ptr))
	{
		Chunk *chunk = ptr->data;

		string_appendf (buf, "%lu-%lu ",
		                (unsigned long)(chunk->start + chunk->transmit),
		                (unsigned long)(chunk->stop));
	}

	/* one newline for the loop above, another to start a new header */
	string_appendf (buf, "\n\n");

	/*
	 * Write every source using an incrementing header name.  This is very
	 * hackish and should be changed to something more sane at some point.
	 */
	for (ptr = t->sources; ptr; ptr = list_next (ptr))
	{
		Source *source = ptr->data;

		string_appendf (buf, "[source%i]\n", i++);
		string_appendf (buf, "user = %s\n", source->user);
		string_appendf (buf, "url = %s\n", source->url);
		string_append  (buf, "\n");
	}

	return buf;
}

static BOOL write_state (Transfer *t, String *statebuf)
{
	FILE  *f;
	char  *tmppath;
	size_t n;

	/*
	 * Use a temp file to write to in case of error.  If an error is
	 * encountered (eg "No space left on device"), we will gracefully back
	 * out and leave the original state file in tact.  Eventually we should
	 * deploy a scheme that will allow the runtime code to reload the
	 * transfer from the previously written state file to prevent the local
	 * transmit values from remaining ahead of the synced file.
	 */
	if (!(tmppath = stringf ("%s.tmp", DOWNLOAD(t)->state_path)))
		return FALSE;

	if (!(f = fopen (tmppath, "w")))
		return FALSE;

	/* write all of our buffered data in a single call for convenience */
	n = fwrite (statebuf->str, sizeof (char), (size_t)statebuf->len, f);

	if (n < statebuf->len)
	{
		GIFT_ERROR (("unable to write to %s: %s", tmppath, GIFT_STRERROR()));
		return FALSE;
	}

	/* make sure we dont have any flushing errors */
	if (fclose (f))
	{
		GIFT_ERROR (("unable to flush %s: %s", tmppath, GIFT_STRERROR()));
		return FALSE;
	}

	if (!file_mv (tmppath, DOWNLOAD(t)->state_path))
	{
		GIFT_ERROR (("unable to move %s to %s: %s",
		             tmppath, DOWNLOAD(t)->state_path, GIFT_STRERROR ()));
	}

	/* phew, all is well... */
	return TRUE;
}

BOOL download_state_save (Transfer *t)
{
	String *statebuf;
	BOOL    ret;

	if (!t || !t->chunks)
		return TRUE;

	assert (DOWNLOAD(t)->uniq != NULL);

	/* the state_path is lazily created the first time the state file
	 * needs to be written... */
	if (!DOWNLOAD(t)->state_path)
	{
		DOWNLOAD(t)->state_path =
		    STRDUP (INCOMING_PATH ((".%s.state", DOWNLOAD(t)->uniq)));
	}

	/* sync all writes to the temp file before we commit a state file to
	 * disk permanently recording these values */
	if (!(download_sync (t)))
	{
		GIFT_ERROR (("unable to sync transfer!"));
		return FALSE;
	}

	/* buffer the entire state file into memory to simplify writing... */
	if (!(statebuf = buffer_state (t)))
	{
		GIFT_ERROR (("unable to create state file buffer!"));
		return FALSE;
	}

	/* safely write the buffer to disk */
	ret = write_state (t, statebuf);
	string_free (statebuf);

	if (!ret)
	{
		GIFT_ERROR (("unable to write state file buffer!"));
		return FALSE;
	}

	/* process is all done and completely successful */
	return TRUE;
}

/*****************************************************************************/

/*
 * Load all the chunks into the transfer object
 *
 * FORMAT (chunks):
 * 0-100 100-200 200-300 300-400
 */
static void fill_chunks (Transfer *t, char *chunks)
{
	char       *token;
	char       *chunks0;
	off_t       start;
	off_t       stop;
	struct stat st;

	/* sanity checking on the temp files size */
	if (!file_stat (t->path, &st))
	{
		if (t->path)
			GIFT_ERROR (("unable to stat %s: %s", t->path, GIFT_STRERROR()));

		return;
	}

	/* make sure we're dealing w/ our own memory here */
	chunks0 = chunks = STRDUP (chunks);

	while ((token = string_sep (&chunks, " ")))
	{
		start = (off_t)(gift_strtoul (string_sep (&token, "-")));
		stop  = (off_t)(gift_strtoul (token));

		/* huh? */
		if (start >= stop)
		{
			GIFT_ERROR (("transfer start %lu overruns stop %lu",
			             (unsigned long)start, (unsigned long)stop));
			continue;
		}

		/*
		 * Make sure that the first chunk does not try to start beyond
		 * the actual file size.
		 *
		 * NOTE:
		 * The chunks list must be sorted prior to calling this routine
		 * or this will fail to accomplish it's goal.
		 */
		if (!t->chunks && start > st.st_size)
			start = st.st_size;

		/* create this unfulfilled chunk division */
		chunk_new (t, NULL, start, stop);
	}

	free (chunks0);
}

/* reads a state file and attempts to construct a transfer from it */
static Transfer *read_state (char *file)
{
	Transfer     *t;
	Config       *conf;
	char         *uniq;
	char         *filename;
	char         *hash;
	char         *chunks;
	char          key_name[256];
	int           s_offs = 0;
	unsigned long total;

	file = STRDUP (file);

	if (!(conf = config_new_ex (file, FALSE)))
		return NULL;

	uniq     =        config_get_str (conf, "transfer/uniq");
	filename =        config_get_str (conf, "transfer/filename");
	hash     =        config_get_str (conf, "transfer/hash");
	total    = ATOUL (config_get_str (conf, "transfer/total"));
	chunks   =        config_get_str (conf, "transfer/chunks");

	/* make sure it's at least reasonably complete...if they fuck with their
	 * state file too much though, fuck them */
	if (!filename || !hash || !chunks || !total)
	{
		GIFT_TRACE (("%s: corrupt state file", file));
		config_free (conf);
		free (file);
		return NULL;
	}

	GIFT_TRACE (("awaking state transfer %s", filename));

	t = download_new (NULL, 0, uniq, filename, hash, (off_t)total);
	DOWNLOAD(t)->state_path = file;
	t->path                 = STRDUP (config_get_str (conf, "transfer/path"));
	t->transmit             = config_get_int (conf, "transfer/transmit");
#if 0
	t->total                = config_get_int (conf, "transfer/total");
#endif
	t->max_seek             = config_get_int (conf, "transfer/max_seek");
#if 0
	t->transmit_change = t->transmit;
#endif

	fill_chunks (t, chunks);

	/* loop the source headers */
	for (s_offs = 0 ;; s_offs++)
	{
		char *user;
		char *url;

		snprintf (key_name, sizeof (key_name) - 1, "source%i/url", s_offs);

		/* check to see if we really have a conf header here or not */
		if (!(url = config_get_str (conf, key_name)))
			break;

		snprintf (key_name, sizeof (key_name) - 1, "source%i/user", s_offs);
		user = config_get_str (conf, key_name);

		if (!user)
		{
			GIFT_DBG (("corrupt resume entry: source%i", s_offs));
			continue;
		}

		download_add_source (t, user, t->hash, url);
	}

	config_free (conf);

	return t;
}

/* restore the previously recorded transfer states from disk */
int download_state_recover (void)
{
	char          *path;
	DIR           *dir;
	struct dirent *d;
	int            recovered = 0;

	path = INCOMING_PATH ((""));

	/* scan the incoming directory for any incomplete state files */
	if (!(dir = file_opendir (path)))
	{
		GIFT_ERROR (("cannot open %s: %s", path, GIFT_STRERROR()));
		return 0;
	}

	while ((d = file_readdir (dir)))
	{
		size_t name_len = strlen (d->d_name);

		/* not long enough for .a.state */
		if (name_len < 8)
			continue;

		/* look for ^. and state$ */
		if (d->d_name[0] == '.' &&
		    !strcmp (d->d_name + (name_len - 5), "state"))
		{
			/* restore this individual download */
			if ((read_state (INCOMING_PATH (("%s", d->d_name)))))
				recovered++;
		}
	}

	file_closedir (dir);

	/* return the total number of recovered state files */
	return recovered;
}
