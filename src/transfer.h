/*
 * $Id: transfer.h,v 1.58 2003/11/12 22:14:28 jasta Exp $
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

#ifndef __TRANSFER_H
#define __TRANSFER_H

/*****************************************************************************/

#include "plugin/protocol.h"
#if 0
#include "plugin/if_event_api.h"
#include "plugin/transfer_api.h"
#endif

#include "if_event.h"

#include "lib/stopwatch.h"
#include "lib/parse.h"

/*****************************************************************************/

#define INCOMING_PATH(args) \
	transfer_output_path ("download/incoming", "incoming", stringf args)

#define COMPLETED_PATH(args) \
	transfer_output_path ("download/completed", "completed", stringf args)

/* runtime switches determine whether or not this code is actually used */
#define THROTTLE_ENABLE

#ifdef THROTTLE_ENABLE
# define THROTTLE_TIME     (1*SECONDS)
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

/* opaque type for the upload queue */
struct upload_queue;

/*
 * WARNING: the entire transfer subsystem is actively being rewritten in
 * another repository not available here (my local repository).  Before you
 * make _ANY_ changes here please consult jasta first to discuss the changes
 * that have already been done.
 *
 * You can safely assume that I am displeased with just about everything
 * here.  I can't stress this enough: this entire system is completely
 * fucked.  If you read this, please please please bother me to finish the
 * rewrite.
 */
typedef struct transfer
{
	IFEvent      *event;

	TransferType  type;                /* the type of the transfer */
	char         *filename;            /* the name of the save file */
	char         *path;                /* fully qualified local path to this
	                                    * file */

	char         *hash;                /* protocol-specific hash to reference
										* this transfer */
	int           active;              /* inactive when first created with 0
										* sources */

	off_t         transmit;            /* total received */
	off_t         transmit_change;     /* same as old but for different purposes
	                                    * NOTE: this is temporary */
	off_t         total;               /* total filesize to download */
	off_t         max_seek;            /* current filesize according to the
										* filesystem */

	StopWatch    *sw;                  /* throughput calculations */

	int           flag;                /* arbitrary state flag...used by
	                                    * downloads for save_state */

	char         *state;               /* last reported transfer state (sum of
	                                    * all chunks basically) */

	List         *chunks;              /* partitioned download segments */
	List         *sources;             /* sources used for downloading */

	FILE         *f;                   /* read/write file descriptor */

	/* DOWNLOAD SPECIFIC */
	struct download
	{
		char         *uniq;            /* unique identifer, see uniq_file in
		                                * download.c */
		char         *state_path;
		unsigned char paused : 1;      /* see SOURCE_PAUSED */
		unsigned char verifying : 1;   /* bla bla, temporary */
	} down;

	/* UPLOAD SPECIFIC */
	struct upload
	{
		/* temporarily hide this transfer from the interface protocol (until its
		 * been completely filled)
		 * NOTE: this is no longer in use */
		unsigned char        display : 1;

		/* special upload (eg authorized by protocol, not giFT) */
		unsigned char        shared : 1;

		/* position in the upload queue */
		struct upload_queue *queue;
	} up;
} Transfer;

/* the rewritten interface uses something similar, so we will wrap it here
 * for the new download_state.[ch] taken from the rewrite */
#define DOWNLOAD(transfer) (&((transfer)->down))
#define UPLOAD(transfer)   (&((transfer)->up))

/*****************************************************************************/

Transfer  *transfer_new             (TransferType direction, char *filename,
                                     char *hash, off_t size);
void       transfer_free            (Transfer *transfer);
void       transfer_pause           (Transfer *transfer);
void       transfer_unpause         (Transfer *transfer);
void       transfer_stop            (Transfer *transfer, int cancel);

int          transfer_user_cmp      (Source *source, char *user);
unsigned int transfer_length        (List *tlist, char *user, int active);

char      *transfer_output_path     (char *key, char *name, char *filename);
void       transfer_log             (Transfer *transfer, char *fmt, ...);

#ifdef THROTTLE_ENABLE
int        transfers_resume         (List *transfers, size_t max_bw,
                                     size_t *r_credits);
#endif

/*****************************************************************************/

Source    *source_new               (char *user, char *hash,
                                     off_t size, char *url);
void       source_free              (Source *source);
int        source_cmp               (Source *a, Source *b);
char      *source_status            (Source *source);
char      *source_status_proto      (Source *source);
void       source_status_set        (Source *source, SourceStatus status,
                                     char *text);

/*****************************************************************************/

Chunk     *chunk_new                (Transfer *transfer, Source *source,
                                     off_t start, off_t stop);
void       chunk_write              (Chunk *chunk, unsigned char *segment,
                                     size_t len);
void       chunk_free               (Chunk *chunk);
void       chunk_cancel             (Chunk *chunk);
size_t     chunk_throttle           (Chunk *chunk, size_t max_bw,
                                     size_t len, size_t credits);
/*****************************************************************************/

#endif /* __TRANSFER_H */
