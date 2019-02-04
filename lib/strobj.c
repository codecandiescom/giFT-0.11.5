/*
 * $Id: strobj.c,v 1.14 2003/12/23 03:37:21 jasta Exp $
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

#include "libgift.h"

#include "string.h"

/*****************************************************************************/

/*
 * Default allocated size for the internally managed character string buffer.
 * This does not apply when initialized with an already allocated buffer
 * object.
 */
#define STRING_SIZE 128

/*****************************************************************************/

void string_init (String *sobj)
{
	assert (sobj != NULL);

	sobj->str = NULL;
	sobj->alloc = 0;
	sobj->len = 0;

	sobj->can_resize = TRUE;
	sobj->managed = TRUE;
}

void string_finish (String *sobj)
{
	assert (sobj != NULL);

	if (sobj->managed)
	{
		assert (sobj->can_resize == TRUE);
		free (sobj->str);
	}
}

char *string_finish_keep (String *sobj)
{
	char *str;

	assert (sobj != NULL);

	if (sobj->managed)
		sobj->managed = FALSE;

	str = sobj->str;

	string_finish (sobj);

	return str;
}

/*****************************************************************************/

String *string_new (char *str, int alloc, int len, BOOL can_resize)
{
	String *sobj;

	if (!(sobj = malloc (sizeof (String))))
		return NULL;

	string_init (sobj);
	string_set_buf (sobj, str, alloc, len, can_resize);

	return sobj;
}

void string_free (String *sobj)
{
	if (!sobj)
		return;

	string_finish (sobj);
	free (sobj);
}

char *string_free_keep (String *sobj)
{
	char *str;

	if (!sobj)
		return NULL;

	str = string_finish_keep (sobj);
	free (sobj);

	return str;
}

/*****************************************************************************/

void string_set_buf (String *sobj, char *str, int alloc, int len,
                     BOOL can_resize)
{
	if (!sobj)
		return;

	/* make sure we clean up any previously used string buffer */
	if (sobj->str && sobj->managed)
		free (sobj->str);

	sobj->str = str;
	sobj->alloc = alloc;
	sobj->len = len;

	sobj->can_resize = can_resize;
	sobj->managed = ((str) ? (FALSE) : (TRUE));

	/*
	 * Guarantee a sentinel if possible.  This interface does not strictly
	 * require NUL-terimnation, but it will guarantee it if possible.
	 */
	if (sobj->str && sobj->alloc > sobj->len)
		sobj->str[sobj->len] = 0;
}

/*****************************************************************************/

static int string_resize (String *sobj, int new_alloc)
{
	char *newbuf;

	if (!sobj->can_resize)
	{
		if (new_alloc > sobj->alloc)
			return 0;

		return sobj->alloc;
	}

	/* realloc only if the object was truncated to zero and never reduced in
	 * size or the requested allocation is larger than the current size */
	if (sobj->len == 0 || new_alloc > sobj->alloc)
	{
		/* uh-oh */
		if (!(newbuf = realloc (sobj->str, new_alloc)))
			return 0;

		sobj->str   = newbuf;
		sobj->alloc = new_alloc;
	}

	return sobj->alloc;
}

/*****************************************************************************/

int string_appendvf (String *sobj, const char *fmt, va_list args)
{
	int written = 0;

	if (!sobj)
		return 0;

	if (sobj->alloc == 0)
	{
		if (string_resize (sobj, STRING_SIZE) == 0)
			return 0;
	}

	/* loop until we've got a buffer big enough to write this damned
	 * string, or until we can no longer make the buffer any bigger */
	for (;;)
	{
		size_t max;

		if (sobj->len < sobj->alloc)
		{
			max = sobj->alloc - sobj->len;

			written = vsnprintf (sobj->str + sobj->len, max, fmt, args);

			/*
			 * Some implementations use -1 to indicate an inability to write
			 * the complete buffer, some return a value equal to the number
			 * of bytes needed (excluding NUL) to perform the operation.
			 */
			if (written > -1 && written < max)
				break;
		}

		/* if we got here, we don't have enough memory in the underlying buffer
		 * to fit this fmt buffer */
		if (string_resize (sobj, sobj->alloc * 2) == 0)
			return 0;
	}

	/* calculate the new string length */
	sobj->len += written;

	/* return the number of bytes added by this call, not the total */
	return written;
}

int string_appendf (String *sobj, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start (args, fmt);
	ret = string_appendvf (sobj, fmt, args);
	va_end (args);

	return ret;
}

int string_append (String *s, const char *str)
{
	/* TODO: opt */
	return string_appendf (s, "%s", str);
}

int string_appendu (String *s, unsigned char *str, size_t len)
{
	/* TODO: FIXME */
	if (!string_resize (s, s->len + len + 1))
		return 0;

	memcpy (s->str + s->len, str, len);
	s->len += len;

	/* always maintain a trailing \0, even when the data interface is
	 * not a human readable string by convention */
	s->str[s->len] = 0;

	return len;
}

int string_appendc (String *s, char c)
{
	return string_appendf (s, "%c", c);
}

/*****************************************************************************/

#if 0
int main (int argc, char **argv)
{
	String *s;

	s = string_new (NULL, 0, 0, TRUE);

	string_append (s, "s: ");

	while (argc--)
		string_appendf (s, "%s ", *argv++);

	string_appendc (s, '\n');

	printf ("%s", s->str);

	string_free (s);

	return 0;
}
#endif
