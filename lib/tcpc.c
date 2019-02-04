/*
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

#include "libgift.h"

#include "network.h"
#include "event.h"

#include "tcpc.h"

/*****************************************************************************/

struct write_msg
{
	unsigned char *data;
	size_t         len;
};

/*****************************************************************************/

static int recv_buf (int fd, void *buf, size_t len, TCPC *c)
{
	int ret;

	if ((ret = recv (fd, buf, len, 0)) > 0)
		c->in += ret;

	return ret;
}

static int peek_buf (int fd, void *buf, size_t len, TCPC *c)
{
	return recv (fd, buf, len, MSG_PEEK);
}

static TCPC *tcp_new (int fd, in_addr_t host, in_port_t port,
                      int outgoing, void *udata)
{
	TCPC *c;

	if (!(c = MALLOC (sizeof (TCPC))))
		return NULL;

	if (!(c->buf = fdbuf_new (fd, (FDBufRead)recv_buf, (FDBufRead)peek_buf, c)))
	{
		free (c);
		return NULL;
	}

	c->fd       = fd;
	c->host     = host;
	c->port     = port;
	c->outgoing = outgoing;
	c->udata    = udata;

	return c;
}

static void tcp_free (TCPC *c)
{
	if (!c)
		return;

	fdbuf_free (c->buf);
	assert (c->wqueue == NULL);

	free (c);
}

/*****************************************************************************/

TCPC *tcp_open (in_addr_t host, in_port_t port, int block)
{
	TCPC *c;
	int   fd;

	if ((fd = net_connect (net_ip_str (host), port, block)) < 0)
		return NULL;

	/* create the structure */
	if (!(c = tcp_new (fd, host, port, TRUE, NULL)))
		net_close (fd);

	return c;
}

TCPC *tcp_accept (TCPC *listening, int block)
{
	TCPC *c;
	int   fd;

	if (!listening)
		return NULL;

	if ((fd = net_accept (listening->fd, block)) < 0)
		return NULL;

	if (!(c = tcp_new (fd, net_peer (fd), listening->port, FALSE, NULL)))
		net_close (fd);

	return c;
}

TCPC *tcp_bind (in_port_t port, int block)
{
	int fd;

	if (port == 0)
		return NULL;

	if ((fd = net_bind (port, block)) < 0)
		return NULL;

	return tcp_new (fd, 0, port, FALSE, NULL);
}

/*****************************************************************************/

static void finish_queue (TCPC *c)
{
	input_remove (c->wqueue_id);
	c->wqueue_id = 0;

	array_unset (&c->wqueue);
}

static int shift_queue (TCPC *c, int write)
{
	struct write_msg *msg;

	/* grab the next waiting message if available (otherwise we should
	 * assume the queue is now empty instead of a fatal error) */
	if (!(msg = array_shift (&c->wqueue)))
	{
		finish_queue (c);
		return FALSE;
	}

	assert (msg->data != NULL);
	assert (msg->len > 0);

	if (write)
		tcp_send (c, msg->data, msg->len);

	free (msg->data);
	free (msg);

	return TRUE;
}

static void shift_queue_cb (int fd, input_id id, TCPC *c)
{
	assert (c->fd == fd);
	assert (c->wqueue_id == id);

	shift_queue (c, TRUE);
}

static int push_queue (TCPC *c, struct write_msg *msg)
{
	if (!array_push (&c->wqueue, msg))
	{
		GIFT_TRACE (("eep!"));
		return FALSE;
	}

	if (c->wqueue_id == 0)
	{
		c->wqueue_id = input_add (c->fd, c, INPUT_WRITE,
		                          (InputCallback)shift_queue_cb, FALSE);

		assert (c->wqueue_id > 0);
	}

	return TRUE;
}

/*****************************************************************************/

void tcp_close (TCPC *c)
{
	if (!c)
		return;

	tcp_flush (c, FALSE);              /* destroy pending writes */
	input_remove_all (c->fd);          /* remove all event inputs */
	net_close (c->fd);                 /* close the socket */
	tcp_free (c);                      /* destroy the data */
}

void tcp_close_null (TCPC **c)
{
	if (!c || !(*c))
		return;

	tcp_close (*c);
	*c = NULL;
}

/*****************************************************************************/

int tcp_flush (TCPC *c, int write)
{
	int cnt = 0;

	if (!c)
		return 0;

	while (shift_queue (c, write))
		cnt++;

	/* just in case */
	finish_queue (c);

	return cnt;
}

static int push_msg (TCPC *c, unsigned char *data, size_t len)
{
	struct write_msg *msg;

	if (!(msg = malloc (sizeof (struct write_msg))))
		return -1;

	/* copy the requested write memory */
	if (!(msg->data = malloc (len)))
	{
		free (msg);
		return -1;
	}

	memcpy (msg->data, data, len);
	msg->len = len;

	push_queue (c, msg);

	return len;
}

int tcp_write (TCPC *c, unsigned char *data, size_t len)
{
	if (!c || c->fd < 0)
		return -1;

	if (len == 0)
		return 0;

	return push_msg (c, data, len);
}

int tcp_writestr (TCPC *c, char *data)
{
	return tcp_write (c, (unsigned char *)data, STRLEN (data));
}

int tcp_send (TCPC *c, unsigned char *data, size_t len)
{
	int ret;

	if (!c || c->fd < 0)
		return -1;

	if (len == 0)
		return 0;

	if ((ret = net_send (c->fd, (char *)data, len)) > 0)
		c->out += ret;

	return ret;
}

/*****************************************************************************/

FDBuf *tcp_readbuf (TCPC *c)
{
	if (!c)
		return NULL;

	return c->buf;
}

static int wrap_recv (TCPC *c, unsigned char *buf, size_t len, int flags)
{
	int ret;

	if (!c)
		return -1;

	if (len == 0)
		return 0;

	ret = recv (c->fd, (void *)buf, len, flags);

	if (ret > 0 && flags == 0)
		c->in += ret;

	return ret;
}

int tcp_recv (TCPC *c, unsigned char *buf, size_t len)
{
	return wrap_recv (c, buf, len, 0);
}

int tcp_peek (TCPC *c, unsigned char *buf, size_t len)
{
	return wrap_recv (c, buf, len, MSG_PEEK);
}
