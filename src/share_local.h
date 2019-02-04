/*
 * $Id: share_local.h,v 1.2 2003/06/22 19:11:48 jasta Exp $
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

#ifndef __SHARE_LOCAL_H
#define __SHARE_LOCAL_H

/*****************************************************************************/

/**
 * @file share_local.h
 *
 * @brief Lookup interface for accessing cached share objects.
 *
 * This interface is primarily created for use by protocol plugins and is
 * implemented as Protocol::share_lookup.
 */

/*****************************************************************************/

/* defines the command constants */
#include "plugin/protocol.h"

/*****************************************************************************/

/**
 * This is used by plugin.c to bridge the gap between the plugin
 * communication protocol and this local giftd function.
 */
Share *share_local_lookupv (int command, va_list args);

/**
 * Main lookup function.  See Protocol::share_lookup for more extensive API
 * documentation.
 */
Share *share_local_lookup (int command, ...);

/*****************************************************************************/

#endif /* __SHARE_LOCAL_H */
