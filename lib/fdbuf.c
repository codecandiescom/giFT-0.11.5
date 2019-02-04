/*
 * $Id: fdbuf.c,v 1.6 2003/09/16 19:46:27 jasta Exp $
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

#include "strobj.h"

#include "fdbuf.h"

#include <errno.h>

/*****************************************************************************/

#define STROBJ(buf) ((buf)->s)

/*****************************************************************************/

static int fdbuf_read_def (int fd, void *buf, size_t len, void *udata)
{
	return recv (fd, buf, len, 0);
}

static int fdbuf_peek_def (int fd, void *buf, size_t len, void *udata)
{
	return recv (fd, buf, len, MSG_PEEK);
}

FDBuf *fdbuf_new (int fd, FDBufRead readfn, FDBufRead peekfn, void *udata)
{
	FDBuf *buf;

	if (!(buf = MALLOC (sizeof (FDBuf))))
		return NULL;

	if (!(buf->s = string_new (NULL, FALSE, 0, TRUE)))
	{
		free (buf);
		return NULL;
	}

	if (!readfn)
		readfn = fdbuf_read_def;

	if (!peekfn)
		peekfn = fdbuf_peek_def;

	buf->fd     = fd;
	buf->readfn = readfn;
	buf->peekfn = peekfn;
	buf->udata  = udata;

	return buf;
}

void fdbuf_free (FDBuf *buf)
{
	if (!buf)
		return;

	string_free (STROBJ(buf));
	free (buf);
}

/*****************************************************************************/

/* translate a return value from the read or peek function into a suitable
 * error code for this interface */
static int fdbuf_err (int read_ret)
{
	int ret = 0;

	if (read_ret < 0)
	{
		/* TODO: provide an errfn interface */
		switch (platform_net_errno())
		{
		 case EAGAIN:        ret = FDBUF_AGAIN;  break;
		 default:            ret = FDBUF_ERR;    break;
		}
	}
	else if (read_ret == 0)
	{
		/* EOF can't return 0 here, as that is reserved for successful
		 * completion of the fill operation. */
		ret = FDBUF_EOF;
	}

	return ret;
}

int fdbuf_fill (FDBuf *buf, int bufsize)
{
	unsigned char data[2048];
	int rlen, rem, n, ret;

	if (!buf)
		return FDBUF_NVAL;

	/* remaining bytes to read */
	rem = (bufsize - STROBJ(buf)->len);

	/* we already have what you asked for! */
	if (rem <= 0)
		return 0;

	rlen = MIN (rem, sizeof (data));

	if ((n = buf->readfn (buf->fd, data, rlen, buf->udata)) <= 0)
		ret = fdbuf_err (n);
	else
	{
		/* write this onto the data buffer */
		if (!string_appendu (STROBJ(buf), data, n))
			ret = FDBUF_ERR;
		else
			ret = MAX (0, (rem - n));
	}

	return ret;
}

static int find_delim (unsigned char *data, size_t len, char *delim)
{
	size_t delim_len;
	int doff;

	assert (data != NULL);
	assert (len > 0);

	delim_len = strlen (delim);
	assert (delim_len > 0);

	for (doff = 0; doff < len; doff++)
	{
		if (memcmp (data + doff, delim, delim_len) == 0)
			return (doff + delim_len);
	}

	return -1;
}

int fdbuf_delim (FDBuf *buf, char *delim)
{
	unsigned char data[2048];
	int n, ret = 0;

	if (!buf || !delim)
		return FDBUF_NVAL;

	/* TODO: FIXME */
	assert (buf->peekfn != NULL);

	if ((n = buf->peekfn (buf->fd, data, sizeof (data), buf->udata)) <= 0)
		ret = fdbuf_err (n);
	else
	{
		int doff;
		int dlen;

		/* locate the delim offset, and total length to push off the queue */
		doff = find_delim (data, n, delim);
		dlen = (doff >= 0) ? (doff) : (n);

		/* push off the remainder of the data */
		if ((n = buf->readfn (buf->fd, data, dlen, buf->udata)) <= 0)
			ret = fdbuf_err (n);
		else
		{
			/*
			 * Successful read, append to the internal buffer.
			 *
			 * NOTE:
			 *
			 * The return value here is left alone (which is 0, success) unless
			 * the delim was unable to be found.  The return scheme here is
			 * that the number of bytes read are returned unless the delim
			 * was found, in which case the 0 will be returned.
			 */
			if (!string_appendu (STROBJ(buf), data, n))
				ret = FDBUF_ERR;
			else if (doff < 0)
				ret = n;
		}
	}

	/* done so soon? */
	return ret;
}

void fdbuf_release (FDBuf *buf)
{
	if (!buf)
		return;

	/* reset the length so subsequent access will overwrite the previous
	 * data, and effectively reterminate the buffer */
	STROBJ(buf)->len = 0;
}

/*****************************************************************************/

unsigned char *fdbuf_data (FDBuf *buf, size_t *len)
{
	if (!buf)
		return NULL;

	if (len)
		*len = (size_t)STROBJ(buf)->len;

	return ((unsigned char *)STROBJ(buf)->str);
}

/*****************************************************************************/

#if 0
unsigned char fake_data[] =
{
	'\r', '\n',
	'f', 'o', 'o', '\n',
	0xde, 0xad, 0xbe, 0xef
};

unsigned int  fake_offs   = 0;
static int    fake_fd     = 0;

static int fake_peek (int fd, void *buf, size_t len)
{
	assert (fd == fake_fd);

	/* do not read more than is available */
	len = CLAMP (len, 0, (sizeof (fake_data) - fake_offs));

	/* make this VERY inefficient to try to find holes */
	if (len > 1)
		len = 1;

	/* EOF */
	if (len <= 0)
		return 0;

	assert (fake_offs + len <= sizeof (fake_data));

	memcpy (buf, fake_data + fake_offs, len);

	return len;
}

static int fake_read (int fd, void *buf, size_t len)
{
	int ret;

	if ((ret = fake_peek (fd, buf, len)) > 0)
		fake_offs += ret;

	return ret;
}

int main (int argc, char **argv)
{
	FDBuf         *buf;
	unsigned char *data;
	unsigned char  delim[2];
	int            ret;

	if (!(buf = fdbuf_new (fake_fd, fake_read, fake_peek)))
		return 1;

 retry1:

	if ((ret = fdbuf_fill (buf, 2)) < 0)
	{
		printf ("error\n");
		exit (1);
	}

	if (ret > 0)
		goto retry1;

	data = fdbuf_data (buf);
	assert (data != NULL);

	assert (strncmp ((char *)data, (char *)fake_data, 2) == 0);
	delim[0] = data[1];
	delim[1] = 0;

	fdbuf_release (buf);

 retry2:

	if ((ret = fdbuf_delim (buf, delim)) < 0)
	{
		printf ("another error\n");
		exit (1);
	}

	if (ret > 0)
		goto retry2;

	data = fdbuf_data (buf);
	assert (strncmp ((char *)data, (char *)(fake_data + 2), 4) == 0);

	fdbuf_free (buf);

	return 0;
}
#endif
