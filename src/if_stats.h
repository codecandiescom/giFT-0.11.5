/*
 * $Id: if_stats.h,v 1.4 2003/03/12 03:07:05 jasta Exp $
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

#ifndef __IF_STATS_H
#define __IF_STATS_H

/*****************************************************************************/

IFEvent  *if_stats_new     (TCPC *c, time_t interval);
void      if_stats_reply   (IFEvent *event, char *protocol,
                            unsigned long users, unsigned long files,
                            double size);
void      if_stats_finish  (IFEvent *event);

/*****************************************************************************/

#endif /* __IF_STATS_H */
