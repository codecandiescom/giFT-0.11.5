/*
 * $Id: mime.c,v 1.5 2003/11/24 03:06:31 hipnod Exp $
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

#include <ctype.h>

#include "libgift.h"

#include "file.h"
#include "parse.h"
#include "mime.h"

#ifdef USE_LIBMAGIC
# include <magic.h>
#endif /* USE_LIBMAGIC */

/*****************************************************************************/

struct mime_type
{
	char *type;
	char *desc;
	char *ext;
};

static Dataset *mime_types     = NULL;

#ifdef USE_LIBMAGIC
static magic_t  magic_db       = NULL;
static BOOL     magic_disabled = FALSE;
#endif /* USE_LIBMAGIC */

/*****************************************************************************/

static int insert_type (char *ext, char *type, char *desc)
{
	struct mime_type *mi;

	if (!(mi = malloc (sizeof (struct mime_type))))
		return FALSE;

	mi->type = STRDUP (type);
	mi->desc = STRDUP (desc);
	mi->ext  = STRDUP (ext);

	dataset_insert (&mime_types, ext, STRLEN_0(ext), mi, 0);
	return TRUE;
}

static void free_type (struct mime_type *mi)
{
	if (!mi)
		return;

	free (mi->ext);
	free (mi->desc);
	free (mi->type);
	free (mi);
}

static void load_types ()
{
	FILE *f;
	char *buf = NULL;
	char *ptr, *filename;

	filename = stringf ("%s/mime.types", platform_data_dir());
	assert (filename != NULL);

	if (!(f = fopen (filename, "r")))
	{
		GIFT_ERROR (("failed to open %s", filename));
		return;
	}

	while (file_read_line (f, &buf))
	{
		char *ext, *exts0, *exts;
		char *type;

		ptr = buf;

		string_trim (ptr);

		if (*ptr == '#')
			continue;

		/* break type and extension list */
		type = string_sep_set (&ptr, " \t");

		if (!ptr || !ptr[0])
			continue;

		string_trim (ptr);

		/* create a unique entry for each type (to satisfy the hash table
		 * lookup) */
		exts0 = exts = STRDUP (ptr);

		while ((ext = string_sep (&exts, " ")))
			insert_type (ext, type, NULL);

		free (exts0);
	}

	fclose (f);
}

#ifdef USE_LIBMAGIC
static magic_t load_database (void)
{
	magic_t db;

	if (magic_disabled)
		return NULL;

	if (magic_db)
		return magic_db;

	if (!(db = magic_open (MAGIC_COMPRESS | MAGIC_MIME)))
		return NULL;

	/* NULL file arg here loads the default mime magic file */
	if (magic_load (db, NULL) < 0)
	{
		GIFT_ERROR (("failed to open magic file: %s", GIFT_STRERROR()));
		magic_disabled = TRUE;
	}

	if (magic_disabled)
	{
		GIFT_WARN (("mime magic support disabled, see previous errors"));
		return NULL;
	}

	return db;
}

/* submitted by saturn <chm at c00 dot info>, so dont blame me for the
 * goto! */
char *mime_magic_type (const char *file)
{
	const char *desc;
	char       *mime;
	char       *mime_final = NULL;
	int         i;

	if (!magic_db)
		return NULL;

	if (!(desc = magic_file (magic_db, file)))
		return NULL;

	GIFT_TRACE (("%s: %s", file, desc));

	/*
	 * Possible descriptions we can get:
	 *
	 *  application/x-tar, POSIX (application/x-gzip)
	 *  text/plain, English; charset=us-ascii
	 *  application/x-archive application/x-debian-package
	 */

	/* extract the mime type, ignore encoding and stuff */
	mime = strdup (desc);

	for (i = 0; mime[i] != '/'; i++)
	{
		if (!(isalnum (mime[i]) || mime[i] == '_' || mime[i] == '-'))
			goto cleanup;
	}

	i++;

	for (; mime[i] != '\0' && !isspace (mime[i]); i++)
	{
		if (!(isalnum (mime[i]) || mime[i] == '_' || mime[i] == '-'))
			goto cleanup;
	}

	mime[i] = '\0';

	if (strcmp (mime, "application/octet-stream") == 0 ||
	    strcmp (mime, "text/plain") == 0)
	{
		/* this usually means that the mime lookup failed, fallback to file
		 * extension instead */
		goto cleanup;
	}

	/* return the entry from the table to save memory */
	mime_final = mime_type_lookup (mime);

 cleanup:

	free (mime);

	return mime_final;
}
#endif /* USE_LIBMAGIC */

/*****************************************************************************/

/* locates the appropriate mime type based on extension */
char *mime_type (const char *file)
{
	struct mime_type *mime_info;
	char *ext, *extl;
	char *type;

	if (!mime_types)
		return NULL;

#ifdef USE_LIBMAGIC
	if ((type = mime_magic_type (file)))
		return type;
#endif /* USE_LIBMAGIC */

	if ((ext = strrchr (file, '.')))
		ext++;

	extl = string_lower (STRDUP(ext));

	/* check for an existent and known extension, otherwise default to
	 * text/plain */
	if ((mime_info = dataset_lookup (mime_types, extl, STRLEN_0(extl))))
		type = mime_info->type;
	else
		type = "application/octet-stream";

	free (extl);

	return type;
}

static int mime_lookup (ds_data_t *key, ds_data_t *value, char *mime)
{
	struct mime_type *val = value->data;

	return (STRCMP (mime, val->type) == 0);
}

char *mime_type_lookup (const char *mime)
{
	struct mime_type *type;

	if (!mime_types)
		return NULL;

	type = dataset_find (mime_types, DS_FIND(mime_lookup), (void *)mime);

	if (!type)
	{
		/* THIS IS SO BROKEN! */
		if (!insert_type ((char *)mime, (char *)mime, NULL))
			return NULL;

		if (!(type = dataset_find (mime_types, DS_FIND(mime_lookup),
		                           (void *)mime)))
			return NULL;
	}

	return type->type;
}

/*****************************************************************************/

BOOL mime_init (void)
{
	load_types ();
#ifdef USE_LIBMAGIC
	load_database ();
#endif

	/* don't check mem failures for now */
	return TRUE;
}

static void clear_type (ds_data_t *key, ds_data_t *value, void *udata)
{
	free_type (value->data);
}

void mime_cleanup (void)
{
	dataset_foreach (mime_types, DS_FOREACH(clear_type), NULL);
	dataset_clear (mime_types);
	mime_types = NULL;

#ifdef USE_LIBMAGIC
	magic_close (magic_db);
	magic_db = NULL;
#endif
}
