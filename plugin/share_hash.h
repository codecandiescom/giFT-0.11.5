/*
 * $Id: share_hash.h,v 1.9 2003/11/10 09:47:39 jasta Exp $
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

#ifndef __SHARE_HASH_H
#define __SHARE_HASH_H

/*****************************************************************************/

/**
 * @file share_hash.h
 *
 * @brief Manage hashing algorithms provided by protocol plugins and applied
 *        to locally shared files.
 *
 * There are really multiple interfaces defined here.  The supplementary
 * share interface should be moved into share.[ch] at some time.
 */

/*****************************************************************************/

struct protocol;                       /* plugin/protocol.h:Protocol */
struct file_share;                     /* plugin/share.h:Share */

/*****************************************************************************/

typedef unsigned char* (*HashFn)    (const char *path, size_t *len);
typedef char*          (*HashDspFn) (unsigned char *hash, size_t len);

/*****************************************************************************/

/**
 * Holds data received from the protocol plugin which describes how to
 * construct a hash from a given path.  Please note that the algofn and dspfn
 * members are of a type defined in plugin/protocol.h.  This is done so that
 * the protocol plugin can share the type definition.
 */
typedef struct
{
	unsigned char ref;                 /**< Referencing counting system */
	int           opt;                 /**< Options provided by the protocol */
	const char   *type;                /**< Hashing algorith name.  Such as,
	                                    *   MD5, SHA1, etc. */
	HashFn      algofn;
	HashDspFn   dspfn;
} HashAlgo;

/**
 * Data structure used to represent an individual hash from the protocol.
 */
typedef struct
{
	HashAlgo      *algo;               /**< Refers to the algorithm type
	                                    *   used to generate this hash */
	unsigned char *data;               /**< Actual hash data.  Must point to
	                                    *   dynamically allocated memory at
	                                    *   least `size' long */
	BOOL           copy;               /**< Should we free data? */
	size_t         len;                /**< Length of data */
} Hash;

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Raw interface to allocate a new hash object.  Please avoid using this
 * directly at all costs.
 *
 * @param algo
 * @param data  Pointer to the beginning of a hash at least `len' bytes
 *              long.  This data must be allocated, and will not be
 *              copied by this function call.
 * @param len   Length of `data'.
 * @param copy  If TRUE, Makes a copy of `data' before storage in the hash
 *              object.
 *
 * @return Dynamically allocated hash object.
 */
LIBGIFTPROTO_EXPORT
  Hash *hash_new (HashAlgo *algo, unsigned char *data, size_t len, BOOL copy);

/**
 * Destroy the result from ::hash_new.  Please note that this will free the
 * original data argument.
 */
LIBGIFTPROTO_EXPORT
  void hash_free (Hash *hash);

/**
 * Duplicate all entries in a hash object.  This includes the original data
 * argument.
 */
LIBGIFTPROTO_EXPORT
  Hash *hash_dup (Hash *hash);

/**
 * Calculate a new hash from the supplied algorithm and file path.  This is
 * used as a work-around to the structured share_hash API, which requires a
 * constructed FileShare to operate.
 */
LIBGIFTPROTO_EXPORT
  Hash *hash_calc (HashAlgo *algo, const char *path);

/**
 * Convert a calculated hash (see ::hash_calc) into a NUL-terminated ASCII
 * representation.
 *
 * @return Dynamically allocated string in the form TYPE:ASCII_DATA.
 */
LIBGIFTPROTO_EXPORT
  char *hash_dsp (Hash *hash);

/*****************************************************************************/

/**
 * Register a new hashing algorithm.  See the documentation available at
 * Protocol::hash_handler.
 *
 * @return Number of references to the supplied hashing algorithm or zero on
 *         error.
 */
LIBGIFTPROTO_EXPORT
  unsigned int hash_algo_register (struct protocol *p, const char *type,
                                   int opt, HashFn algofn, HashDspFn dspfn);

/**
 * Unregister a hashing algorithm.  This is used when a protocol is unloaded
 * so that we can gracefully update the internal data structure without
 * interfering with currently loaded protocols.
 *
 * @return Number of references remaining.  If zero, the algorithm has been
 *         completely removed and it's memory free'd.
 */
LIBGIFTPROTO_EXPORT
  unsigned int hash_algo_unregister (struct protocol *p, const char *type);

/**
 * Lookup a currently registered algorithm by name.
 */
LIBGIFTPROTO_EXPORT
  HashAlgo *hash_algo_lookup (const char *type);

/*****************************************************************************/

/**
 * Associate a predetermined hashed value with a share object.
 *
 * @param file  Share object.
 * @param type  Hashing algorithm's formal name as registered by the
 *              plugin.
 * @param data  Hash data.  Memory management within the share object is
 *              conditionally dependent on `copy'.
 * @param len   Length of `data'.
 * @param copy  Specifies whether or not a copy of the data parameter should
 *              be used for local storage and management within the
 *              share object.  See ::hash_new for more details.
 *
 * @return Boolean success or failure.  Successful completion of this
 *         operation is defined by the final placement of the specified
 *         hash parameter in the internal share object's hash, regardless
 *         of whether or not this required an overwrite of the previous
 *         data.  Use ::share_get_hash if you need to know if there is
 *         already an associated hash.
 *
 */
LIBGIFTPROTO_EXPORT
  BOOL share_set_hash (struct file_share *file, const char *type,
                       unsigned char *data, size_t len, BOOL copy);

/**
 * Access the previously set hash.  See ::share_hash_set.
 */
LIBGIFTPROTO_EXPORT
  Hash *share_get_hash (struct file_share *file, const char *type);

/**
 * Clear all hashes associated with the given file.  Management of the
 * internal hash memory is dependent on the parameters given to
 * ::share_set_hash.
 */
LIBGIFTPROTO_EXPORT
  void share_clear_hash (struct file_share *file);

/**
 * Iterate through all hashes currently associated with this file.
 * Internally uses ::dataset_foreach.
 */
LIBGIFTPROTO_EXPORT
  void share_foreach_hash (struct file_share *file, DatasetForeachFn func,
                           void *udata);

/**
 * Build a hash of the supplied file for all currently registered hashing
 * algorithms.  Keep in mind that only those primary or local algos will be
 * used here.  Secondary algorithms will be used mainly for data integrity.
 *
 * @return Number of hashes successfully created.
 */
LIBGIFTPROTO_EXPORT
  unsigned int share_run_hash (struct file_share *file);

/**
 * Return the ASCII representation of the hash described after lookup.  See
 * ::share_get_hash.
 *
 * @note This operation must query the protocol plugin which owns the
 *       registration of the described hashing algorithm.
 *
 * @return Pointer to a dynamically allocated NUL-terminated string or NULL
 *         on error.
 */
LIBGIFTPROTO_EXPORT
  char *share_dsp_hash (struct file_share *file, const char *type);

/*****************************************************************************/

/**
 * Given a giFT-specific hash string return a static reference to ALGOTYPE.
 * The protocol has no awareness of this hack, and it is only used to
 * serialize hashes over the interface protocol.
 *
 * @param hashstr  Serialized hash string in the form ALGOTYPE:ASCIIDATA
 *
 * @return Pointer to a static NUL-terminated string representation of the
 *         algorithm name or NULL on error.
 */
LIBGIFTPROTO_EXPORT
  char *hashstr_algo (const char *hashstr);

/**
 * Access the data portion of the hash string.  See ::hashstr_algo.
 *
 * @return Pointer into the supplied hashstr object.
 */
LIBGIFTPROTO_EXPORT
  char *hashstr_data (const char *hashstr);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __SHARE_HASH_H */
