/*
 * $Id: transfer_api.h,v 1.10 2003/12/23 03:54:39 jasta Exp $
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

#ifndef __TRANSFER_API_H
#define __TRANSFER_API_H

/*****************************************************************************/

/**
 * @file transfer_api.h
 *
 * @brief Defines structures and API necessary to work with Transfer,
 *        Chunk, and Source.
 *
 * This API is required as protocol.h makes reference to these types.
 */

/*****************************************************************************/

typedef enum
{
	TRANSFER_DOWNLOAD,
	TRANSFER_UPLOAD
} TransferType;

struct transfer;

/* leave the type opaque for now... */
#ifdef GIFT_PLUGIN
typedef struct transfer Transfer;
#endif /* GIFT_PLUGIN */

/*****************************************************************************/

/**
 * Interface structure for the Protocol::upload_auth method.  See
 * documentation there for more information how this is used to communicate
 * extra auth data.
 */
typedef struct
{
	/**
	 * @name UPLOAD_AUTH_MAX
	 */
	unsigned int queue_pos;            /**< Current position in the fair
	                                    *   upload queue */
	unsigned int queue_ttl;            /**< Number of queued uploads */
} upload_auth_t;

/*****************************************************************************/

/**
 * Defines status groups that giFT can use to modify the transfer logic.
 * Protocols are required to set an appropriate grouping with
 * p->source_status (p, ...).
 */
typedef enum source_status
{
	SOURCE_UNUSED = 0,                 /**< this source is unused and
	                                    *   (possibly) waiting to be placed */
	SOURCE_PAUSED,                     /**< source has been explicitly paused
	                                    *   by user */
	SOURCE_QUEUED_REMOTE,              /**< protocol says the other end is
	                                    *   preventing us from downloading */
	SOURCE_QUEUED_LOCAL,               /**< we are preventing ourselves from
	                                    *   downloading this file */
	SOURCE_COMPLETE,                   /**< last known event was that the
	                                    *   chunk associated complete
	                                    *   successfully, and is now moving
	                                    *   onto another */
	SOURCE_CANCELLED,                  /**< remote end cancelled an active
	                                    *   transfer */
	SOURCE_TIMEOUT,                    /**< date timeout */
	SOURCE_WAITING,                    /**< asked the protocol to download but
	                                    *   haven't received any status back */
	SOURCE_ACTIVE,                     /**< set once data has started coming
	                                    *   in */
} SourceStatus;

/**
 * Defines contact url, hash, and status information.  When actively
 * transferring, the associated chunk will be referenced.
 */
typedef struct source
{
	struct protocol *p;                /**< protocol that controls this
	                                    *   source (extracted from the URL
	                                    *   family) */
	struct chunk    *chunk;            /**< chunk using this source */
	SourceStatus     status;           /**< current status family */
	char            *status_data;      /**< current protocol-specific
	                                    *   status string */
	char            *user;             /**< arbitrary user string supplied
	                                    *   from the plugin */
	char            *hash;             /**< interface protocol hack to include
	                                    *   hash type and hash in the same
	                                    *   string (both provided by the
	                                    *   plugin, but constructed in this
	                                    *   form internally) */
	off_t            size;             /**< total size of the file described
										*   by this source */
	char            *url;              /**< protocol-specific contact url that
	                                    *   can be used to initiate a transfer
	                                    *   of the file described by the
	                                    *   parent transfer structure */
	void            *udata;            /**< Arbitrary plugin-specific user
	                                    *   data */
} Source;

/*****************************************************************************/

/**
 * Arbitrary file division of a single transferring file.  Usedin connection
 * with sources to perform the multi-source downloading without the plugins
 * awareness/interaction.
 */
typedef struct chunk
{
	struct transfer *transfer;         /**< parent transfer that created this
	                                    *   instance */
	Source          *source;           /**< source fulfilling this chunks
	                                    *   data requirements */

	off_t            start;            /**< beginning offset in the file,
	                                    *   please note that if you are asked
	                                    *   to begin downloading from a chunk,
	                                    *   you must start at start+transmit
	                                    *   as start represents the actual
	                                    *   division made by giFT */
	off_t            stop;             /**< ending offset in the file */
	unsigned char    stop_change : 1;  /**< stop was moved...protocol might
	                                    *   need to know this, hopefully this
	                                    *   will be cleaned up in the future */

	unsigned char    suspended   : 1;  /**< chunk was suspended for the
	                                    *   purpose of userspace throttling */
/**
 * The following members are from old throttling code and are no longer being
 * used.  They are being added to the structure for binary compatibility only.
 */
#if 1
	float            adjust_dep;
#endif

 	off_t            transmit;         /**< number of recvd or sent bytes */

#if 1
	off_t            transmit_old_dep;
	unsigned long    throughput_dep;
#endif

 	unsigned long    tmp_recv;         /**< similar to throughput, deprecated */

	int              timeout_cnt;      /**< timeout counter (how many seconds
	                                    *   of inactivity exist) */
	int              timeout_max;      /**< maximum number of timeout counts
	                                    *   until an action is performed */

	void            *udata;            /**< protocol-specific user data */
} Chunk;

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Simple accessor for the transfer data structure to retrieve the direction.
 * This is generally useful for plugins because there is little data
 * structure distinction between a download and an upload.
 *
 * Exported by giFT, not libgiftproto.  This is not correct, and will change
 * in the future.
 */
GIFTD_EXPORT
  TransferType transfer_direction (struct transfer *t);

/*****************************************************************************/

/*
 * These will move to Protocol:: someday.  These are also exported by giFT,
 * not libgiftproto.
 */
GIFTD_EXPORT
 size_t download_throttle (Chunk *chunk, size_t len);
GIFTD_EXPORT
 size_t upload_throttle (Chunk *chunk, size_t len);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __TRANSFER_API_H */
