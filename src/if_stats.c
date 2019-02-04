/*
 * $Id: if_stats.c,v 1.5 2003/05/25 23:03:26 jasta Exp $
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

#include "if_event.h"
#include "if_stats.h"

/*****************************************************************************/

/* TODO -- this is a temporary hack */
static List *stats_list = NULL;

struct _stats
{
	char          *protocol;
	unsigned long  users;
	unsigned long  files;
	double         size;
};

IFEvent *if_stats_new (TCPC *c, time_t interval)
{
	return if_event_new (c, 0, IFEVENT_NOID, NULL, NULL, NULL, NULL, NULL, NULL);
}

/* NOTE: size is given in GB */
void if_stats_reply (IFEvent *event, char *protocol,
                     unsigned long users, unsigned long files,
                     double size)
{
	struct _stats *stats;

	if (!(stats = malloc (sizeof (struct _stats))))
		return;

	stats->protocol = STRDUP (protocol);
	stats->users    = users;
	stats->files    = files;
	stats->size     = size;

	stats_list = list_append (stats_list, stats);
}

static void put_stats (Interface *cmd, char *p, char *keyname, char *value)
{
	char *key;

	key = stringf_dup ("%s/%s", p, keyname);
	interface_put (cmd, key, value);
	free (key);
}

static int stats_reply (struct _stats *stats, Interface *cmd)
{
	interface_put (cmd, stats->protocol, NULL);

	/* do not show users for giFT, nothing to show */
	if (strcmp (stats->protocol, "giFT"))
	{
		put_stats (cmd, stats->protocol, "users",
		           stringf ("%lu", stats->users));
	}

	put_stats (cmd, stats->protocol, "files", stringf ("%lu", stats->files));
	put_stats (cmd, stats->protocol, "size", stringf ("%.02f", stats->size));

	free (stats->protocol);
	free (stats);

	return TRUE;
}

void if_stats_finish (IFEvent *event)
{
	Interface *cmd;

	if (!(cmd = interface_new ("STATS", NULL)))
		return;

	list_foreach_remove (stats_list, (ListForeachFunc)stats_reply, cmd);
	stats_list = NULL;

	if_event_send (event, cmd);
	interface_free (cmd);

	if_event_finish (event);
}
