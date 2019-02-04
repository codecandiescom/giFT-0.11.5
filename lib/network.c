/*
 * $Id: network.c,v 1.33 2003/12/09 10:22:13 hipnod Exp $
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

#include "libgift.h"

#include "network.h"

#ifndef WIN32
# include <errno.h>
typedef void* optval_t;
#else /* WIN32 */
typedef char* optval_t;
#endif /* !WIN32 */

#ifndef SHUT_RDWR
# define SHUT_RDWR 2
#endif /* SHUT_RDWR */

static int default_buf_size = -1;

/**************************************************************************/

void net_close (int fd)
{
	if (fd < 0)
		return;

#ifndef WIN32
	shutdown (fd, SHUT_RDWR);
	close (fd);
#else /* !WIN32 */
	shutdown (fd, SD_BOTH);
	closesocket (fd);
#endif /* WIN32 */
}

int net_connect (const char *ip, in_port_t port, BOOL blocking)
{
	int s_fd;
	struct sockaddr_in server;

	if (!ip || !port)
		return -1;

	if ((s_fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
		return s_fd;

	memset (&server, 0, sizeof (struct sockaddr_in));
	server.sin_family      = AF_INET;
	server.sin_port        = htons (port);
	server.sin_addr.s_addr = net_ip (ip);

	net_set_blocking (s_fd, blocking);

	if (connect (s_fd, (struct sockaddr *)&server, sizeof (struct sockaddr)) < 0 &&
#ifndef WIN32
	    errno != EINPROGRESS)
#else /* WIN32 */
	    WSAGetLastError () != WSAEWOULDBLOCK)
#endif /* !WIN32 */
	{
		net_close (s_fd);
		return -1;
	}

	return s_fd;
}

int net_accept (int s_fd, int blocking)
{
	int accept_sock, len;
	struct sockaddr_in saddr;

	len = sizeof (saddr);

	accept_sock = accept (s_fd, (struct sockaddr *)&saddr, &len);

	net_set_blocking (accept_sock, blocking);

	return accept_sock;
}

int net_bind (in_port_t port, BOOL blocking)
{
	int s_fd, len;
	struct sockaddr_in server;

	if (port == 0)
		return -1;

	if ((s_fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
		return s_fd;

	memset (&server, 0, sizeof (server));
	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = htonl (INADDR_ANY);
	server.sin_port        = htons (port);

	/* set something non-zero */
	len = sizeof (server);

	setsockopt (s_fd, SOL_SOCKET, SO_REUSEADDR, (optval_t) &len, sizeof (len));

	net_set_blocking (s_fd, blocking);

	if (bind (s_fd, (struct sockaddr *)&server, sizeof (struct sockaddr)) < 0)
	{
		GIFT_ERROR (("bind: %s", platform_net_error()));
		net_close (s_fd);
		return -1;
	}

	listen (s_fd, 5);

	return s_fd;
}

/*****************************************************************************/

int net_set_blocking (int s_fd, BOOL blocking)
{
#ifndef WIN32
	int flags;

	flags = fcntl (s_fd, F_GETFL);

	if (blocking)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;

	fcntl (s_fd, F_SETFL, flags);

	return flags;
#else /* WIN32 */
	unsigned long arg = !blocking;

	ioctlsocket (s_fd, FIONBIO, &arg);

	return arg;
#endif /* !WIN32 */
}

/*****************************************************************************/

int net_sock_error (int s_fd)
{
	int err, len, x;

	len = sizeof (err);

	/* if getsockopt errors out, we can safely assume there are errors
	 * on s_fd :) */
	if ((x = getsockopt (s_fd, SOL_SOCKET, SO_ERROR, (optval_t) &err, &len)) < 0)
		return x;

	/* this is quite probably a very very bad idea */
	if (err > 0)
	{
		errno = err;
		return err;
	}

	/* no errors on the socket */
	return 0;
}

int net_sock_adj_buf (int s_fd, int buf_name, float factor)
{
	int buf_size;
	int len;

	if (s_fd < 0)
		return -1;

	len = sizeof (buf_size);

	if (default_buf_size == -1)
	{
		if (getsockopt (s_fd, SOL_SOCKET, buf_name,
		                (optval_t) &default_buf_size, &len) < 0)
		{
			default_buf_size = RW_SOCKBUFFER;
		}
	}

	buf_size = CLAMP ((default_buf_size * factor), 0, default_buf_size);

	if (buf_size < 0)
		return -1;

	/* don't error check these, it's just going to annoy people ;) */
	setsockopt (s_fd, SOL_SOCKET, buf_name, (optval_t) &buf_size, len);

#if 0
	/* check to make sure it succeeded */
	getsockopt (s_fd, SOL_SOCKET, buf_name, (optval_t) &buf_size, &len);
#endif

	return buf_size;
}

/*****************************************************************************/

int net_send (int fd, const char *data, size_t len)
{
	int flags = 0;

	if (len == 0)
		len = strlen (data);

	/* debugging simplicity ... SIGPIPE is still SIG_IGN */
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif

	return send (fd, (const void *)data, len, flags);
}

in_addr_t net_ip (const char *ip_str)
{
	if (!ip_str)
		return 0;

	return inet_addr (ip_str);
}

static char *get_ip_str (in_addr_t ip)
{
	struct in_addr ina;

	memset (&ina, 0, sizeof (ina));
	ina.s_addr = ip;

	return inet_ntoa (ina);
}

#ifdef NET_IP_STRBUF
char *net_ip_strbuf (in_addr_t ip, char *buf, size_t size)
{
	char *ipstr;

	if (!(ipstr = get_ip_str (ip)))
		return NULL;

	memcpy (buf, ipstr, MIN (STRLEN_0(ipstr), size));
	return buf;
}
#endif /* NET_IP_STRBUF */

char *net_ip_str (in_addr_t ip)
{
#ifdef NET_IP_STRBUF
	static char ipbuf[32];
	return net_ip_strbuf (ip, ipbuf, sizeof (ipbuf));
#else /* !NET_IP_STRBUF */
	return get_ip_str (ip);
#endif /* NET_IP_STRBUF */
}

in_addr_t net_peer (int fd)
{
	struct sockaddr_in saddr;
	in_addr_t ip  = 0;
	int       len = sizeof (saddr);

	if (getpeername (fd, (struct sockaddr *)&saddr, &len) == 0)
		ip = saddr.sin_addr.s_addr;

	return ip;
}

char *net_peer_ip (int fd)
{
	return net_ip_str (net_peer (fd));
}

in_addr_t net_local_ip (int fd, in_port_t *portret)
{
	struct sockaddr_in saddr;
	in_addr_t ip   = 0;
	in_port_t port = 0;
	int       len  = sizeof (saddr);

	/* similar to getpeername, but grabs our portion of the socket */
	if (getsockname (fd, (struct sockaddr *)&saddr, &len) == 0)
	{
		ip   = saddr.sin_addr.s_addr;
		port = ntohs (saddr.sin_port);
	}

	if (portret)
		*portret = port;

	return ip;
}

/*****************************************************************************/

/* returns a netmask given the desired bitwidth */
in_addr_t net_mask (int bitwidth)
{
	in_addr_t mask;

	for (mask = 0; bitwidth > 0; bitwidth--)
	{
		/* TODO - hardcoding size is evil */
		mask |= (1 << (32 - bitwidth));
	}

	mask = htonl (mask);

	return mask;
}

/*****************************************************************************/

/* match may be in the form 192.168.0.0/16 or "LOCAL" */
BOOL net_match_host (in_addr_t ip, char *match)
{
	char *host;
	char *block;
	unsigned short bitwidth;
	in_addr_t      host_ip = 0;

	/* this is a bad idea */
	if (!strcasecmp (match, "ALL"))
		return TRUE;

	/* special case to match all local addresses */
	if (!strcasecmp (match, "LOCAL"))
	{
		/* 0.0.0.0 */
		if (!ip)
			return TRUE;

		/* convert to host order to make things easier and more efficient
		 * below */
		ip = ntohl (ip);

		if (((ip & 0xff000000) == 0x7f000000) || /* 127.0.0.0 */
		    ((ip & 0xffff0000) == 0xc0a80000) || /* 192.168.0.0 */
		    ((ip & 0xfff00000) == 0xac100000) || /* 172.16-31.0.0 */
		    ((ip & 0xff000000) == 0x0a000000))   /* 10.0.0.0 */
		{
			return TRUE;
		}

		return FALSE;
	}

	/* we need to parse 152.32.0.0[/16] */
	host = string_sep (&match, "/");
	bitwidth = (match && *match) ? ATOI (match) : 32;

	/* TODO - this won't work with incomplete IP addresses */
	while ((block = string_sep (&host, ".")))
		host_ip = (host_ip << 8 | (ATOI (block) & 0xff));

	/* convert to network order */
	host_ip = htonl (host_ip);

	/* apply the netmask */
	ip      &= net_mask (bitwidth);
	host_ip &= net_mask (bitwidth);

	/* compare */
	return BOOL_EXPR (ip == host_ip);
}

/*****************************************************************************/

#define NET_GET(src,tohost,tohostfunc,sizetype,relsizetype) \
	sizetype dst; \
	memcpy (&dst, src, sizeof (sizetype)); \
	return ((tohost) ? (sizetype)tohostfunc((relsizetype)dst) : dst);

uint8_t net_get8 (unsigned char *src)
{
	NET_GET (src, FALSE, ntohl, uint8_t, unsigned char);
}

uint16_t net_get16 (unsigned char *src, int tohost)
{
	NET_GET (src, tohost, ntohs, uint16_t, unsigned short);
}

uint32_t net_get32 (unsigned char *src, int tohost)
{
	NET_GET (src, tohost, ntohl, uint32_t, unsigned long);
}

#define NET_PUT(dst,src) \
	memcpy (dst, &src, sizeof (src));

void net_put8 (unsigned char *dst, uint8_t src) { NET_PUT (dst, src); }
void net_put16 (unsigned char *dst, uint16_t src) { NET_PUT (dst, src); }
void net_put32 (unsigned char *dst, uint32_t src) { NET_PUT (dst, src); }
