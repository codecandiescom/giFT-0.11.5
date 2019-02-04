/*
 * $Id: if_event.h,v 1.6 2003/07/10 15:32:05 jasta Exp $
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

#ifndef __IF_EVENT_H
#define __IF_EVENT_H

/*****************************************************************************/

#include "plugin/if_event_api.h"

/*****************************************************************************/

typedef enum
{
	IFEVENT_NONE       = 0x00,
	IFEVENT_NOID       = 0x01,         /* do not report identifier */
	IFEVENT_BROADCAST  = 0x02,         /* broadcast to all clients */
	IFEVENT_PERSIST    = 0x04          /* persist even if all clients leave */
} IFEventFlags;

struct if_event;
typedef void (*IFEventCB) (TCPC *c, struct if_event *event, void *udata);

typedef struct if_event
{
	/* unique identifer associated w/ this event */
	if_event_id  id;

	/* list of connections to report status of this event back to.  if
	 * IFEVENT_PERSIST, this may be NULL */
	List        *reply;

	/* connection that initiated this event */
	TCPC        *initiate;

	IFEventFlags flags;

	IFEventCB    attached;
	void        *attached_data;
	IFEventCB    finished;
	void        *finished_data;

	/* whatever giFT wants to put here */
	char        *data_name;
	void        *data;
} IFEvent;

/*
 * this structure is assigned to Connection->data and is used as an
 * indicator for the attached status of the current interface connection.
 * also contains the lookup table of session-specific identifiers
 */
typedef struct
{
	Dataset *notify;
	Dataset *events;
} IFConnection;

/*****************************************************************************/

IFEvent  *if_event_new    (TCPC *c, if_event_id session_id,
                           IFEventFlags flags,
                           IFEventCB attached, void *attached_data,
                           IFEventCB finished, void *finished_data,
                           char *data_name, void *data);
void      if_event_finish (IFEvent *event);
void      if_event_send   (IFEvent *event, Interface *cmd);

void     *if_event_data   (IFEvent *event, char *data_name);
IFEvent  *if_event_lookup (if_event_id id);

void      if_event_attach (TCPC *c);
void      if_event_detach (TCPC *c);

if_event_id if_connection_get_id    (IFConnection *ifc, IFEvent *event);
IFEvent    *if_connection_get_event (IFConnection *ifc, if_event_id id);

/*****************************************************************************/

#endif /* __IF_EVENT_H */
