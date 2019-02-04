/*
 * $Id: strobj.h,v 1.9 2003/12/23 03:43:56 jasta Exp $
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

#ifndef __STRING_H
#define __STRING_H

/*****************************************************************************/

/**
 * @file strobj.h
 *
 * @brief Dynamic string manipulation.
 */

/*****************************************************************************/

/**
 * Extremely crude string object.  This supports only the most basic string
 * building operations and even those leave a lot to be desired.  Please
 * don't expect much from this system.
 */
typedef struct
{
	char        *str;                  /**< Internal string buffer associated
	                                    *   with this object */
	int          alloc;                /**< Number of bytes allocated to str */
	int          len;                  /**< Number of bytes written to str */
	BOOL         can_resize;           /**< Is realloc on str OK? */
	BOOL         managed;              /**< Did we allocate str ourselves? */
} String;

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

LIBGIFT_EXPORT
  void string_init (String *sobj);

LIBGIFT_EXPORT
  void string_finish (String *sobj);

LIBGIFT_EXPORT
  char *string_finish_keep (String *sobj);

/*****************************************************************************/

LIBGIFT_EXPORT
  String *string_new (char *str, int alloc, int len, BOOL can_resize);

LIBGIFT_EXPORT
  void string_free (String *sobj);

LIBGIFT_EXPORT
  char *string_free_keep (String *sobj);

/*****************************************************************************/

LIBGIFT_EXPORT
  void string_set_buf (String *sobj, char *str, int alloc, int len,
                       BOOL can_resize);

/*****************************************************************************/

LIBGIFT_EXPORT
  int string_appendvf (String *sobj, const char *fmt, va_list args);

LIBGIFT_EXPORT
  int string_appendf (String *sobj, const char *fmt, ...);

LIBGIFT_EXPORT
  int string_append (String *sobj, const char *str);

LIBGIFT_EXPORT
  int string_appendu (String *sobj, unsigned char *str, size_t len);

LIBGIFT_EXPORT
  int string_appendc (String *sobj, char c);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __STRING_H */
