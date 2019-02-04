/*
 * $Id: share_hash.c,v 1.11 2003/11/06 11:14:44 jasta Exp $
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

#include "lib/file.h"

#include "protocol.h"
#include "share.h"

#include "share_hash.h"

/*****************************************************************************/

/*
 * Please note that a similar data structure is also maintained on the
 * protocol itself.  This is here only to avoid complication of colliding
 * algorithms.
 */
static Dataset *algos = NULL;

/*****************************************************************************/

static HashAlgo *algo_new (const char *type, int opt,
                           HashFn algofn, HashDspFn dspfn)
{
	HashAlgo *algo;

	if (!(algo = MALLOC (sizeof (HashAlgo))))
		return NULL;

	assert (type != NULL);
	assert (algofn != NULL);

	algo->ref    = 0;
	algo->opt    = opt;
	algo->type   = type;
	algo->algofn = algofn;
	algo->dspfn  = dspfn;

	if (algo->opt & HASH_PRIMARY)
		algo->opt |= HASH_LOCAL;

	return algo;
}

static void algo_free (HashAlgo *algo)
{
	assert (algo->ref <= 0);
	free (algo);
}

/*****************************************************************************/

Hash *hash_new (HashAlgo *algo, unsigned char *data, size_t len, BOOL copy)
{
	Hash *hash;

	assert (data != NULL);
	assert (len > 0);

	if (!(hash = MALLOC (sizeof (Hash))))
		return NULL;

	/* NOTE: 'algo' must be allowed to be null to handle hashes that aren't
	 * unregistered now, such as plugins that are not currently loaded */
#if 0
	assert (algo != NULL);
#endif

	hash->algo = algo;

	if (copy)
		hash->data = gift_memdup (data, len);
	else
		hash->data = data;

	hash->copy = copy;
	hash->len = len;

	if (!hash->data)
	{
		free (hash);
		return NULL;
	}

	return hash;
}

void hash_free (Hash *hash)
{
	if (!hash)
		return;

	if (hash->copy)
		free (hash->data);

	free (hash);
}

Hash *hash_dup (Hash *hash)
{
	Hash *dup;

	if (!hash)
		return NULL;

	if (!(dup = MALLOC (sizeof (Hash))))
		return NULL;

	dup->algo = hash->algo;
	dup->data = gift_memdup (hash->data, hash->len);
	dup->len  = hash->len;

	return dup;
}

Hash *hash_calc (HashAlgo *algo, const char *path)
{
	Hash          *hash = NULL;
	unsigned char *data;
	size_t         len = 0;

	if (!path)
		return NULL;

	assert (algo != NULL);

	if ((data = algo->algofn (path, &len)))
	{
		hash = hash_new (algo, data, len, TRUE);
		free (data);
	}

	return hash;
}

char *hash_dsp (Hash *hash)
{
	HashAlgo *algo;
	char     *dsp;
	String   *dsphack;

	if (!hash || !(algo = hash->algo))
		return NULL;

	assert (algo->dspfn != NULL);      /* TODO */

	if (!(dsp = algo->dspfn (hash->data, hash->len)))
		return NULL;

	/* display strings are special serialized representations of the data
	 * and the algorithm name */
	if ((dsphack = string_new (NULL, 0, 0, TRUE)))
		string_appendf (dsphack, "%s:%s", algo->type, dsp);

	free (dsp);

	return string_free_keep (dsphack);
}

/*****************************************************************************/

HashAlgo *hash_algo_lookup (const char *type)
{
	return dataset_lookupstr (algos, (char *)type);
}

unsigned int hash_algo_register (Protocol *p, const char *type, int opt,
                                 HashFn algofn, HashDspFn dspfn)
{
	HashAlgo *algo;

	if (!p || !type || !algofn)
		return 0;

	if (!(algo = hash_algo_lookup (type)))
	{
		if (!(algo = algo_new (type, opt, algofn, dspfn)))
			return 0;

		/* add the newly allocated algorithm to the global list, then fall
		 * through and assert that it is in place in the protocol structure
		 * as well */
		dataset_insert (&algos, (char *)type, STRLEN_0(type), algo, 0);
	}

	/* use a reference counting system to know when it's safe to unload */
	algo->ref++;

	/* add another index so that we can breakdown hashes from protocols */
	dataset_insert (&p->hashes, type, STRLEN_0(type), algo, 0);

	return algo->ref;
}

unsigned int hash_algo_unregister (Protocol *p, const char *type)
{
	HashAlgo *algo;

	algo = hash_algo_lookup (type);
	assert (algo != NULL);

	dataset_remove (p->hashes, type, STRLEN_0(type));

	/* this algorithm still has references out there, keep it around */
	if (--algo->ref > 0)
		return algo->ref;

	/* proceed with its destruction */
	dataset_removestr (algos, (char *)type);
	algo_free (algo);

	return 0;
}

/*****************************************************************************/

/* not really the first in insertion order, but the first in the
 * dataset's internal order */
static int get_first (ds_data_t *key, ds_data_t *value, void *udata)
{
	Hash     *hash = value->data;
	HashAlgo *algo = hash->algo;

	if (!algo)
		return FALSE;

	if (!(algo->opt & HASH_LOCAL))
		return FALSE;

	return TRUE;
}

Hash *share_get_hash (Share *file, const char *type)
{
	Hash *hash;

	if (!file)
		return NULL;

	if (type)
		hash = dataset_lookupstr (file->hash, (char *)type);
	else
	{
		/* extract any of the hashes */
		hash = dataset_find (file->hash, DS_FIND(get_first), NULL);
	}

	return hash;
}

static int set_hash (Share *file, const char *type, Hash *hash)
{
	dataset_insert (&file->hash, (char *)type, STRLEN_0(type), hash, 0);

	return TRUE;
}

BOOL share_set_hash (Share *file, const char *type,
                     unsigned char *data, size_t len, BOOL copy)
{
	Hash     *hash;
	HashAlgo *algo;

	if (!file || !type || !data || len == 0)
		return FALSE;

	assert (share_get_hash (file, type) == NULL);

	algo = hash_algo_lookup (type);

	/* NOTE: algo may be null here */
	if (!(hash = hash_new (algo, data, len, copy)))
		return FALSE;

	return set_hash (file, type, hash);
}

static int sh_clear (ds_data_t *key, ds_data_t *value, Share *file)
{
	hash_free (value->data);

	return DS_CONTINUE | DS_REMOVE;
}

void share_clear_hash (Share *file)
{
	if (!file)
		return;

	dataset_foreach_ex (file->hash, DS_FOREACH_EX(sh_clear), file);
	dataset_clear (file->hash);

	file->hash = NULL;
}

void share_foreach_hash (Share *file, DatasetForeachFn func, void *udata)
{
	if (!file || !func)
		return;

	dataset_foreach (file->hash, func, udata);
}

static Hash *algo_run_calc (HashAlgo *algo, char *path)
{
	Hash *hash;

	hash = hash_calc (algo, path);
	free (path);

	return hash;
}

static void algo_run (ds_data_t *key, ds_data_t *value, Array **args)
{
	Hash         *hash;
	HashAlgo     *algo = value->data;
	Share        *file;
	unsigned int *i;

	array_list (args, &file, &i, NULL);

	/* not supposed to use this algorithm for local hashing, so skip it */
	if (!(algo->opt & HASH_LOCAL))
		return;

	/* hash already present, assume that it is up to date (the mtime
	 * checking code exists elsewhere in the code) */
	if (share_get_hash (file, algo->type))
		return;

	/* dirty little trick to make the allocation copy as a local arg to this
	 * function so its easier to work with */
	if ((hash = algo_run_calc (algo, file_host_path (file->path))))
	{
		set_hash (file, algo->type, hash);
		(*i)++;
	}
}

unsigned int share_run_hash (Share *file)
{
	Array       *args = NULL;
	unsigned int i = 0;

	if (!file)
		return 0;

	array_push (&args, file);
	array_push (&args, &i);

	dataset_foreach (algos, DS_FOREACH(algo_run), &args);

	array_unset (&args);

	return i;
}

char *share_dsp_hash (Share *file, const char *type)
{
	Hash *hash;

	if (!(hash = share_get_hash (file, type)))
		return NULL;

	return hash_dsp (hash);
}

/*****************************************************************************/

char *hashstr_algo (const char *hashstr)
{
	char       *colon;
	size_t      len;
	static char algostr[32];

	if (!hashstr || !(colon = strchr (hashstr, ':')))
		return NULL;

	/* determine the length of the text leading up to the ':', so that we
	 * can copy into our internal buffer */
	len = CLAMP ((colon - hashstr), 0, (sizeof (algostr) - 1));

	/* strncpy is evil */
	memcpy (algostr, hashstr, len);
	algostr[len] = 0;

	return algostr;
}

char *hashstr_data (const char *hashstr)
{
	char *data;

	if (!hashstr || !(data = strchr (hashstr, ':')))
		return (char *)hashstr;

	return (data + 1);
}
