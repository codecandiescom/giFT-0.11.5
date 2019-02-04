/*
 * $Id: tcpc.h,v 1.5 2003/10/16 18:50:55 jasta Exp $
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

#ifndef __TCPC_H
#define __TCPC_H

/*****************************************************************************/

/**
 * @file tcpc.h
 *
 * @brief High-level TCP connection abstraction.
 *
 * This structure is passed along to pretty much every internal giFT function
 * to encompass all the network functionality that will be required. Also the
 * recommended method of interfacing with the event loop.
 */

/*****************************************************************************/

#include "fdbuf.h"

/*****************************************************************************/

/**
 * Basic building block for all connections spawned using libgiFT.  If you
 * wish to use libgiFT at all, use these.
 */
typedef struct tcp_conn
{
	/**
	 * @name Public
	 */
	FDBuf            *buf;             /**< Read buffer */
	void             *udata;           /**< Any arbitrary data that you want
	                                    *   passed along */

	/**
	 * @name Read-only
	 */
	int               fd;              /**< The file discriptor of this
	                                    *   connection */
	in_addr_t         host;            /**< Peer host address */
	in_port_t         port;            /**< Peer connection port */
	unsigned char     outgoing : 1;    /**< Simple convenience so you know
	                                    *   which direction this connection
	                                    *   was initiated */
	unsigned long     in;              /**< Number of bytes received */
	unsigned long     out;             /**< Number of bytes sent */

	/**
	 * @name Private
	 */
	Array            *wqueue;          /**< Write queue for ::tcp_write */
	input_id          wqueue_id;       /**< Automagically write when avail */
	float             adjust;          /**< Ratio to adjust the send/recv
	                                    *   buffer */
} TCPC;

typedef TCPC Connection;               /**< Backwards compatibility */

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Creates a new connection structure and attempts an outgoing TCP
 * connection using ::net_connect.
 *
 * @see net_connect
 *
 * @return Pointer to a dynamically allocated Connection structure.
 */
LIBGIFT_EXPORT
  TCPC *tcp_open (in_addr_t host, in_port_t port, int block);

/**
 * Accepts an incoming socket connection.
 *
 * @see tcp_open
 * @see net_accept
 */
LIBGIFT_EXPORT
  TCPC *tcp_accept (TCPC *listening, int block);

/**
 * Binds to the supplied port.
 *
 * @see tcp_open
 * @see net_accept
 * @see net_bind
 */
LIBGIFT_EXPORT
  TCPC *tcp_bind (in_port_t port, int block);

/**
 * Close (and free) a connection pointer.
 *
 * @param c  Location of the Connection structure.
 */
LIBGIFT_EXPORT
  void tcp_close (TCPC *c);

/**
 * Wrapper for ::tcp_close that sets the calling argument to NULL
 * afterwards.
 *
 * @param c  Address of a Connection pointer.
 */
LIBGIFT_EXPORT
  void tcp_close_null (TCPC **c);

/*****************************************************************************/

/**
 * Flush and destroy the pending write queue.
 *
 * @param c
 * @param write  If TRUE, appropriate send calls will be performed.  Otherwise,
 *               the data will simply be destroyed.
 *
 * @return Number of elements removed.
 */
LIBGIFT_EXPORT
  int tcp_flush (TCPC *c, int write);

/**
 * Abstracted send call implemented with a user-space socket write queue to
 * avoid flooding the kernel.
 */
LIBGIFT_EXPORT
  int tcp_write (TCPC *c, unsigned char *data, size_t len);

/**
 * Simple wrapper for ::tcp_write for NUL-terminated strings.
 */
LIBGIFT_EXPORT
  int tcp_writestr (TCPC *c, char *data);

/**
 * Direct send wrapper to maintain a consistent interface.
 */
LIBGIFT_EXPORT
  int tcp_send (TCPC *c, unsigned char *data, size_t len);

/**
 * Accessor for the buffer object.
 */
LIBGIFT_EXPORT
  FDBuf *tcp_readbuf (TCPC *c);

/**
 * Direct recv wrapper to maintain a consistent interface.
 */
LIBGIFT_EXPORT
  int tcp_recv (TCPC *c, unsigned char *buf, size_t len);

/**
 * Identical to ::tcp_recv, except that recv is given the MSG_PEEK option.
 */
LIBGIFT_EXPORT
  int tcp_peek (TCPC *c, unsigned char *buf, size_t len);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __TCPC_H */
