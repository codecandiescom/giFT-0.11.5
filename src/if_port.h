/*
 * $Id: if_port.h,v 1.6 2003/04/12 02:40:41 jasta Exp $
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

#ifndef __IF_PORT_H
#define __IF_PORT_H

/*****************************************************************************/

#define INTERFACE_PORT \
	config_get_int (gift_conf, "main/client_port=1213")

/*****************************************************************************/

int  if_port_init    (in_port_t port);
void if_port_cleanup (void);

/*****************************************************************************/

#endif /* __IF_PORT_H */
