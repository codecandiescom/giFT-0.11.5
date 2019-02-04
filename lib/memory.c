/*
 * $Id: memory.c,v 1.6 2003/07/06 03:58:18 jasta Exp $
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

#include "memory.h"

/*****************************************************************************/

static void *check_alloc (void *alloc, size_t size)
{
	if (alloc)
		return alloc;

	/* attention all passengers, please fasten your safety belts as we appear
	 * to have encountered a little minor turbulence on our flight */
	GIFT_ERROR (("failed to allocate %u bytes", (unsigned int)size));

	return alloc;
}

/*****************************************************************************/

void *gift_malloc (size_t size)
{
	if (!size)
		return NULL;

	return check_alloc (malloc (size), size);
}

void *gift_calloc (size_t nmemb, size_t size)
{
	if (nmemb == 0 || size == 0)
		return NULL;

	return check_alloc (calloc (nmemb, size), nmemb * size);
}

void gift_free (void *ptr)
{
	/* ANSI C89 says it's ok to free NULL, so by god, we'll let free NULL
	 * happen! */
	free (ptr);
}

void *gift_realloc (void *ptr, size_t size)
{
	if (!ptr)
		return gift_malloc (size);

	if (size == 0)
	{
		gift_free (ptr);
		return NULL;
	}

	return check_alloc (realloc (ptr, size), size);
}

/*****************************************************************************/

void *gift_memdup (const void *ptr, size_t len)
{
	void *dup;

	if (len == 0)
		return NULL;

	if (!(dup = gift_malloc (len)))
		return NULL;

	memcpy (dup, ptr, len);
	return dup;
}
