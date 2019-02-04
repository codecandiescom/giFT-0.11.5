/*
 * $Id: fdbuf.h,v 1.7 2003/10/22 22:35:40 jasta Exp $
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

#ifndef __FDBUF_H
#define __FDBUF_H

/*****************************************************************************/

/**
 * @file fdbuf.h
 *
 * @brief Buffering subsystem used to guarantee completeness of read buffers
 * 	      using non-blocking interfaces.
 *
 * This interface is intended to be used through a TCPC object.  A buffer
 * will be created on ::tcp_open or ::tcp_accept.
 */

/*****************************************************************************/

#define FDBUF_ERR       (-1)           /**< Low-level error, examine errno */
#define FDBUF_AGAIN     (-2)           /**< Convenience for EAGAIN */
#define FDBUF_EOF       (-3)           /**< End-Of-File or remote termination */
#define FDBUF_NVAL      (-4)           /**< Invalid argument */

/**
 * Prototype for the buffer read function.
 *
 * @param fd
 * @param buf
 * @param len
 * @param udata
 *
 * @return Number of bytes successfully read.  Returns 0 on end-of-file and
 *         -1 on error, and errno will be examined by the caller.
 */
typedef int (*FDBufRead) (int fd, void *buf, size_t len, void *udata);

/**
 * Buffering structure.
 */
typedef struct
{
	/**
	 * @name Public
	 */
	int           flag;                /**< Stage counter */
	void         *udata;

	/**
	 * @name Read-only
	 */
	int           fd;                  /**< File descriptor to read */

	/**
	 * @name Private
	 */
	FDBufRead     readfn;              /**< Function to poll more data */
	FDBufRead     peekfn;              /**< Function to peek data (optional) */
	String       *s;                   /**< Currently processed buffer */
} FDBuf;

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Construct a new buffer instance.  This should only be done once per file
 * descriptor, as the object instance must persist in order to take advantage
 * of the buffering.
 *
 * @param fd      File descriptor to read.
 * @param readfn  Optional interface for reading data.  If NULL, socket recv
 *                is used.
 * @param peekfn  Optional interface for peeking data.  If NULL, MSG_PEEK will
 *                be used.
 * @param udata   Arbitrary data passed along to readfn, peekfn, and attached
 *                to the FDBuf object.
 */
LIBGIFT_EXPORT
  FDBuf *fdbuf_new (int fd, FDBufRead readfn, FDBufRead peekfn, void *udata);

/**
 * Destroy a buffer instance.
 */
LIBGIFT_EXPORT
  void fdbuf_free (FDBuf *buf);

/*****************************************************************************/

/**
 * Fill the internal buffer up to bufsize.
 *
 * @param buf
 * @param bufsize  Total number of bytes requested to be filled.
 *
 * @return Number of bytes remaining to fill bufsize.  A return value of 0
 *         indicates that bufsize is completely satisfied.  A negative value
 *         indicates an internal error condition and you may check with the
 *         return codes described above.  The appropriate errno function may
 *         be used to query the low-level error.
 */
LIBGIFT_EXPORT
  int fdbuf_fill (FDBuf *buf, int bufsize);

/**
 * Search for the specific delim string on the input buffers.  This currently
 * requires "peek" functionality on the file descriptor, and as such will
 * only work on sockets.  This is a bug and should be fixed.
 *
 * @param buf
 * @param delim  NUL-terminated delim string.  This is not a set of delims.
 *
 * @return Number of bytes read without locating the delim string or 0 if
 *         delim was found.  Negative return values have the same result as
 *         in ::fdbuf_fill.
 */
LIBGIFT_EXPORT
  int fdbuf_delim (FDBuf *buf, char *delim);

/**
 * Flush the currently populated buffer to begin anew.  This does not
 * invalidate the calling argument, merely the data buffers held within.
 */
LIBGIFT_EXPORT
  void fdbuf_release (FDBuf *buf);

/*****************************************************************************/

/**
 * Accessor for the data segment filled into the buffer.
 *
 * @param buf
 * @param len  Storage location to "return" the length of the data buffer.  If
 *             NULL, no action will be taken on this parameter.
 */
LIBGIFT_EXPORT
  unsigned char *fdbuf_data (FDBuf *buf, size_t *len);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __FDBUF_H */
