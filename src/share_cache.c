/*
 * $Id: share_cache.c,v 1.64 2003/10/31 13:14:48 jasta Exp $
 *
 * TODO: This file desperately needs to be rewritten.  Volunteers are
 * encouraged!
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

#include "plugin/protocol.h"
#include "plugin/share.h"
#include "plugin.h"

#include "lib/parse.h"
#include "lib/conf.h"
#include "lib/network.h"
#include "lib/file.h"
#include "lib/event.h"
#include "lib/mime.h"

#include "share_cache.h"
#include "share_db.h"

#include "meta.h"

#include "upload.h"
#include "download.h"

#include "if_share.h"

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

#ifndef WIN32
#define TRACK_DIR_INODES
#endif /* !WIN32 */

/*****************************************************************************/

#ifdef TRACK_DIR_INODES
struct _dir_inode
{
	ino_t inode;
	dev_t dev;
};
#endif /* TRACK_DIR_INODES */

struct _share_root
{
	char         *path;
	unsigned char recurse : 1;
};

/*****************************************************************************/

static Dataset      *local_shares     = NULL;
static List         *local_duplicates = NULL;

/* cache the size of local_shares so that it doesn't need to be calculated
 * everytime OpenFT wants it */
static unsigned long local_files = 0;
static double        local_size  = 0.0; /* as MB */

/* set to TRUE anytime any operation is currently being executed on the
 * shares file
 * NOTE: this is used to block concurrent access */
static int           indexing = FALSE;

/*****************************************************************************/

static timer_id sync_timer_id = 0;

/* hide files with a leading . (UNIX hidden files) */
#define HIDE_DOT_FILES \
	config_get_int (gift_conf, "sharing/hide_dot_files=1")

/* share files as they are finished downloading */
#define SHARE_COMPLETED \
	config_get_int (gift_conf, "sharing/share_completed=1")

/* follow symbolic links */
#define FOLLOW_SYMLINKS \
	config_get_int (gift_conf, "sharing/follow_symlinks=1")

/*****************************************************************************/

static int free_share_root (struct _share_root *sr, void *udata)
{
	free (sr->path);
	free (sr);

	return TRUE;
}

static void destroy_share_root (List *sroot)
{
	if (!sroot)
		return;

	/* TODO -- shouldnt we be freeing sroot->data as well? */
	list_foreach_remove (sroot, (ListForeachFunc) free_share_root, NULL);
}

static void add_share_root (List **sroot, char *root, int recurse)
{
	struct _share_root *sr;
	char *ptr;

	if (!root)
		return;

	/* make sure there are no trailing /'s */
	for (ptr = root + strlen (root) - 1;
	     ptr >= root && *ptr == '/'; ptr--);

	/* if no / was found, ptr[1] will equal 0 anyway */
	ptr[1] = 0;

	if (*root != '/')
	{
		GIFT_WARN (("invalid sharing root '%s': relative path", root));
		return;
	}

	if (!(sr = malloc (sizeof (struct _share_root))))
		return;

	sr->path    = STRDUP (root);
	sr->recurse = recurse;

	/* add the supplied path in host order.  see file_host_path for
	 * more details */
	*sroot = list_append (*sroot, sr);
}

static List *build_share_root (void)
{
	List *sroot = NULL;
	char *sharing_root, *sharing_root0;
	char *root;

	if (!(sharing_root = config_get_str (gift_conf, "sharing/root")))
	{
		GIFT_WARN (("no sharing root found!!  share you bastard!"));
		return NULL;
	}

	sharing_root0 = sharing_root = STRDUP (sharing_root);

	while ((root = string_sep (&sharing_root, ":")))
		add_share_root (&sroot, file_expand_path (root), TRUE);

	free (sharing_root0);

	if (SHARE_COMPLETED)
	{
		char *unix_path;

		unix_path = file_unix_path (COMPLETED_PATH (("")));
		add_share_root (&sroot, unix_path, FALSE);
		free (unix_path);
	}

	return sroot;
}

static char *calculate_root (List *sroot, char *filename)
{
	struct _share_root *sr;
	List *ptr;

	if (!sroot || !filename)
		return NULL;

	for (ptr = sroot; ptr; ptr = list_next (ptr))
	{
		sr = ptr->data;

		/* found root */
		if (!strncmp (filename, sr->path, strlen (sr->path)))
			return sr->path;
	}

	return NULL;
}

/*****************************************************************************/
/* FILESHARE CREATION HELPERS */

static size_t share_root_len (char *root)
{
	char      *ptr;
	size_t     len;

	if (!root)
		return 0;

	len = ((ptr = strrchr (root, '/'))) ? ptr - root : 0;

	return len;
}

/* create a FileShare from a file on disk
 * NOTE: root and path are in HOST file order */
static FileShare *create_share_disk (char *host_root, char *host_path)
{
	FileShare     *file;
	char          *unix_root;
	char          *unix_path;
	struct stat    st;

	/* acquire the necessary information */
	if (!file_stat (host_path, &st) || st.st_size == 0)
		return NULL;

	/* convert to UNIX file order
	 * NOTE: this is done first simply because md5_checksum is the most
	 * expensive operation here and should be last in case an error occurs
	 * elsewhere */
	unix_root = file_unix_path (host_root);
	if (!(unix_path = file_unix_path (host_path)))
	{
		free (unix_root);
		return NULL;
	}

	file = share_new_ex (NULL, unix_root, share_root_len (unix_root),
	                     unix_path, NULL, st.st_size, st.st_mtime);

	free (unix_root);
	free (unix_path);

	if (!file)
		return NULL;

	if (!share_run_hash (file))
	{
		share_free (file);
		return NULL;
	}

	return file;
}

/*****************************************************************************/

static int cmp_list_entry (FileShare *file, char *path)
{
	if (!SHARE_DATA(file))
		return -1;

	if (!strcmp (SHARE_DATA(file)->path, path))
		return 0;

	return -1;
}

static int cmp_hash_entry (ds_data_t *key, ds_data_t *value, char *path)
{
	FileShare *file = value->data;

	return (cmp_list_entry (file, path) == 0);
}

static FileShare *find_share_path (Dataset **shares, List **duplicates,
                                   char *path)
{
	FileShare *file = NULL;

	if (!path)
		return NULL;

	if (shares)
		file = dataset_find (*shares, DS_FIND(cmp_hash_entry), path);

	if (!file && duplicates)
	{
		List *link;

		link = list_find_custom (*duplicates, path,
		                         (CompareFunc)cmp_list_entry);
		file = list_nth_data (link, 0);
	}

	return file;
}

static FileShare *find_share_hash (Dataset **shares, List **duplicates,
                                   Hash *hash)
{
	FileShare *file = NULL;

	if (!hash)
		return NULL;

	if (shares)
		file = dataset_lookup (*shares, hash->data, hash->len);

#if 0
	if (!file && duplicates)
		/* TODO */;
#endif

	return file;
}

/*****************************************************************************/

static void add_dataset_share (Dataset **dataset, FileShare *file)
{
	Hash *hash;

	if (!dataset)
		return;

	if (!(hash = share_get_hash (file, NULL)))
		return;

	if (!(*dataset))
		*dataset = dataset_new (DATASET_HASH);

	dataset_insert (dataset, hash->data, hash->len, file, 0);
}

static void add_list_share (List **list, FileShare *file)
{
	if (!list)
		return;

	*list = list_prepend (*list, file);
}

/* adds the share to the appropriate structure
 * NOTE: return value is whether or not it was successfully added to
 * shares */
static int add_share (Dataset **shares, List **duplicates, FileShare *file)
{
	FileShare *cache = NULL;
	Hash      *hash;

	if (!share_complete (file))
		return FALSE;

	/*
	 * This may return null, if the file only has currently unregistered
	 * hashes. If so, add it to the list of duplicates temporarily, in
	 * hopes of a registered algorithm coming along soon.
	 */
	hash = share_get_hash (file, NULL);

	if (hash && !(cache = find_share_hash (shares, NULL, hash)))
		add_dataset_share (shares, file);
	else
		add_list_share (duplicates, file);

	return (cache == NULL);
}

/*****************************************************************************/

static void remove_dataset_share (Dataset **dataset, FileShare *file)
{
	Hash *hash;

	if (!dataset)
		return;

	if (!(hash = share_get_hash (file, NULL)))
		return;

	dataset_remove (*dataset, hash->data, hash->len);
}

static void remove_list_share (List **duplicates, FileShare *file)
{
	if (!duplicates)
		return;

	*duplicates = list_remove (*duplicates, file);
}

static int remove_share (Dataset **shares, List **duplicates, FileShare *file)
{
	remove_dataset_share (shares, file);
	remove_list_share (duplicates, file);
	share_unref (file);
	return TRUE;
}

/*****************************************************************************/

static BOOL handle_file (SDB *db, Dataset **shares, List **duplicates,
                        char *host_root, char *host_path, off_t size,
                        time_t mtime)
{
	FileShare *file  = NULL;
	FileShare *cache = NULL;
	FileShare *cache_s = NULL;
	FileShare *cache_d = NULL;
	char      *unix_path;
	BOOL       usable;
	BOOL       ret;

	if (!(unix_path = file_unix_path (host_path)))
		return FALSE;

	if (!(cache_s = find_share_path (shares, NULL, unix_path)) &&
	    !(cache_d = find_share_path (NULL, duplicates, unix_path)))
	{
		if (!(file = create_share_disk (host_root, host_path)))
		{
			free (unix_path);
			return FALSE;
		}
	}
	else
	{
		cache = (cache_s) ? cache_s : cache_d;

		/* this path has been cached previously, if the mtimes differ remove
		 * it from the list and recalculate it as if it's a new file */
		if (SHARE_DATA(cache)->mtime != mtime)
		{
			remove_share (shares, duplicates, cache);

			if (!(file = create_share_disk (host_root, host_path)))
			{
				free (unix_path);
				return FALSE;
			}
		}
	}

	if (!file)
	{
		file   = cache;
		usable = BOOL_EXPR (cache_s != NULL);
	}
	else
	{
		/* if we did not successfully add to shares then this is a duplicate
		 * share (diff path, same hash) */
		usable = add_share (shares, duplicates, file);
	}

	/* make sure we have all algorithms accounted for
	 * NOTE: this is harmless if there is a value for all necessary
	 * types */
	share_run_hash (file);

	/* no meta data, try again */
	if (!SHARE_DATA(file)->meta)
		meta_run (file);

	if (!(ret = sdb_write (db, file)))
	{
#if 0
		/* sigh... */
		GIFT_FATAL (("FIXME: share database write failed, no error handling "
		             "defined"));
#endif
	}

	free (unix_path);

	/* this isn't really treated as any kind of error condition,
	 * unfortunately... */
	return ret;
}

/*
 * Inform the interface protocol of a change in progress via the
 * socketpair provided from parent_child_proc.
 */
static void update_interface (SubprocessData *subproc, int total)
{
	char str[64];

	/* started directly, without platform_child (see --index-only) */
	if (!subproc)
		return;

	/* write the total as a string with the nul character for parsing ease on
	 * the parents side */
	snprintf (str, sizeof (str), "%i", total);
	platform_child_sendmsg (subproc, str, strlen (str) + 1);
}

/* returns the total files read plus the old passed as arg 'total' */
static int path_traverse (SDB *db, int total, SubprocessData *subproc,
                          Dataset **shares, List **duplicates,
                          Dataset **dir_inodes, char *root, char *path,
                          int recurse)
{
	DIR           *dir;
	struct dirent *d;
	struct stat    st;
	size_t         path_len = 0;
	char           newpath[PATH_MAX];
	char          *path_fmt;
	int            retval;

	if (!path)
		return total;

	GIFT_TRACE (("descending %s...", path));

	/* calculate the length here for optimization purposes */
	path_len = strlen (path);

	if (!(dir = file_opendir (path)))
	{
		GIFT_WARN (("cannot open dir %s: %s", path, GIFT_STRERROR()));
		return total;
	}

#ifndef WIN32
	path_fmt = "%s/%s";
#else
	path_fmt = "%s\\%s";
#endif

	while ((d = file_readdir (dir)))
	{
		if (!d->d_name || !d->d_name[0])
			continue;

		if (HIDE_DOT_FILES)
		{
			if (d->d_name[0] == '.')
				continue;
		}

		/* ignore '.' and '..' */
		if (!strcmp (d->d_name, ".") || !strcmp (d->d_name, ".."))
			continue;

		snprintf (newpath, sizeof (newpath) - 1, path_fmt, path, d->d_name);

#ifndef WIN32
		if (!FOLLOW_SYMLINKS)
			retval = lstat (newpath, &st);
		else
#endif /* !WIN32 */
			retval = stat (newpath, &st);

		if (retval == -1)
		{
			GIFT_TRACE (("cannot stat %s: %s", newpath, GIFT_STRERROR ()));
			continue;
		}

		if (S_ISDIR (st.st_mode) && recurse)
		{
#ifdef TRACK_DIR_INODES
			struct _dir_inode di;

			memset (&di, 0, sizeof (di));
			di.inode = st.st_ino;
			di.dev = st.st_dev;

			/* refuse to descend this dir, inode matches */
			if (dataset_lookup (*dir_inodes, &di, sizeof (di)))
			{
				GIFT_TRACE (("ignoring %s: already traversed this inode",
				             newpath));
				continue;
			}

			dataset_insert (dir_inodes, &di, sizeof (di), "dir_inode", 0);
#endif /* TRACK_DUP_INODES */

			total = path_traverse (db, total, subproc, shares, duplicates,
			                       dir_inodes, root, newpath, recurse);
		}
		else if (S_ISREG (st.st_mode))
		{
			total += handle_file (db, shares, duplicates, root, newpath,
			                      st.st_size, st.st_mtime);
			update_interface (subproc, total);
		}
	}

	file_closedir (dir);

	return total;
}

static int share_build_index (Dataset **shares, List **duplicates, List *sroot,
                              unsigned long *r_files, double *r_size)
{
	SDB          *db  = NULL;
	FileShare    *file;
	unsigned long l_files = 0;
	double        l_size  = 0.0;

	GIFT_TRACEFN;

	if (!(db = sdb_open (gift_conf_path ("shares"), "rb")))
		GIFT_TRACE (("*** creating new shares file"));
	else
	{
		while ((file = sdb_read (db)))
		{
			if (!add_share (shares, duplicates, file))
				continue;

			l_files++;
			l_size += ((float) file->size / 1024.0 / 1024.0);
		}

		sdb_close (db);
	}

	if (r_files)
		*r_files = l_files;

	if (r_size)
		*r_size = l_size;

	return TRUE;
}

/*****************************************************************************/

static int clear_list_entry (FileShare *file, void *udata)
{
	share_unref (file);
	return TRUE;
}

static int clear_dataset_entry (ds_data_t *key, ds_data_t *value, void *udata)
{
	clear_list_entry (value->data, udata);

	return DS_CONTINUE | DS_REMOVE;
}

static void share_clear (Dataset **shares, List **duplicates)
{
	if (shares && *shares)
	{
		dataset_foreach_ex (*shares, DS_FOREACH_EX(clear_dataset_entry), NULL);
		dataset_clear (*shares);
		*shares = NULL;
	}

	if (duplicates && *duplicates)
	{
		*duplicates =
		    list_foreach_remove (*duplicates,
		                         (ListForeachFunc)clear_list_entry, NULL);
	}
}

/*****************************************************************************/

static CHILD_FUNC (child_update_index)
{
	share_write_index (subproc);

	/* parent gets called if we write to fd or close it by exiting */
	return TRUE;
}

static PARENT_FUNC (parent_update_index)
{
	share_read_index ();

	/* it's now safe to update again */
	indexing = FALSE;

	return FALSE;
}

/* update index and report progress to the interface protocol */
static PARENT_FUNC (parent_update_index_report)
{
	IFEvent    *event;
	if_event_id id = *(if_event_id *)udata;
	int         ret;

	event = if_event_lookup (id);

	/* do not abort on legit writes */
	if (platform_child_recvmsg (subproc) > 0)
	{
		/* sanity check the incoming data to make sure we wrote properly */
		assert (subproc->data[subproc->len - 1] == 0);

		if (event)
			if_share_action (event, "sync", subproc->data);

		return TRUE;
	}

	/* we are finished syncing, update everything */
	ret = parent_update_index (subproc, udata);

	if (event)
	{
		if_share_action (event, "sync", "Done");
		if_share_finish (event);
	}

	free (udata);

	return ret;
}

static int update_index (ChildFunc cfunc, ParentFunc pfunc, void *udata)
{
	if (indexing)
	{
		GIFT_WARN (("previous update request is still pending"));
		return FALSE;
	}

	indexing = TRUE;

	if (!(platform_child (cfunc, pfunc, udata)))
	{
		GIFT_TRACE (("UNABLE TO FORK!"));
		return FALSE;
	}

	return TRUE;
}

void share_update_index (void)
{
	IFEvent *event;
	int      ret;
	void    *udata;

	GIFT_WARN (("updating index..."));

	if (!(event = if_share_new (NULL, 0)))
		return;

	if (sync_timer_id)
		timer_reset (sync_timer_id);

	if_share_action (event, "sync", "0");
	udata = gift_memdup (&event->id, sizeof (event->id));

	ret = update_index ((ChildFunc)child_update_index,
	                    (ParentFunc)parent_update_index_report, udata);

	/* update_index failed to start the child processes (may already be
	 * indexing), so cleanup the memory allocated */
	if (!ret)
	{
		if_share_finish (event);
		free (udata);
	}
}

void share_init_timer (int timeout)
{
	assert (timeout >= 0);

	if (timeout == 0)
		return;

	sync_timer_id = timer_add ((time_t)(timeout * SECONDS),
	                           (TimerCallback)share_update_index, NULL);
}

/*****************************************************************************/

static CHILD_FUNC (child_add_entry)
{
	SDB  *db;
	List *sroot;
	char *shares, shares_buf[PATH_MAX];
	char *host_root, *unix_root;
	char *host_path, *unix_path;

	shares = gift_conf_path_r (shares_buf, sizeof (shares_buf), "shares");
	assert (shares != NULL);

	if (!(db = sdb_open (shares, "ab")))
	{
		if (!(db = sdb_create (shares, "ab")))
			return FALSE;
	}

	sroot = build_share_root ();

	/* eww, this is very messy */
	host_path = udata;
	unix_path = file_unix_path (host_path);

	unix_root = calculate_root (sroot, unix_path);
	host_root = file_host_path (unix_root);

	destroy_share_root (sroot);

	handle_file (db, NULL, NULL, host_root, host_path, 0, 0);

	free (unix_path);
	free (host_root);

	sdb_close (db);

	return FALSE;
}

static PARENT_FUNC (parent_update_entry)
{
	int ret;

	ret = parent_update_index (subproc, udata);
	free (udata);

	return ret;
}

/* god this function is a hack */
void share_add_entry (char *host_path)
{
	update_index ((ChildFunc)child_add_entry,
	              (ParentFunc)parent_update_entry, STRDUP (host_path));
}

static CHILD_FUNC (child_remove_entry)
{
	/* TODO */
	return child_update_index (subproc, udata);
}

void share_remove_entry (char *host_path)
{
	update_index ((ChildFunc)child_remove_entry,
	              (ParentFunc)parent_update_entry, STRDUP (host_path));
}

/* refreshes the share entry if it is stale.  returns FALSE if the entry
 * doesn't need to be updated
 * NOTE: this may be forked, you cannot rely on an immediate update! */
int share_update_entry (FileShare *file)
{
	char       *host_path;
	struct stat st;
	int         ret;

	if (!file || !SHARE_DATA(file))
		return FALSE;

	host_path = file_host_path (SHARE_DATA(file)->path);

	ret = file_stat (host_path, &st);

	/* this entry needs to be updated if the mtime on disk and in
	 * the structure do not match (or if the file no longer exists at all) */
	if (!ret || SHARE_DATA(file)->mtime != st.st_mtime ||
	    file->size != st.st_size)
	{
		free (host_path);
		share_update_index ();
		return TRUE;
	}

	free (host_path);
	return FALSE;
}

/*****************************************************************************/

/*
 * Output the ~/.giFT/shares file.
 *
 * NOTE:
 * Uses only local memory as it is expected to be run from a child
 * process.
 */
void share_write_index (SubprocessData *subproc)
{
	SDB       *db;
	List      *ptr;
	List      *sroot;
	Dataset   *shares     = NULL;
	List      *duplicates = NULL;
	Dataset   *dir_inodes = NULL;
	char      *tmp,    tmp_buf[PATH_MAX];
	char      *tmp_to, tmp_to_buf[PATH_MAX];
	int        total = 0;

	GIFT_TRACEFN;

	sroot = build_share_root ();

	/* rebuild a data structure from the original file for use with the md5
	 * lookup in path_traverse */
	share_build_index (&shares, &duplicates, sroot, NULL, NULL);

	tmp = gift_conf_path_r (tmp_buf, sizeof (tmp_buf), "shares.tmp");
	assert (tmp != NULL);

	tmp_to = gift_conf_path_r (tmp_to_buf, sizeof (tmp_to_buf), "shares");
	assert (tmp_to != NULL);

	if (!(db = sdb_create (tmp, "wb+")))
		return;

#ifdef TRACK_DIR_INODES
	dir_inodes = dataset_new (DATASET_HASH);
#endif

	/*
	 * Loop through all sharing root paths.  This will optionally include
	 * the download completed directory (non-recursively).
	 */
	for (ptr = sroot; ptr; ptr = list_next (ptr))
	{
		struct _share_root *sr = ptr->data;
		char *root;

		/*
		 * We need to use the native host style path before we traverse here,
		 * otherwise, we're gonna be all fucked up with duplicated allocations
		 * from the conversion.
		 */
		if (!(root = file_host_path (sr->path)))
			continue;

		GIFT_TRACE (("descending root: %s...", root));
		total = path_traverse (db, total, subproc, &shares, &duplicates,
		                       &dir_inodes, root, root, sr->recurse);

		free (root);
	}

#ifdef TRACK_DIR_INODES
	dataset_clear (dir_inodes);
#endif

	sdb_close (db);

	/* TODO: errgh, we dont track errors writing shares_tmp!!! */
	file_mv (tmp, tmp_to);

	destroy_share_root (sroot);

	share_clear (&shares, &duplicates);
}

/*****************************************************************************/

static void emit_share_new (ds_data_t *key, ds_data_t *value, FileShare *file)
{
	Protocol *p = value->data;
	void     *data;

	/* make sure we can't have any data left over, this should probably
	 * be an assert */
	if ((data = share_get_udata (file, p->name)))
	{
		p->share_free (p, file, data);
		share_set_udata (file, p->name, NULL);
	}

	/*
	 * Ask the protocol to create new arbitrary data (if it needs to be
	 * created) and then associate it with this structure.  If no data
	 * is provided we do not consider it an error.
	 */
	if ((data = p->share_new (p, file)))
		share_set_udata (file, p->name, data);
}

static void emit_share_new_all (ds_data_t *key, ds_data_t *value, void *udata)
{
	/* we need to emit share_new for all protocols, as well */
	plugin_foreach (DS_FOREACH(emit_share_new), value->data);
}

static void emit_share_free (ds_data_t *key, ds_data_t *value, FileShare *file)
{
	Protocol *p = value->data;
	void     *data;

	/*
	 * Delete any arbitrary data the protocol wanted associated here.  We
	 * must also remove it from this structure as we may not necessarily
	 * want to assume we are going to free the whole structure immediately
	 * after this.
	 */
	if ((data = share_get_udata (file, p->name)))
	{
		p->share_free (p, file, data);
		share_set_udata (file, p->name, NULL);
	}
}

static void emit_share_free_all (ds_data_t *key, ds_data_t *value, void *udata)
{
	plugin_foreach (DS_FOREACH(emit_share_free), value->data);
}

static void emit_share_sync (ds_data_t *key, ds_data_t *value, int *begin)
{
	Protocol *p = value->data;

	/*
	 * Let the protocol know that we are entering the sync stage so that
	 * it may initialize whatever it needs to.  This same function is used
	 * (when *begin is FALSE) to terminate the sync.  Merely a convenience
	 * for protocols.
	 */
	p->share_sync (p, *begin);
}

/*****************************************************************************/

/*
 * Attempts to determine if data expressed by a_node exists in both dataset a
 * and b.  Returns TRUE if that is the case.
 *
 * This is done so that we can determine the practical differences between
 * the old set of files shared and the new.
 */
static int eq_filesets (ds_data_t *key, ds_data_t *value, Dataset *b)
{
	FileShare *a_file = value->data;
	FileShare *b_file;
	ds_data_t *b_key;

	/*
	 * The file doesn't exist in the new set at all, therefore it must've
	 * been removed and we can safely return FALSE without any further
	 * checks.
	 */
	if (!(b_key = dataset_lookup_ex (b, key)))
		return FALSE;

	b_file = b_key->data;

	/*
	 * If the mtime's of the files differ, giFT would've recalculated
	 * new data and these cannot be considered the same any longer.
	 */
	return (SHARE_DATA(a_file)->mtime == SHARE_DATA(b_file)->mtime);
}

static void emit_share_remove (ds_data_t *key, ds_data_t *value,
                               FileShare *file)
{
	Protocol *p = value->data;

	/*
	 * Inform the protocol that we are no longer sharing the specified
	 * file.
	 */
	p->share_remove (p, file, share_get_udata (file, p->name));
}

static void emit_share_remove_all (ds_data_t *key, ds_data_t *value,
                                   Dataset *latter)
{
	if (!eq_filesets (key, value, latter))
		plugin_foreach (DS_FOREACH(emit_share_remove), value->data);
}

static void emit_share_add (ds_data_t *key, ds_data_t *value, FileShare *file)
{
	Protocol *p = value->data;

	/*
	 * We have a new share for the protocol.  The protocol-specific data
	 * should have already been added, so this function isn't expected
	 * to do that again.
	 */
	p->share_add (p, file, share_get_udata (file, p->name));
}

static int emit_share_add_all (ds_data_t *key, ds_data_t *value,
                               Dataset *former)
{
	if (!eq_filesets (key, value, former))
		plugin_foreach (DS_FOREACH(emit_share_add), value->data);

	return FALSE;
}

/*****************************************************************************/

static void handle_protocols (Dataset *former, Dataset *latter)
{
	int begin;

	/*
	 * Loop through all shares in the "new" set, emitting a share_new call
	 * to each protocol for each file in this set.
	 */
	dataset_foreach (latter, DS_FOREACH(emit_share_new_all), NULL);

	/*
	 * Now we must inform the protocol of all the modifications that occurred
	 * to our local repository which should be reflected on whichever
	 * network is implemented.
	 */
	begin = TRUE; plugin_foreach (DS_FOREACH(emit_share_sync), &begin);
	dataset_foreach (former, DS_FOREACH(emit_share_remove_all), latter);
	dataset_foreach (latter, DS_FOREACH(emit_share_add_all), former);
	begin = FALSE; plugin_foreach (DS_FOREACH(emit_share_sync), &begin);

	/*
	 * We must now inform the protocol of all the structures which will soon
	 * be deleted.
	 */
	dataset_foreach (former, DS_FOREACH(emit_share_free_all), NULL);
}

/*****************************************************************************/

/*
 * Read ~/.giFT/shares into a data structure
 *
 * NOTE:
 * Affects program globals, expected to be executed from a parent
 * process
 */
Dataset *share_read_index (void)
{
	List         *sroot;
	Dataset      *shares     = NULL;
	List         *duplicates = NULL;
	unsigned long files = 0;
	double        size  = 0.0;

	GIFT_TRACEFN;

	/* read from the file */
	sroot = build_share_root ();
	share_build_index (&shares, &duplicates, sroot, &files, &size);
	destroy_share_root (sroot);

	handle_protocols (local_shares, shares);

	/* clear any shares that existed before */
	share_clear (&local_shares, &local_duplicates);
	local_files = 0;
	local_size  = 0.0;

	GIFT_TRACE (("total shares: %lu (%.02fMB)", files, size));

	/* reset local variables */
	local_shares     = shares;
	local_duplicates = duplicates;
	local_files      = files;
	local_size       = size;

	return shares;
}

/*****************************************************************************/

void share_clear_index (void)
{
	share_clear (&local_shares, &local_duplicates);
}

/*****************************************************************************/

Dataset *share_index (unsigned long *files, double *size)
{
	if (files)
		*files = local_files;
	if (size)
		*size = local_size;

	return local_shares;
}

/*****************************************************************************/

static int index_sort_cmp (FileShare *a, FileShare *b)
{
	return strcmp (SHARE_DATA(a)->path, SHARE_DATA(b)->path);
}

/* WARNING!  This is slow as hell! */
List *share_index_sorted (void)
{
	List *index;

	index = list_sort (dataset_flatten (local_shares),
	                   (CompareFunc)index_sort_cmp);

	return index;
}

/*****************************************************************************/

void share_foreach (DatasetForeachExFn foreach_fn, void *data)
{
	if (!local_shares)
		return;

	dataset_foreach_ex (local_shares, foreach_fn, data);
}

/*****************************************************************************/

static int find_file (ds_data_t *key, ds_data_t *value, char *filename)
{
	FileShare *file = value->data;
	char      *hpath;

	if (!filename || !(hpath = share_get_hpath (file)))
		return FALSE;

	return (strcmp (filename, hpath) == 0);
}

/* this function accepts a hidden path (/file.mp3) and returns the full
 * FileShare structure for it (the below function may be more appropriate for
 * you) */
FileShare *share_find_file (char *filename)
{
	FileShare *file;

	if (!filename)
		return NULL;

	file = dataset_find (share_index (NULL, NULL),
						 DS_FIND(find_file), filename);

	return file;
}

/*****************************************************************************/

int share_indexing (void)
{
	return indexing;
}
