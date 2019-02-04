/*
 * $Id: share.h,v 1.6 2003/10/16 18:50:55 jasta Exp $
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

#ifndef __SHARE_H
#define __SHARE_H

/*****************************************************************************/

/**
 * @file share.h
 *
 * @brief Interface for referencing complex elements of a shared file.
 *
 * Please note that is an integral part of the giFT communication API for
 * protocol plugins.  You are encouraged to use this internally wherever
 * appropriate.
 *
 * Also note that there is a supplementary share_hash.[ch] that provides the
 * rest of the API.  Eventually it will merged in here, but as of now it is
 * better left independent.
 */

/*****************************************************************************/

/* duplicate here because this file may be used without protocol.h */
#ifndef LIBGIFTPROTO_EXPORT
# define LIBGIFTPROTO_EXPORT /* nothing */
#endif

/*****************************************************************************/

struct protocol;

/*****************************************************************************/

#include "share_hash.h"

/*****************************************************************************/

/**
 * Structure used to describe a complete shared file.  This is used by giFT
 * and plugins internally, and is required for some of the plugin
 * communication.  Learn it, use it, love it.
 */
struct file_share
{
	/**
	 * @name Disk
	 */
	char        *path;                 /**< Local path in UNIX form */
	char        *root;                 /**< Original sharing/root that
	                                    *   caught this file, such as
	                                    *   "/data/movies" */
	char        *mime;                 /**< Pointer to the mime lookup
	                                    *   table in src/mime.c */
	time_t       mtime;                /**< Modification time as reported by
	                                    *   stat(2) */
	off_t        size;                 /**< Size in bytes as reported by
	                                    *   stat(2) */

	/**
	 * @name Object
	 */
	unsigned int ref;                  /**< Current mortality count */

	/**
	 * @name Meta/Supplementary
	 */
	Dataset     *meta;                 /**< ASCII NUL-terminated meta data */
	Dataset     *hash;                 /**< AsciiType -> BinaryHash */
	Dataset     *udata;                /**< Associated user data set */

	/**
	 * @name Deprecated
	 */
	struct protocol *p;                /**< Convenience member to determine
	                                    *   which domain the object was
	                                    *   created in */
};

typedef struct file_share Share;

/*****************************************************************************/

/* backwards compatibility should be maintained for now so that we can
 * easily identify old interfaces */
#define SHARE_BACKCOMPAT

#ifdef SHARE_BACKCOMPAT
typedef struct file_share FileShare;
#define SHARE_DATA(share) (share)
#endif /* SHARE_BACKCOMPAT */

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Similar to the ::share_new constructor, except that new memory is not
 * allocated.  Instead, a pre-allocated storage address is supplied.
 */
LIBGIFTPROTO_EXPORT
  BOOL share_init (Share *share, const char *path);

/**
 * Corresponding to ::share_init, but will destroy any internally owned
 * memory from the storage location provided.  The calling argument will not
 * be free'd.
 */
LIBGIFTPROTO_EXPORT
  void share_finish (Share *share);

/*****************************************************************************/

/**
 * Construct a new shared file object.  This is intended to be used both by
 * giFT for each local file shared as well as by protocol plugins.  The
 * protocol communication API may require usage of this structure.
 *
 * @param path  Full local path in UNIX form.  The location provided will be
 *              copied into the Share object.
 */
LIBGIFTPROTO_EXPORT
  Share *share_new (const char *path);

/**
 * Backwards compatible interface that matches the old ::share_new.  This is
 * done as a convenience to current protocol plugins out there and will
 * hopefully be phased out with time.
 */
LIBGIFTPROTO_EXPORT
  Share *share_new_ex (struct protocol *p, const char *root, size_t root_len,
                       const char *path, const char *mime, off_t size,
                       time_t mtime);

/**
 * Destroy an allocated Share object and all its internally managed members
 * as with ::share_init.  The only difference between the two is that this
 * function will free the calling argument.
 */
LIBGIFTPROTO_EXPORT
  void share_free (Share *share);

/*****************************************************************************/

/**
 * Increment the mortality/reference count for the supplied object.  Please
 * note that the initial reference count is 1 when constructed, and will be
 * free'd automatically using ::share_free when the reference reaches 0 from
 * ::share_unref.
 *
 * @return The reference post-incrementation.
 */
LIBGIFTPROTO_EXPORT
  unsigned int share_ref (Share *share);

/**
 * Decrement the mortality/reference count for the supplied object.  See
 * ::share_ref for more information.  Please note that direct calls to
 * ::share_free will not obey reference counting, only this function will.
 *
 * @return The reference post-decrementation.
 */
LIBGIFTPROTO_EXPORT
  unsigned int share_unref (Share *share);

/*****************************************************************************/

/**
 * Access the "hidden" path from the object that is to be sent out to remote
 * peers when responding to searches (whichever method is used to do this by
 * the protocol plugin).  When requests come back for a new upload, they will
 * be required to be in this form.
 *
 * Please note that this routine will not function properly without a path
 * and a root value set.  See ::share_set_path, and ::share_set_root for more
 * details.
 *
 * @return A pointer to an internally managed memory segment (NUL-terminated
 *         ASCII string) representing the hidden path.
 */
LIBGIFTPROTO_EXPORT
  char *share_get_hpath (Share *share);

/*****************************************************************************/

/**
 * Set the share objects path.  This is called by ::share_new, which also
 * further documents the functionality of this function.
 */
LIBGIFTPROTO_EXPORT
  void share_set_path (Share *share, const char *path);

/**
 * Copy len number of bytes of the supplied `root' parameter into an
 * internally allocated memory segment.  Special precuation is taken to avoid
 * setting share->root to NULL even when invalid or ambiguous parameters are
 * given here.
 */
LIBGIFTPROTO_EXPORT
  void share_set_root (Share *share, const char *root, size_t len);

/**
 * Attempts to map the supplied MIME type in an internally managed table of
 * MIME types and then assigns a pointer there.  If the MIME type has been
 * seen before, the memory address will be shared.
 */
LIBGIFTPROTO_EXPORT
  void share_set_mime (Share *share, const char *mime);

/*****************************************************************************/

/**
 * Set arbitrary meta data.  Please note that key will be copied and
 * lowercased.  A copy of value will also be made by the dataset and
 * internally managed.
 *
 * @param share
 * @param key
 * @param value  If NULL, removes the meta data at the key supplied.
 */
LIBGIFTPROTO_EXPORT
  void share_set_meta (Share *share, const char *key, const char *value);

/**
 * Access previously set meta data.
 */
LIBGIFTPROTO_EXPORT
  char *share_get_meta (Share *share, const char *key);

/**
 * Clear all previously set meta data entries.
 */
LIBGIFTPROTO_EXPORT
  void share_clear_meta (Share *share);

/**
 * Simple abstraction from the Share::meta member.
 */
LIBGIFTPROTO_EXPORT
  void share_foreach_meta (Share *share, DatasetForeachFn func, void *udata);

/*****************************************************************************/

/**
 * Attach arbitrary user-data for your plugin using the key described by
 * `proto'.
 *
 * @param share
 * @param proto  Complete protocol name, as Protocol::name would recognize.
 * @param udata  Arbitrary user-data to attach.  If NULL, the attach user
 *               data will be removed.
 */
LIBGIFTPROTO_EXPORT
  void share_set_udata (Share *share, const char *proto, void *udata);

/**
 * Access previously attached user-data.  This should be pretty
 * self-explanatory.
 */
LIBGIFTPROTO_EXPORT
  void *share_get_udata (Share *share, const char *proto);

/*****************************************************************************/

/**
 * Determines if the supplied share object is filled.  The current
 * determination is that a path must be set, and that it must begin with a
 * leading '/'.
 */
LIBGIFTPROTO_EXPORT
  BOOL share_complete (Share *share);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __SHARE_H */
