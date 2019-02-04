/*
 * $Id: if_transfer.c,v 1.15 2003/08/11 18:27:57 jasta Exp $
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

#include "giftd.h"

#include "lib/event.h"

#include "transfer.h"

#include "if_event.h"
#include "if_transfer.h"

/*****************************************************************************/

static char *get_direction (Transfer *transfer)
{
	char *dir;

	if (!transfer)
		return NULL;

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		dir = "DOWNLOAD";
		break;
	 case TRANSFER_UPLOAD:
		dir = "UPLOAD";
		break;
	 default:
		dir = NULL;
		break;
	}

	assert (dir != NULL);
	return dir;
}

static void set_state (Transfer *transfer)
{
	char *sold;
	char *snew;

	/* save a copy in case we have no change */
	sold = transfer->state;

	if (DOWNLOAD(transfer)->paused)
		snew = "Paused";
	else if (DOWNLOAD(transfer)->verifying)
		snew = "Verifying";
	else if (transfer->transmit >= transfer->total)
		snew = "Completed";
	else
		snew = "Active";

	if (!STRCMP (sold, snew))
		return;

	transfer->state = STRDUP (snew);
	free (sold);
}

static void transfer_body_append (Transfer *transfer, Interface *cmd)
{
	if (!transfer)
		return;

	if (transfer->hash)
		interface_put (cmd, "hash", transfer->hash);

	set_state (transfer);

	interface_put  (cmd, "state", transfer->state);
	INTERFACE_PUTL (cmd, "transmit", transfer->transmit);
	INTERFACE_PUTL (cmd, "size", transfer->total);
	interface_put  (cmd, "file", transfer->filename);

	if (transfer->type == TRANSFER_UPLOAD)
		INTERFACE_PUTI (cmd, "shared", UPLOAD(transfer)->shared);
}

static void put_source (Interface *cmd, char *command, char *keyname,
                        char *value)
{
	char *key;

	if (command)
		key = stringf_dup ("%s/%s", command, keyname);
	else
		key = STRDUP (keyname);

	interface_put (cmd, key, value);
	free (key);
}

static void source_append (Source *source, Interface *cmd, char *command,
                           int child)
{
	Chunk *chunk;

	if (command)
		interface_put (cmd, command, NULL);

	if (!child)
		command = NULL;

	put_source (cmd, command, "user", source->user);
	put_source (cmd, command, "url", source->url);
	put_source (cmd, command, "statusgrl", source_status (source));
	put_source (cmd, command, "status", source_status_proto (source));

	if ((chunk = source->chunk))
	{
		put_source (cmd, command, "start",
		            stringf ("%li", (long)chunk->start));
		put_source (cmd, command, "transmit",
		            stringf ("%li", (long)chunk->transmit));
		put_source (cmd, command, "total",
		            stringf ("%li", (long)(chunk->stop - chunk->start)));
	}
}

/*
 * A source counter is used here because of defficiencies in the interface.c
 * code.  adding SOURCE, SOURCE/user, then SOURCE, and SOURCE/user again
 * causes great confusion.
 */
static unsigned int source_cnt = 0;

static int sources_iter (Source *source, Interface *cmd)
{
	char *command;

	command = stringf_dup ("SOURCE[%u]", source_cnt++);
	source_append (source, cmd, command, TRUE);
	free (command);

	return TRUE;
}

static void transfer_sources_append (Transfer *transfer, Interface *cmd)
{
	list_foreach (transfer->sources, (ListForeachFunc)sources_iter, cmd);
}

static void transfer_attached (TCPC *c, IFEvent *event, void *udata)
{
	Transfer  *transfer;
	Interface *cmd;
	char      *dir;
	char      *id = NULL;

	/* a legitimately attached TCPC should be supplied here if we arent
	 * abusing the function, see below */
	if (!udata)
	{
		assert (c != NULL);
		assert (c->udata != NULL);
	}

	if (!(transfer = if_event_data (event, "Transfer")))
		return;

	if (!(dir = get_direction (transfer)))
		return;

	/* hack hack hack */
	if (!udata)
	{
		if (!(id = stringf_dup ("%lu", if_connection_get_id (c->udata, event))))
			return;
	}

	/* fill in id at if_event_send */
	cmd = interface_new (stringf ("ADD%s", dir), id);
	free (id);

	if (!cmd)
		return;

	transfer_body_append (transfer, cmd);
	transfer_sources_append (transfer, cmd);

	/* if udata has been supplied a new transfer has been registered (and
	 * not merely a new attach).  this is a special condition to reduce
	 * code duplication, sorry */
	if (udata)
		if_event_send (event, cmd);
	else
		interface_send (cmd, c);

	interface_free (cmd);
}

static void transfer_finished (TCPC *c, IFEvent *event, void *udata)
{
	Transfer  *transfer;
	Interface *cmd;
	char      *dir;

	/* update for UIs that want to be lazy */
	if_transfer_change (event, TRUE);

	if (!(transfer = if_event_data (event, "Transfer")))
		return;

	if (!(dir = get_direction (transfer)))
		return;

	/* fill in id at if_event_send */
	if (!(cmd = interface_new (stringf ("DEL%s", dir), NULL)))
		return;

	if_event_send (event, cmd);
	interface_free (cmd);
}

IFEvent *if_transfer_new (TCPC *c, if_event_id requested,
                          Transfer *transfer)
{
	IFEvent *event;

	if (!transfer)
		return NULL;

	event = if_event_new (c, requested, IFEVENT_BROADCAST | IFEVENT_PERSIST,
	                      (IFEventCB) transfer_attached, NULL,
	                      (IFEventCB) transfer_finished, NULL,
	                      "Transfer",                    transfer);

	/* register this new transfer */
	transfer_attached (c, event, &requested);

	return event;
}

void if_transfer_change (IFEvent *event, int force)
{
	Transfer  *transfer;
	Interface *cmd;
	char      *dir;
	long       throughput;
	long       elapsed;

	if (!event || !(transfer = if_event_data (event, "Transfer")))
		return;

	/* determine how much time has elapsed since the last SUCCESSFUL
	 * _change call */
	elapsed = (long)(stopwatch_elapsed (transfer->sw, NULL) * SECONDS);

	/* no change, ignore change request */
	if (!force && transfer->transmit == transfer->transmit_change)
	{
		/* let it slip through if its been a very long time */
		if (elapsed < (60 * SECONDS))
			return;
	}

	if (!(dir = get_direction (transfer)))
		return;

	/* handle throughput */
	throughput = transfer->transmit - transfer->transmit_change;

	transfer->transmit_change = transfer->transmit;
	stopwatch_start (transfer->sw);

	/* construct packet */
	if (!(cmd = interface_new (stringf ("CHG%s", dir), NULL)))
		return;

	INTERFACE_PUTL (cmd, "throughput", throughput);
	INTERFACE_PUTL (cmd, "elapsed", elapsed);

	transfer_body_append (transfer, cmd);
	transfer_sources_append (transfer, cmd);

	if_event_send (event, cmd);
	interface_free (cmd);
}

void if_transfer_addsource (IFEvent *event, Source *source)
{
	Interface *cmd;

	if (!(cmd = interface_new ("ADDSOURCE", NULL)))
		return;

	source_append (source, cmd, NULL, FALSE);

	if_event_send (event, cmd);
	interface_free (cmd);
}

void if_transfer_delsource (IFEvent *event, Source *source)
{
	Interface *cmd;

	if (!(cmd = interface_new ("DELSOURCE", NULL)))
		return;

	source_append (source, cmd, NULL, FALSE);

	if_event_send (event, cmd);
	interface_free (cmd);
}

void if_transfer_finish (IFEvent *event)
{
	if_event_finish (event);
}
