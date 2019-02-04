/*
 * $Id: libgift.h,v 1.10 2003/10/16 18:57:33 jasta Exp $
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

#ifndef __LIBGIFT_H
#define __LIBGIFT_H

/*****************************************************************************/

/**
 * @file libgift.h
 *
 * @brief Centralized header for all things using libgiFT.
 */

/*****************************************************************************/

/* this should be moved to something controlled by configure */
#ifndef LIBGIFT_DEBUG
# define LIBGIFT_DEBUG
#endif /* LIBGIFT_DEBUG */

/* conditionally setup the environment based on configured features */
#include "giftconfig.h"

/* we don't need LIBGIFT_EXPORT for anything but building the DLL on win32 */
#ifndef LIBGIFT_EXPORT
# define LIBGIFT_EXPORT
#endif

/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if TIME_WITH_SYS_TIME
# include <time.h>
# include <sys/time.h>
#else /* !TIME_WITH_SYS_TIME */
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif /* TIME_WITH_SYS_TIME */

/*****************************************************************************/

#ifndef MIN
# define MIN(x,y)              (((x)<(y))?(x):(y))
#endif

#ifndef MAX
# define MAX(x,y)              (((x)>(y))?(x):(y))
#endif

#ifndef CLAMP
# define CLAMP(value,min,max)  (((value)<(min))?(min):(((value)>(max))?(max):(value)))
#endif

#ifndef INTCMP
# define INTCMP(x,y)           (((x)>(y))?1:(((x)<(y))?-1:0))
#endif

#ifndef BOOL_EXPR
# define BOOL_EXPR(expr)       ((expr)?(TRUE):(FALSE))
#endif /* BOOL_EXPR */

/*****************************************************************************/

#include "platform.h"
#include "memory.h"
#include "strobj.h"
#include "parse.h"

#include "dataset.h"
#include "list.h"
#include "list_lock.h"
#include "tree.h"
#include "array.h"

#include "event.h"
#include "network.h"
#include "tcpc.h"
#include "interface.h"

#include "conf.h"
#include "log.h"

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Initialize all libgiFT subsystems.  This should be a safe operation no
 * matter how much you decide to use libgiFT functionality.
 *
 * @param prog     Your programs identity, not argv[0].  For example, giFT
 *                 would use "giFT".
 * @param opt      Log options to use if the logging facility will be required.
 *                 It is recommended that you supply GLOG_STDERR for direct
 *                 output to stderr.
 * @param logfile  If the GLOG_FILE flag was set, this is the file that will
 *                 be written to.  Otherwise, it has no effect and it is
 *                 recommended that you supply NULL.
 */
LIBGIFT_EXPORT
  int libgift_init (const char *prog, LogOptions opt, const char *logfile);

/**
 * Uninitialize any applicable data.  This is merely used for a graceful exit
 * and not strictly required.  At any rate, use it anyway :)
 */
LIBGIFT_EXPORT
  int libgift_finish (void);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __LIBGIFT_H */
