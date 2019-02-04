/*
 * $Id: network.h,v 1.28 2003/12/09 10:22:14 hipnod Exp $
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

#ifndef __NETWORK_H
#define __NETWORK_H

/*****************************************************************************/

/**
 * @file network.h
 *
 * @brief Abstracted socket routines.
 *
 * Contains high-level abstraction of the low-level socket functions.  Most
 * of these routines are meant for TCP socket manipulation only, but UDP will
 * likely come in the future.  UI developers should make sure they see
 * ::tcp_open before using any of these routines.
 */

/*****************************************************************************/

#include <fcntl.h>
#include <sys/types.h>

/* socket includes */
#ifndef WIN32
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
#endif /* !WIN32 */

/*****************************************************************************/

/**
 * Sane network block size.
 */
#define RW_BUFFER       2048

/**
 * Socket layer read/write buffer.  Used for throttling of bandwidth usage
 * during large transfers.
 */
#define RW_SOCKBUFFER   65535  /* socket layer read/write buffer */

/**
 * Defined when ::net_ip_strbuf is to be provided.  This causes a slight
 * slowdown in ::net_ip_str, and should be disabled if the functionality will
 * never be used.
 */
#define NET_IP_STRBUF

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Close a socket file descriptor.  Use this routine if portability is
 * important to you.
 *
 * @param fd File descriptor to close.
 */
LIBGIFT_EXPORT
  void net_close (int fd);

/**
 * Make an outgoing TCP connection
 *
 * @param ip       IP address (in dotted notation) that you wish to connect to.
 * @param port
 * @param blocking
 *   Boolean value used to set the blocking (or non-blocking) state of the
 *   socket that will be created.  If you're unsure you probably want
 *   blocking set to TRUE.
 *
 * @return Newly created socket.
 *
 * @retval >=0
 *   Successfully created the socket and if blocking, successfully connected
 *   to the remote host.  You should be aware that if you do not set blocking
 *   this call will need to be later checked with \ref net_sock_error.
 * @retval -1   Error.
 */
LIBGIFT_EXPORT
  int net_connect (const char *ip, in_port_t port, BOOL blocking);

/**
 * Accept an incoming connection.
 *
 * @note This function creates a new socket using the \em accept call.  For
 *       more information try looking up the man page.
 *
 * @param sock     Bind socket to accept from.
 * @param blocking Blocking state to use for the new socket.
 *
 * @return See accept's manual page.
 */
LIBGIFT_EXPORT
  int net_accept (int sock, BOOL blocking);

/**
 * Bind (and listen) on the specified port.
 *
 * @param port     Port to listen on.
 * @param blocking Blocking state for the new socket.
 *
 * @return Newly created socket.
 */
LIBGIFT_EXPORT
  int net_bind (in_port_t port, BOOL blocking);

/**
 * Manually change the blocking status of a socket that has already been
 * created.
 *
 * @param fd       File descriptor to manipulate.
 * @param blocking Blocking state for the original socket.
 */
LIBGIFT_EXPORT
  int net_set_blocking (int fd, BOOL blocking);

/**
 * Check if the socket has any pending errors resulting from a previous
 * socket operation.  If you are using a non-blocking socket scheme you will
 * need to use this function when the selected socket gains the WRITE state
 * after sending a connect request.
 *
 * @param fd  File descriptor to check.
 *
 * @return If greater than zero, the socket has errors as reported by
 *         getsockopt and the return value reflects this error.  If
 *         less than zero, getsockopt failed.  If exactly 0, no errors
 *         present on this socket.
 */
LIBGIFT_EXPORT
  int net_sock_error (int fd);

/**
 * Send data over the socket
 *
 * @param fd   File descriptor for writing.
 * @param data Linear character array to write.
 * @param len
 *   Length of non-terminated data stream.  Use 0 if you wish to simply call
 *   strlen from within this function.
 *
 * @return See send's manual page.
 */
LIBGIFT_EXPORT
  int net_send (int fd, const char *data, size_t len);

/**
 * Wrapper for inet_addr.
 */
LIBGIFT_EXPORT
  in_addr_t net_ip (const char *ip_str);

/**
 * Formats an IPv4 address for display.
 *
 * @param ip IPv4 address to format.
 *
 * @return Pointer to internally used memory that describes the dotted
 * quad address notation of the supplied long address.
 */
LIBGIFT_EXPORT
  char *net_ip_str (in_addr_t ip);

#ifdef NET_IP_STRBUF
/**
 * Similar to ::net_ip_str except that the supplied buffer will be used to
 * copy the inet_ntoa(3) result into.  ::net_ip_str uses its own internal
 * buffer when this function is provided so that they will never collide.
 */
LIBGIFT_EXPORT
  char *net_ip_strbuf (in_addr_t ip, char *buf, size_t size);
#endif /* NET_IP_STRBUF */

/**
 * Determine the peer IP address.
 *
 * @param fd  File descriptor to examine.  This descriptor must be fully
 *            connected in order for this function do anything.
 */
LIBGIFT_EXPORT
  in_addr_t net_peer (int fd);

/**
 * Wrapper for net_peer which passes the result to ::net_ip_str.
 */
LIBGIFT_EXPORT
  char *net_peer_ip (int fd);

/**
 * Determine the locally bound ip address and optionally port.
 */
LIBGIFT_EXPORT
  in_addr_t net_local_ip (int fd, in_port_t *port);

/**
 * Returns a network mask.
 *
 * @param bitwidth Width of the mask.
 *
 * @return Network order IPv4 address.
 */
LIBGIFT_EXPORT
  in_addr_t net_mask (int bitwidth);

/**
 * Matches an IP address against a worded notation.
 *
 * @param ip    IPv4 address supplied for comparison.
 * @param match
 *   Worded notation.  Either ALL, LOCAL, or a dotted quad notation with an
 *   optionally appended "/" followed by the bitwidth of the mask to apply.
 *
 * @retval TRUE  Successful match.
 * @retval FALSE No match could be identified.
 */
LIBGIFT_EXPORT
  BOOL net_match_host (in_addr_t ip, char *match);

/*****************************************************************************/

LIBGIFT_EXPORT
  uint8_t net_get8 (unsigned char *src);
LIBGIFT_EXPORT
  uint16_t net_get16 (unsigned char *src, int tohost);
LIBGIFT_EXPORT
  uint32_t net_get32 (unsigned char *src, int tohost);

LIBGIFT_EXPORT
  void net_put8 (unsigned char *dst, uint8_t src);
LIBGIFT_EXPORT
  void net_put16 (unsigned char *dst, uint16_t src);
LIBGIFT_EXPORT
  void net_put32 (unsigned char *dst, uint32_t src);

/*****************************************************************************/

LIBGIFT_EXPORT
  int net_sock_adj_buf (int fd, int buf_name, float factor);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __NETWORK_H */
