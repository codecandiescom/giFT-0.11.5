/*
 * $Id: memory.h,v 1.15 2003/10/16 18:50:54 jasta Exp $
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

#ifndef __MEMORY_H
#define __MEMORY_H

/*****************************************************************************/

/**
 * @file memory.h
 *
 * @brief Simple wrappers for the C libraries allocation functions.
 */

/*****************************************************************************/

#undef CALLOC
#undef MALLOC
#undef FREE
#undef REALLOC

/*
 * TCGProf is a simple memory profiling tool designed to debug only
 * allocations localized to the users defined space, so anything beyond
 * memory.h will use it.
 *
 * Please note that in order to use tcgprof you must disable libltdl support.
 * Don't ask me why, I just know that tcg_dump_mem() segfaults after a call
 * to ltdl_exit() or ltdl_close().  I would assume this is because any
 * allocations made in the plugins space are cleaned up and thus the
 * references that remain in the tcgprof table are invalid.  That's all I can
 * come up with, anyway...
 */
#ifdef USE_TCGPROF
# include <tcgprof.h>
#endif /* USE_TCGPROF */

#ifndef USE_TCGPROF
# define CALLOC(nmemb,size)  gift_calloc(nmemb,size)
# define MALLOC(size)        CALLOC(1,size)
# define NEW(type)           MALLOC(sizeof(type))
# define FREE(ptr)           gift_free(ptr)
# define REALLOC(ptr,size)   gift_realloc(ptr,size)
#else /* USE_TCGPROF */
# define CALLOC(nmemb,size)  calloc(nmemb,size)
# define MALLOC(size)        CALLOC(1,size)
# define NEW(type)           MALLOC(sizeof(type))
# define FREE(ptr)           free(ptr)
# define REALLOC(ptr,size)   realloc(ptr,size)
#endif /* !USE_TCGPROF */

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Wrapper for malloc that prevents malloc(0).
 */
LIBGIFT_EXPORT
  void *gift_malloc (size_t size);

/**
 * Wrapper for calloc to prevent nmemb 0 or size 0.
 */
LIBGIFT_EXPORT
  void *gift_calloc (size_t nmemb, size_t size);

/**
 * Dummy wrapper for free to just go along with this scheme.
 */
LIBGIFT_EXPORT
  void gift_free (void *ptr);

/**
 * Realloc wrapper that prevents some broken implementations of realloc from
 * failing when ptr is NULL or size is 0.
 */
LIBGIFT_EXPORT
  void *gift_realloc (void *ptr, size_t size);

/*****************************************************************************/

/**
 * Similar to strdup, except that it operates on raw memory.  I'm baffled why
 * such a function was not provided by the standard.
 */
LIBGIFT_EXPORT
  void *gift_memdup (const void *ptr, size_t size);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __MEMORY_H */
