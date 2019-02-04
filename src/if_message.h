/*
 * $Id: if_message.h,v 1.4 2003/03/12 03:07:02 jasta Exp $
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

#ifndef __IF_MESSAGE_H
#define __IF_MESSAGE_H

/*****************************************************************************/

typedef char* (*IFMessageCB) (TCPC *c, IFEvent *event, void *udata);
IFEvent *if_message_new (TCPC *c, Protocol *p,
                         IFMessageCB display_msg, void *udata);
void if_message_finish (IFEvent *event);
void if_message_send (TCPC *c, Protocol *p, char *msg);

/*****************************************************************************/

#endif /* __IF_MESSAGE_H */
