/*
 * $Id: if_event_api.h,v 1.3 2003/07/02 08:29:53 jasta Exp $
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

#ifndef __IF_EVENT_API_H
#define __IF_EVENT_API_H

/*****************************************************************************/

/**
 * @file if_event_api.h
 *
 * @brief Necessary definitions for plugins.
 */

/*****************************************************************************/

typedef uint16_t if_event_id;

struct if_event;
#ifdef GIFT_PLUGIN
typedef struct if_event IFEvent;
#endif /* GIFT_PLUGIN */

/*****************************************************************************/

#endif /* __IF_EVENT_API_H */
