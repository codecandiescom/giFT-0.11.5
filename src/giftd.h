/*
 * $Id: giftd.h,v 1.4 2003/10/16 18:50:55 jasta Exp $
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

#ifndef __GIFTD_H
#define __GIFTD_H

/*****************************************************************************/

/**
 * @file giftd.h
 *
 * @brief Centralized header for the giFT daemon.
 */

/*****************************************************************************/

/* hack to prefix all things produced by TRACE and friends with "giFT: " */
#ifndef LOG_PFX
# define LOG_PFX "giFT: "
#endif /* LOG_PFX */

/* defined by the autotools environment when building from our own local
 * tree ... */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

/* we don't need GIFTD_EXPORT for anything but building the giFT DLL on win32 */
#ifndef GIFTD_EXPORT
# define GIFTD_EXPORT
#endif

#define    GIFT_DEBUG
#define LIBGIFT_DEBUG

/* the giFT daemon is still a slave to libgift */
#include "lib/libgift.h"

/*****************************************************************************/

/*
 * We export gift_main on windows if we build giFT as a DLL and use
 * win32/loader.c as a wrapper.  This allows us to implement the loader as a
 * windows service for example without having to modify giFT.
 */
GIFTD_EXPORT
  int gift_main (int argc, char **argv);

/*****************************************************************************/

#endif /* __GIFTD_H */
