/*
 * $Id: mime.h,v 1.3 2003/10/16 18:50:54 jasta Exp $
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

#ifndef __MIME_H
#define __MIME_H

/*****************************************************************************/

LIBGIFT_EXPORT
char *mime_type (const char *file);

LIBGIFT_EXPORT
char *mime_type_lookup (const char *mime);

/*****************************************************************************/

LIBGIFT_EXPORT
BOOL mime_init    (void);

LIBGIFT_EXPORT
void mime_cleanup (void);

/*****************************************************************************/

#endif /* __MIME_H */
