/*
 * $Id: if_port.c,v 1.22 2003/07/11 08:14:47 jasta Exp $
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

#include "lib/interface.h"
#include "lib/network.h"
#include "lib/event.h"

#include "if_event.h"

#include "daemon.h"

/*****************************************************************************/

extern Config *gift_conf;
static TCPC   *if_port_listen = NULL;

/*****************************************************************************/

/* main reading loop */
static void if_connection (int fd, input_id id, TCPC *c)
{
	Interface     *cmd;
	FDBuf         *buf;
	unsigned char *data;
	size_t         data_len = 0;
	int            n;

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, ";")) < 0)
	{
		if_event_detach (c);
		tcp_close (c);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &data_len);
	assert (data != NULL);

	/*
	 * This can occur because our nb_read loop is actually pretty awful:
	 * search query(foo\;bar); is perfectly valid, however the first ; will
	 * be matched and interface_unserialize should fail.  There is obviously
	 * a much better way of handling this, but for the moment I'm being lazy
	 * so we'll chalk this up as a TODO.
	 */
	if (!(cmd = interface_unserialize (data, data_len)))
	{
		GIFT_TRACE (("packet '%s' appears incomplete, waiting for more input...",
		             data));
		return;
	}

	fdbuf_release (buf);

	daemon_command_handle (c, cmd);
	interface_free (cmd);
}

/*****************************************************************************/

static BOOL if_auth (TCPC *c)
{
	char *allow, *allow0;
	char *host;
	BOOL  ret = FALSE;

	if (!c || c->fd <= 0)
		return FALSE;

	allow0 = allow =
	    STRDUP (config_get_str (gift_conf, "main/hosts_allow=LOCAL"));

	/* loop all available host masks */
	while ((host = string_sep (&allow, " ")))
	{
		if ((ret = net_match_host (c->host, host)))
			break;
	}

	free (allow0);

	return ret;
}

static void handle_incoming (int fd, input_id id, TCPC *c)
{
	TCPC *new_c;

	if (!(new_c = tcp_accept (c, FALSE)))
		return;

	/* make sure they are allowed to connect ;) */
	if (!if_auth (new_c))
	{
		tcp_close (new_c);
		return;
	}

	input_add (new_c->fd, new_c, INPUT_READ,
	           (InputCallback)if_connection, FALSE);
}

/*****************************************************************************/

int if_port_init (in_port_t port)
{
	TCPC *listen;

	if (!(listen = tcp_bind (port, FALSE)))
		return FALSE;

	if_port_listen = listen;

	input_add (listen->fd, listen, INPUT_READ,
	           (InputCallback)handle_incoming, FALSE);

	return TRUE;
}

void if_port_cleanup (void)
{
	tcp_close (if_port_listen);
	if_port_listen = NULL;
}
