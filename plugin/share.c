/*
 * $Id: share.c,v 1.7 2003/07/23 23:51:59 hipnod Exp $
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

#include "lib/libgift.h"

#include "lib/dataset.h"

#include "protocol.h"

#include "lib/mime.h"

#include "share.h"

/*****************************************************************************/

/* this is used by share_finish, but not meant to be used externally */
static unsigned int share_clear_udata (Share *share);

/*****************************************************************************/

BOOL share_init (Share *share, const char *path)
{
	assert (share != NULL);

	memset (share, 0, sizeof (Share));

	share_set_path (share, path);
	share_ref (share);

	return TRUE;
}

void share_finish (Share *share)
{
	unsigned int n;

	free (share->path);
	free (share->root);

	share_clear_hash (share);
	share_clear_meta (share);

	/* when clearing the udata dataset make sure that there are no lingering
	 * entries from plugins that obviously need to be removed gracefully to
	 * prevent leaks */
	if ((n = share_clear_udata (share)) > 0)
	{
#if 0
		GIFT_WARN (("possible leak: %i lingering udata entries removed", n));
#endif
	}
}

Share *share_new (const char *path)
{
	Share *share;

	/* share_init will zero for us */
	if (!(share = malloc (sizeof (Share))))
		return NULL;

	share_init (share, path);

	return share;
}

/* legacy interface, please do not use... */
Share *share_new_ex (Protocol *p, const char *root, size_t root_len,
                     const char *path, const char *mime, off_t size,
                     time_t mtime)
{
	Share *share;

	if (!(share = share_new (path)))
		return NULL;

	/* high-level memory managing API... */
	share_set_root (share, root, root_len);
	share_set_mime (share, mime);

	/* raw access... */
	share->p     = p;
	share->size  = size;
	share->mtime = mtime;

	return share;
}

void share_free (Share *share)
{
	if (!share)
		return;

	share_finish (share);
	free (share);
}

/*****************************************************************************/

static unsigned int change_ref (Share *share, int change)
{
	if (!share)
		return 0;

	if (change < 0)
		assert (share->ref > 0);

	share->ref += change;

	return share->ref;
}

unsigned int share_ref (Share *share)
{
	return change_ref (share, 1);
}

unsigned int share_unref (Share *share)
{
	unsigned int ref;

	if (!(ref = change_ref (share, -1)))
		share_free (share);

	return ref;
}

/*****************************************************************************/

/* this code was copied back from the "old" share_file.c, and is left just as
 * inefficient until i can confirm that optimizations will not have any
 * unforeseen side effects... */
char *share_get_hpath (Share *share)
{
	size_t root_len;

	if (!share)
		return NULL;

	if (!share->root)
		return NULL;

	root_len = strlen (share->root);

	return (share->path + root_len);
}

void share_set_path (Share *share, const char *path)
{
	if (!share)
		return;

	free (share->path);
	share->path = STRDUP (path);
}

void share_set_root (Share *share, const char *root, size_t len)
{
	if (!share)
		return;

	free (share->root);

	if (!root || len > 0)
		share->root = gift_strndup (root, len);
	else
		share->root = gift_strdup ("");
}

void share_set_mime (Share *share, const char *mime)
{
	if (!share)
		return;

	if (mime)
		share->mime = mime_type_lookup (mime);
	else
		share->mime = NULL;
}

/*****************************************************************************/

void share_set_meta (Share *share, const char *key, const char *value)
{
	char *keylow;

	if (!share || !key)
		return;

	/* lowercase the key for consistency, although it would be best if we
	 * could specify case insensitivity through the dataset :( */
	if (!(keylow = string_lower (STRDUP(key))))
		return;

	/* remove any data already present here */
	dataset_removestr (share->meta, keylow);

	/* add the data back */
	if (value)
		dataset_insertstr (&share->meta, keylow, (char *)value);

	free (keylow);
}

char *share_get_meta (Share *share, const char *key)
{
	if (!share || !key)
		return NULL;

	return dataset_lookupstr (share->meta, (char *)key);
}

void share_clear_meta (Share *share)
{
	if (!share)
		return;

	dataset_clear (share->meta);
	share->meta = NULL;
}

void share_foreach_meta (Share *share, DatasetForeachFn func, void *udata)
{
	dataset_foreach (share->meta, func, udata);
}

/*****************************************************************************/

void share_set_udata (Share *share, const char *proto, void *udata)
{
	ds_data_t key;
	ds_data_t data;

	if (!share || !proto)
		return;

	ds_data_init (&key, (void *)proto, STRLEN_0(proto), DS_NOCOPY);

	if (!udata)
		dataset_remove_ex (share->udata, &key);
	else
	{
		ds_data_init (&data, udata, 0, DS_NOCOPY);
		dataset_insert_ex (&share->udata, &key, &data);
	}
}

void *share_get_udata (Share *share, const char *proto)
{
	ds_data_t  key;
	ds_data_t *data;

	if (!share || !proto)
		return NULL;

	ds_data_init (&key, (void *)proto, STRLEN_0(proto), DS_NOCOPY);

	if (!(data = dataset_lookup_ex (share->udata, &key)))
		return NULL;

	return data->data;
}

static unsigned int share_clear_udata (Share *share)
{
	unsigned int cleared;

	if (!share)
		return 0;

	cleared = dataset_length (share->udata);
	dataset_clear (share->udata);
	share->udata = NULL;

	return cleared;
}

/*****************************************************************************/

/* copied verbatim from the old share_file.c, it appears to have obvious flow
 * bugs, but i wont mess with it now... */
BOOL share_complete (Share *share)
{
	if (!share)
		return FALSE;

	if (share->path && share->size)
	{
		/* absolute path only */
		if (share->path[0] != '/')
			return FALSE;
	}

	return TRUE;
}
