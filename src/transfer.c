/*
 * $Id: transfer.c,v 1.95 2003/11/12 22:21:17 jasta Exp $
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

#include "plugin/protocol.h"
#include "plugin/share.h"
#include "plugin.h"

#include "lib/event.h"
#include "lib/file.h"

#include "transfer.h"

#include "download.h"
#include "upload.h"

#include "if_transfer.h"

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

/* laziness.  this should be typedef'd */
void download_write (Chunk *chunk, unsigned char *segment, size_t len);
void upload_write   (Chunk *chunk, unsigned char *segment, size_t len);

/*****************************************************************************/

Transfer *transfer_new (TransferType direction,
                        char *filename, char *hash, off_t size)
{
	Transfer *transfer;

	if (!(transfer = MALLOC (sizeof (Transfer))))
		return NULL;

	transfer->type            = direction;
	transfer->filename        = STRDUP (filename);
	transfer->hash            = STRDUP (hash);
	transfer->transmit        = 0;
	transfer->transmit_change = 0;
	transfer->total           = size;
	transfer->max_seek        = 0;
	transfer->sw              = stopwatch_new (TRUE);

	UPLOAD(transfer)->shared  = TRUE;

	return transfer;
}

void transfer_free (Transfer *transfer)
{
	List *ptr;

	assert (transfer);

	for (ptr = transfer->chunks; ptr; ptr = list_next (ptr))
		chunk_free (ptr->data);

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
		source_free (ptr->data);

	list_free (transfer->chunks);
	list_free (transfer->sources);

	free (transfer->hash);
	free (DOWNLOAD(transfer)->uniq);
	free (transfer->filename);
	free (transfer->path);
	free (DOWNLOAD(transfer)->state_path);

#ifdef TRANSFER_LOG
	free (transfer->log_path);
#endif

	free (transfer->state);

	stopwatch_free (transfer->sw);

	if (transfer->f)
		fclose (transfer->f);

#if 0
	timer_remove (transfer->timer);
#endif

	/* NOTE: this is probably going to be NULL here as the transfer direction
	 * subsystem will probably clean it up before it removes the respective
	 * transfer list */
	if_event_finish (transfer->event);

	free (transfer);
}

/*****************************************************************************/

void transfer_pause (Transfer *transfer)
{
	if (!transfer || DOWNLOAD(transfer)->paused)
		return;

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		download_pause (transfer);
		break;
	 default:
		break;
	}

	if_transfer_change (transfer->event, TRUE);
}

void transfer_unpause (Transfer *transfer)
{
	if (!transfer || !DOWNLOAD(transfer)->paused)
		return;

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		download_unpause (transfer);
		break;
	 default:
		break;
	}

	if_transfer_change (transfer->event, TRUE);
}

void transfer_stop (Transfer *transfer, int cancel)
{
	if (!transfer)
		return;

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		download_stop (transfer, cancel);
		break;
	 case TRANSFER_UPLOAD:
		upload_stop (transfer, cancel);
		break;
	}
}

/*****************************************************************************/

int transfer_user_cmp (Source *source, char *user)
{
	Protocol *p;

	if (!source || !source->p)
		return -1;

	p = source->p;

	return p->user_cmp (p, source->user, user);
}

/* NOTE: "active" here means the protocol is actually doing something
 *       with the transfer, rather than SOURCE_ACTIVE */
unsigned int transfer_length (List *tlist, char *user, int active)
{
	List *ptr;
	unsigned int length = 0;

	/* <transfers> */
	for (; tlist; tlist = list_next (tlist))
	{
		Transfer *transfer = tlist->data;

		/* <chunks> */
		for (ptr = transfer->chunks; ptr; ptr = list_next (ptr))
		{
			Chunk *chunk = ptr->data;

			if (!chunk || !chunk->source)
				continue;

			/* only non-queued sources be should counted */
			if (active && chunk->source->status == SOURCE_QUEUED_LOCAL)
				continue;

			/* only transfers from this user should be counted */
			if (user)
			{
				if (transfer_user_cmp (chunk->source, user))
					continue;
			}

			length++;
		}
		/* </chunks> */
	}
	/* </transfers> */

	return length;
}

/*****************************************************************************/

/* determine the path to store the incoming/completed files, given the
 * filename destined for the path */
char *transfer_output_path (char *key, char *name, char *file)
{
	char *dir;
	char *fqp = NULL;

	/* file is memory from stringf */
	if (!(file = STRDUP (file)))
		return NULL;

	key = stringf ("%s=%s", key, gift_conf_path (name));
	assert (key != NULL);

	/* this is an inefficient mess, please shoot me */
	if ((dir = file_expand_path (config_get_str (gift_conf, key))))
	{
		char *tmp_host;

		/* combine the directory path and filename */
		fqp = stringf ("%s/%s", dir, file);
		assert (fqp != NULL);

		/* and then make a copy and convert it to the host-order */
		tmp_host = file_host_path (fqp);
		assert (tmp_host != NULL);

		/* and write it right back into stringf's static buffer */
		fqp = stringf ("%s", tmp_host);
		assert (fqp != NULL);

		/* and now cleanup our completely wasteful strdup copy */
		free (tmp_host);
	}

	free (file);

	/* make sure this path exists */
	file_create_path (fqp, 0755);

	return fqp;
}

/*****************************************************************************/

GIFTD_EXPORT
TransferType transfer_direction (Transfer *t)
{
	assert (t != NULL);

	return t->type;
}

/*****************************************************************************/

#ifdef THROTTLE_ENABLE
/*
 * Just a stupid little helper so that suspend and resume do not have too
 * much needless code duplication.
 */
static Protocol *suspend_or_resume (Transfer *transfer, Chunk *chunk,
                                    Source *source)
{
	if (!chunk || !source)
		return NULL;

	return source->p;
}

static void suspend_op (Transfer *transfer, Chunk *chunk, Source *source)
{
	Protocol *p;

	if (!(p = suspend_or_resume (transfer, chunk, source)))
		return;

	chunk->suspended = TRUE;
	p->chunk_suspend (p, transfer, chunk, source);
}

static void resume_op (Transfer *transfer, Chunk *chunk, Source *source)
{
	Protocol *p;

	if (!(p = suspend_or_resume (transfer, chunk, source)))
		return;

	chunk->suspended = FALSE;
	p->chunk_resume (p, transfer, chunk, source);
}

int transfers_resume (List *transfers, size_t max_bw, size_t *r_credits)
{
	List          *ptr, *cptr;
	Transfer      *trans;
	Chunk         *chunk;
	Source        *source;

	/* resume the individual chunks */
	for (ptr = transfers; ptr; ptr = list_next (ptr))
	{
		trans = ptr->data;

		for (cptr = trans->chunks; cptr; cptr = list_next (cptr))
		{
			chunk = cptr->data;

			source = chunk->source;

			if (!source)
				continue;

			if (chunk->suspended)
				resume_op (trans, chunk, source);

			assert (chunk->suspended == FALSE);
		}
	}

	/* recredit all transfers */
	*r_credits = max_bw * THROTTLE_TIME / SECONDS;

	return TRUE;
}
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

Source *source_new (char *user, char *hash, off_t size, char *url)
{
	Protocol *p = NULL;
	char     *ptr;
	Source   *source;

	/* hash may be NULL */
	if (!user || !url)
		return NULL;

	/* locate the protocol from the url */
	if ((ptr = strchr (url, ':')))
	{
		char  *buf;
		size_t buf_len;

		buf_len = ptr - url;

		/* allocate a buffer to hold the protocol name for lookup */
		if (!(buf = malloc (buf_len + 1)))
			return NULL;

		gift_strncpy (buf, url, buf_len);

		if (!(p = plugin_lookup (buf)))
			GIFT_WARN (("unable to lookup protocol %s: not loaded", buf));

		free (buf);
	}

	/* allocate the Source structure */
	if (!(source = MALLOC (sizeof (Source))))
		return NULL;

	source->chunk  = NULL;
	source->user   = STRDUP (user);
	source->hash   = STRDUP (hash);
	source->url    = STRDUP (url);
	source->p      = p;
	source->udata  = NULL;

	return source;
}

void source_free (Source *source)
{
	if (!source)
		return;

	if (source->chunk)
		source->chunk->source = NULL;

	free (source->user);
	free (source->hash);
	free (source->url);
	free (source->status_data);

	free (source);
}

int source_cmp (Source *a, Source *b)
{
	int ret = 0;

	if (!a || !a->p)
		return -1;

	if (!b || !b->p)
		return 1;

	/* make sure they both belong to the same protocol */
	if ((ret = STRCMP (a->p->name, b->p->name)))
		return ret;

	return a->p->source_cmp (a->p, a, b);
}

char *source_status (Source *source)
{
	char *status = NULL;

	if (!source)
		return NULL;

	switch (source->status)
	{
	 case SOURCE_UNUSED:
	 case SOURCE_WAITING:        status = "Waiting";              break;
	 case SOURCE_PAUSED:         status = "Paused";               break;
	 case SOURCE_QUEUED_REMOTE:  status = "Queued (Remotely)";    break;
	 case SOURCE_QUEUED_LOCAL:   status = "Queued";               break;
	 case SOURCE_ACTIVE:         status = "Active";               break;
	 case SOURCE_COMPLETE:       status = "Complete";             break;
	 case SOURCE_CANCELLED:      status = "Cancelled (Remotely)"; break;
	 case SOURCE_TIMEOUT:        status = "Timed out";            break;
	 default:                                                     break;
	}

	return status;
}

char *source_status_proto (Source *source)
{
	char *status;

	if (!source)
		return NULL;

	/*
	 * Grab the protocol-specific status, or fall back to the normal status
	 * value if nothing extra was provided.
	 */
	if (!(status = source->status_data))
		status = source_status (source);

	return status;
}

void source_status_set (Source *source, SourceStatus status, char *text)
{
	if (!source)
		return;

	free (source->status_data);
	source->status = status;
	source->status_data = STRDUP (text);

#if 0
	if (!source->chunk || !source->chunk->transfer)
		GIFT_TRACE (("source->chunk = %p", source->chunk));
#endif

	/* gotta love these goofy data structures */
	if (source->chunk && source->chunk->transfer)
		if_transfer_change (source->chunk->transfer->event, TRUE);
}

/*****************************************************************************/

Chunk *chunk_new (Transfer *transfer, Source *source,
                  off_t start, off_t stop)
{
	Chunk *chunk;

	assert (start >= 0);
	assert (stop <= transfer->total);

	if (!(chunk = MALLOC (sizeof (Chunk))))
		return NULL;

	chunk->transfer     = transfer;
	chunk->source       = source;
	chunk->udata        = NULL;

	chunk->transmit     = 0;
	chunk->start        = start;
	chunk->stop         = stop;

	/* activate this source as well */
	if (source)
		source->chunk = chunk;

	transfer->chunks = list_append (transfer->chunks, chunk);

	return chunk;
}

void chunk_free (Chunk *chunk)
{
	if (!chunk)
		return;

	if (chunk->source)
		chunk->source->chunk = NULL;

	/* source_free (chunk->source); */
	free (chunk);
}

#ifdef THROTTLE_ENABLE
size_t chunk_throttle (Chunk *chunk, size_t max_bw, size_t len, size_t credits)
{
	assert (len != 0);

	/* ignore bandwidth limiting if the limit is zero */
	if (max_bw == 0)
		return len;

	/*
	 * If the bw limit is low, make the transfer len
	 * smaller.
	 *
	 * This helps keep somewhat equal rates with multiple
	 * chunks, even with a small limit.
	 */
	if (max_bw < (4 * len))
		len = max_bw / 8 + 1;

	if (len > credits)
		len = credits;

	if (len == 0)
	{
		/* suspend this chunk for the rest of THROTTLE_TIME */
		suspend_op (chunk->transfer, chunk, chunk->source);
		assert (chunk->suspended);
	}

	return len;
}
#endif /* THROTTLE_ENABLE */

void chunk_write (Chunk *chunk, unsigned char *segment, size_t len)
{
	Transfer *transfer = chunk->transfer;

	assert (chunk);
	assert (transfer);

	assert (transfer->type == TRANSFER_DOWNLOAD ||
	        transfer->type == TRANSFER_UPLOAD);

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD: download_write (chunk, segment, len); break;
	 case TRANSFER_UPLOAD:     upload_write (chunk, segment, len); break;
	}
}
