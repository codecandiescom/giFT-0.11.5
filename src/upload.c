/*
 * $Id: upload.c,v 1.112 2003/11/12 22:14:28 jasta Exp $
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

#include "lib/event.h"
#include "lib/file.h"

#include "plugin/protocol.h"           /* UPLOAD_AUTH_ */
#include "plugin.h"

#include "share_cache.h"

#include "transfer.h"
#include "upload.h"

#include "if_transfer.h"

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

typedef struct upload_queue
{
	char     *user;                    /* username from queue_hit */
	char     *path;                    /* path to the file requested */
	off_t     len;                     /* length of the transfer allowed */
	size_t    pos;                     /* position in queue */
	BOOL      active;                  /* queue slot is actively transferring */
	timer_id  timer;                   /* timer keeping this object alive */
} tqueue_t;

/*****************************************************************************/

static input_id  upload_timer  = 0;
static List     *uploads       = NULL;
static Array    *uploads_queue = NULL; /* see queue_hit */

/*****************************************************************************/

/*
 * max_uploads:
 *
 *  0    no upload slots available
 * -1    unlimited
 */
static int max_uploads = -1;
static int disabled    = FALSE;

#define MAX_PERUSER_UPLOADS \
	    config_get_int (gift_conf, "sharing/max_peruser_uploads=1")

/*****************************************************************************/

#ifdef THROTTLE_ENABLE

static size_t   max_upstream          = 0;
static size_t   upload_credits        = 0;
static timer_id throttle_resume_timer = 0;

#define MAX_UPLOAD_BW max_upstream

#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

static tqueue_t     *queue_find_inactive (Protocol *p, const char *user,
                                          const char *path);
static unsigned int  queue_hit           (Protocol *p, const char *user,
                                          const char *path, size_t len);
static void          queue_remove        (tqueue_t *queue);
static void          queue_reset         (tqueue_t *queue, time_t interval);

/*****************************************************************************/

static void upload_queue_detach          (Transfer *transfer);
static void upload_queue_attach          (Transfer *transfer, const char *user,
                                          const char *path);
static BOOL upload_queue_write           (Transfer *transfer, size_t len);

/*****************************************************************************/

static int upload_report_progress (Transfer *transfer, void *udata)
{
	if (!UPLOAD(transfer)->display)
	{
		GIFT_TRACE (("son of a bitch"));
		return TRUE;
	}

	if_transfer_change (transfer->event, FALSE);

	return TRUE;
}

static int upload_report (void *udata)
{
	list_foreach (uploads, (ListForeachFunc) upload_report_progress, NULL);
	return TRUE;
}

size_t upload_throttle (Chunk *chunk, size_t len)
{
	off_t rem;

	/* get the number of bytes remaining for this chunk so that we can
	 * clamp len properly */
	rem = chunk->stop - (chunk->start + chunk->transmit);
	len = (size_t)(MIN (rem, len));

#ifdef THROTTLE_ENABLE
	return chunk_throttle (chunk, MAX_UPLOAD_BW, len, upload_credits);
#else
	return len;
#endif /* THROTTLE_ENABLE */
}

/*****************************************************************************/

List *upload_list ()
{
	return uploads;
}

#ifdef THROTTLE_ENABLE
static int throttle_resume (void *arg)
{
	transfers_resume (uploads, MAX_UPLOAD_BW, &upload_credits);
	return TRUE;
}
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

static Source *setup_source (Transfer *t, char *proto,
                             char *user, char *hash, off_t size, char *path)
{
	Source *source;
	char   *url;
#if 0
	BOOL    ret;
#endif

	if (!(url = stringf_dup ("%s://%s:0%s", proto, user, path)))
		return NULL;

	if (!(source = source_new (user, hash, size, url)))
		return NULL;

	/* url is copied by source_new */
	free (url);

#if 0
	assert (source->p != NULL);
	if (!(ret = source->p->source_add (source->p, t, source)))
	{
		source_free (source);
		return NULL;
	}
#endif

	/* push this into the list, should be the only one */
	assert (t->sources == NULL);
	t->sources = list_prepend (t->sources, source);

	return source;
}

static Chunk *setup_chunk (Transfer *t, Source *source, off_t start, off_t stop)
{
	Chunk *chunk;

	chunk = chunk_new (t, source, start, stop);

	/* make sure chunk_new did what we expected */
	assert (chunk->source == source);
	assert (source->chunk == chunk);

	return chunk;
}

static void upload_register (Transfer *t)
{
	/* register the transfer object */
	uploads = list_prepend (uploads, t);

	/* ensure that we have some kind of status reporting */
	if (upload_timer == 0)
	{
		upload_timer =
			timer_add (1 * SECONDS, (TimerCallback)upload_report, NULL);
	}
}

static void notify_avail (ds_data_t *key, ds_data_t *value,
                          unsigned long *avail)
{
	Protocol *p = value->data;

	p->upload_avail (p, *avail);
}

static void send_avail_update (void)
{
	unsigned long avail = upload_availability ();

	plugin_foreach (DS_FOREACH(notify_avail), &avail);
}

/* display is a boolean used to register this data structure but NOT report
 * it to the user interface yet.  this is useful for firewalled push
 * (http_push_file) */
Transfer *upload_new (Protocol *p, char *user, char *hash,
                      char *filename, char *path, off_t start, off_t stop,
                      int display, int shared)
{
	Transfer *transfer;
	Chunk    *chunk;
	Source   *source;
	char     *dup;

	/* legacy crap */
	assert (display == TRUE);
	assert (shared == TRUE);

	/* create the transfer object */
	if (!(transfer = transfer_new (TRANSFER_UPLOAD, filename, hash, stop)))
		return NULL;

	transfer->transmit        = start;
	transfer->transmit_change = start;

	transfer->path            = STRDUP (path);
	UPLOAD(transfer)->display = display;
	UPLOAD(transfer)->shared  = shared;

	/* create and assign the chunk and source values into the transfer so
	 * that this upload behaves similarly to downloads in a data sense */
	if (!(source = setup_source (transfer, p->name, user, hash, stop, path)))
	{
		GIFT_TRACE (("%s: %s(%s): failed construct", hash, user, path));
		transfer_free (transfer);
		return NULL;
	}

	chunk = setup_chunk (transfer, source, start, stop);
	assert (chunk != NULL);

#ifdef THROTTLE_ENABLE
	max_upstream =
	    (size_t)config_get_int (gift_conf, "bandwidth/upstream=0");

	if (throttle_resume_timer == 0 && MAX_UPLOAD_BW > 0)
	{
		throttle_resume_timer =
		    timer_add (THROTTLE_TIME, (TimerCallback) throttle_resume, NULL);

		/* set the initial upload credits */
		upload_credits = max_upstream * THROTTLE_TIME / SECONDS;
	}
#endif /* THROTTLE_ENABLE */

	upload_register (transfer);

	if (display)
	{
		source->status = SOURCE_WAITING;
		send_avail_update ();
	}

	/* make a copy because user may be from stringf() */
	dup = STRDUP (user);

	/* add the if_event */
	if (!(transfer->event = if_transfer_new (NULL, 0, transfer)))
	{
		transfer_free (transfer);
		free (dup);
		return NULL;
	}

	/* attach the transfer to the upload queue */
	upload_queue_attach (transfer, dup, path);

	free (dup);

	return transfer;
}

static void upload_free (Transfer *upload)
{
	Chunk  *chunk;
	Source *source;

	/* make sure everything is how we expect it to be */
	assert (upload != NULL);

	chunk  = list_nth_data (upload->chunks, 0);
	assert (chunk != NULL);

	source = chunk->source;
	assert (source != NULL);

	/*
	 * Unregister this event.  This should also dump the last known status to
	 * the ui for reporting.
	 */
	if_transfer_finish (upload->event);
	upload->event = NULL;

	/* update the current uploads list */
	uploads = list_remove (uploads, upload);

	/* inform all protocols of an availability change */
	send_avail_update ();

	/* inform the protocol that handled this upload that it has been
	 * destroyed */
	source->p->upload_stop (source->p, upload, chunk, source);

	/* detach this transfer from its upload slot, if any */
	upload_queue_detach (upload);

	/* kaboom */
	transfer_free (upload);

	/*
	 * If no more uploads are left we should unload the timer until the next
	 * upload is created.  This should reduce cycles for inactive nodes.
	 */
	if (!uploads)
	{
		timer_remove_zero (&upload_timer);

#ifdef THROTTLE_ENABLE
		timer_remove_zero (&throttle_resume_timer);
#endif /* THROTTLE_ENABLE */
	}
}

/*****************************************************************************/

void upload_stop (Transfer *upload, int cancel)
{
	Chunk  *chunk;
	Source *source;

	assert (upload);

	chunk  = upload->chunks->data;
	source = chunk->source;

	if (chunk->start + chunk->transmit >= chunk->stop)
		source->status = SOURCE_COMPLETE;

#if 0
	/* give the protocol a heads up */
	if (source->p->upload)
		(*source->p->upload) (chunk, PROTOCOL_TRANSFER_CANCEL, NULL);
#endif

#if 0
	chunk->source = NULL;
	source->chunk = NULL;
#endif

	upload_free (upload);
}

/*****************************************************************************/

void upload_write (Chunk *chunk, unsigned char *segment, size_t len)
{
	unsigned long remainder;

	assert (chunk);
	assert (chunk->transfer);

	if (!segment || len == 0)
	{
		upload_stop (chunk->transfer, TRUE);
		return;
	}

	remainder = chunk->transfer->total - chunk->transfer->transmit;

	if (len > remainder)
		len = remainder;

	chunk->transmit           += len;
	chunk->transfer->transmit += len;

	chunk->source->status = SOURCE_ACTIVE;

#ifdef THROTTLE_ENABLE
	if (MAX_UPLOAD_BW > 0)
		upload_credits = len < upload_credits ? upload_credits - len : 0;
#endif /* THROTTLE_ENABLE */

	/* this will stop uploading if the transfer length for the slot
	 * was exceeded */
	if (!upload_queue_write (chunk->transfer, len))
		return;

	/* upload completed, tell the protocol to clean itself up */
	if (remainder - len <= 0)
		upload_stop (chunk->transfer, TRUE);
}

/*****************************************************************************/

int upload_length (char *user)
{
	return transfer_length (uploads, user, FALSE);
}

/*****************************************************************************/

/* find any inactive queue slots for this {user, path} combination */
static tqueue_t *queue_find_inactive (Protocol *p,
                                      const char *user, const char *path)
{
	size_t    i;
	size_t    len = array_count (&uploads_queue);
	tqueue_t *queue;

	for (i = 0; i < len; i++)
	{
		queue = array_index (&uploads_queue, i);
		assert (queue != NULL);

		if (p->user_cmp (p, queue->user, user) != 0)
			continue;

		if (strcmp (queue->path, path) != 0)
			continue;

		if (queue->active)
			continue;

		return queue;
	}

	return NULL;
}

static tqueue_t *queue_new (const char *user, const char *path, size_t len)
{
	tqueue_t *queue;

	assert (user != NULL);

	if (!(queue = MALLOC (sizeof (tqueue_t))))
		return NULL;

	queue->user   = STRDUP (user);
	queue->timer  = 0;
	queue->path   = STRDUP (path);
	queue->len    = len;
	queue->active = FALSE;

	return queue;
}

static void queue_free (tqueue_t *queue)
{
	assert (queue != NULL);

	free (queue->path);
	free (queue->user);
	free (queue);
}

static void adjust_pos (void)
{
	size_t i;
	size_t cnt = array_count (&uploads_queue);

	for (i = 0; i < cnt; i++)
	{
		tqueue_t *queue;

		queue = array_index (&uploads_queue, i);
		assert (queue != NULL);

		queue->pos = i;
	}
}

static void queue_remove (tqueue_t *queue)
{
	tqueue_t *queue_chk;

	assert (queue->pos >= 0);
	assert (queue->pos < array_count (&uploads_queue));

	/* make sure queue->pos isnt wrong */
	queue_chk = array_index (&uploads_queue, queue->pos);
	assert (queue_chk != NULL);
	assert (queue_chk == queue);

	/* remove the timer, if any */
	timer_remove_zero (&queue->timer);

	/* remove the entry and nuke the queue object */
	array_splice (&uploads_queue, queue->pos, 1, NULL);
	queue_free (queue);

	/* rebuild the queue->pos elements so they match the respective positions
	 * in the array after the shift */
	adjust_pos ();
}

static BOOL remove_queue_tick (tqueue_t *queue)
{
	GIFT_TRACE (("timed out %s(%u)", queue->user, (unsigned int)queue->pos));

	queue_remove (queue);
	return FALSE;
}

static void queue_reset (tqueue_t *queue, time_t interval)
{
	if (!queue)
		return;

	assert (queue->timer == 0);

	/* run a timer to remove this entry after so much idle time (that is, time
	 * spent between hits) */
	queue->timer =
	    timer_add (interval, (TimerCallback)remove_queue_tick, queue);
}

static tqueue_t *queue_add (const char *user, const char *path, size_t len)
{
	tqueue_t *queue;

	if (!(queue = queue_new (user, path, len)))
		return NULL;

	/* the position is meant to be the index, which would be the previous
	 * number of members */
	queue->pos = array_count (&uploads_queue);
	array_push (&uploads_queue, queue);

	/* run a timer to remove this entry after so much idle time (that is, time
	 * spent between hits) */
	queue_reset (queue, 3 * MINUTES);

	return queue;
}

/* translate position in queue array into position in abstract queue */
static unsigned int queue_pos (unsigned int array_pos)
{
	unsigned int available_slots = max_uploads;

	/* should only be called if there's a queue to worry about */
	assert (max_uploads >= 0);

	if (array_pos < available_slots)
		return 0;

	return array_pos - available_slots + 1;
}

static int queue_ttl (void)
{
	unsigned int array_len = array_count (&uploads_queue);

	assert (max_uploads >= 0);

	if (array_len < max_uploads)
		return 0;

	return array_len - max_uploads;
}

static size_t queue_length (Protocol *p, const char *user)
{
	size_t    i;
	tqueue_t *queue;
	size_t    size   = 0;
	size_t    len    = array_count (&uploads_queue);

	for (i = 0; i < len; i++)
	{
		queue = array_index (&uploads_queue, i);
		assert (queue != NULL);

		if (p->user_cmp (p, queue->user, user) != 0)
			continue;

		size++;
	}

	return size;
}

/*
 * Each time a queued upload request is made we "hit" the users status to
 * keep them alive in the fair position-based upload queue.  This queue
 * method is used to guarantee that no one can alter their timers to get a
 * better "chance" at slipping into a queue slot.  It's inefficient as shit
 * but its implemented here so that other protocols may take advantage of the
 * feature (even if the protocol has no such support).
 *
 * The fair upload queue effectively tracks all failed requests and will wait
 * for the next person in line to request a file when the queue has an
 * available slot.  If any user does not "hit" their upload in a given amount
 * of time a timer will be raised which removes them from the queue and they
 * must start over again.  This is done because most common implementations
 * will use a basic 60 second timer for re-requesting and we can rely on a
 * lacking request in x amount of time means the transfer was cancelled on
 * their client side.
 */
static unsigned int queue_hit (Protocol *p,
                               const char *user, const char *path, size_t len)
{
	tqueue_t *queue;

	if ((queue = queue_find_inactive (p, user, path)))
		timer_reset (queue->timer);
	else
	{
		queue = queue_add (user, path, len);
		assert (queue != NULL);
	}

	return ((unsigned int)queue->pos);
}

/*****************************************************************************/

/* attach the Transfer into the queue */
static void upload_queue_attach (Transfer *transfer,
                                 const char *user, const char *path)
{
	tqueue_t   *queue;
	Source     *source;
	Protocol   *p;

	/* grab the source at the head of the sources list */
	source = list_nth_data (transfer->sources, 0);
	assert (source != NULL);

	if (!(p = source->p))
		return;

	if (!(queue = queue_find_inactive (p, user, path)))
		return;

	/* stop trying to timeout this queue slot, as the upload has started */
	timer_remove_zero (&queue->timer);

	/* mark this queue slot active */
	queue->active = TRUE;

	UPLOAD(transfer)->queue = queue;
}

static void upload_queue_detach (Transfer *transfer)
{
	tqueue_t *queue    = transfer->up.queue;
	time_t    period;

	if (!queue)
		return;

	/* clear activity on this slot */
	queue->active = FALSE;

	if (queue->len <= 0)
	{
		queue_remove (queue);
		return;
	}

	/*
	 * Keep the upload slot open for a brief period when data remains.
	 * If there is still some data left on the transfer, the peer probably
	 * aborted (client crashed, etc), and we keep the slot open longer.
	 */
	period = 15 * SECONDS;

	if (transfer->total - transfer->transmit > 0)
		period = 1 * MINUTES;

	GIFT_TRACE (("keeping upload slot for %s(%s)[%d bytes left][timeout %us]",
	             queue->user, queue->path, queue->len,
	             (unsigned int)(period / SECONDS)));

	queue_reset (queue, period);
}

static BOOL upload_queue_write (Transfer *transfer, size_t len)
{
	tqueue_t *queue = UPLOAD(transfer)->queue;

	/* ignore the transfer if its not on the queue */
	if (!queue)
		return TRUE;

	if (len > queue->len)
		len = queue->len;

	queue->len -= len;

	if (queue->len <= 0)
	{
		upload_stop (transfer, TRUE);
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static void set_queued_info (const char *user, const char *path,
                             unsigned int array_pos, upload_auth_t *info)
{
	unsigned int queued_pos;
	unsigned int queued_ttl;

	/* get the position in the queue from the array position */
	queued_pos = queue_pos (array_pos);
	queued_ttl = queue_ttl ();

	GIFT_TRACE (("hit %s(%s): %u/%u", user, path, queued_pos, queued_ttl));

	/* pass back the queue position for the caller */
	if (info)
	{
		info->queue_pos = queued_pos;
		info->queue_ttl = queued_ttl;
	}
}

static int test_max_uploads (Protocol *p, const char *user, Share *share,
                             upload_auth_t *info)
{
	int max_peruser         = MAX_PERUSER_UPLOADS;
	unsigned int active_uls;
	unsigned int pos;

	/* initialize as a convenience for the caller, however you should only rely
	 * on this data for UPLOAD_AUTH_MAX */
	if (info)
		memset (info, 0, sizeof (upload_auth_t));

	/*
	 * Only check if the per-user limit was exceeded if this upload wasn't
	 * previously queued.
	 */
	if (max_peruser > 0 && !(queue_find_inactive (p, user, share->path)))
	{
		/* get the number of other uploads queued for this user */
		active_uls = queue_length (p, user);

		if (active_uls >= max_peruser)
			return UPLOAD_AUTH_MAX_PERUSER;
	}

	if (max_uploads == 0 || disabled)
		return UPLOAD_AUTH_HIDDEN;

	/* add this request to the upload queue */
	pos = queue_hit (p, user, share->path, share->size);

	/*
	 * NOTE: If upload_availability() returns > 0 right now, we may still not
	 * allow the upload if we are reserving an upload slot for somebody else.
	 */
	if (max_uploads < 0 || pos < max_uploads)
		return UPLOAD_AUTH_ALLOW;

	/* set the queued status information for the caller */
	set_queued_info (user, share->path, pos, info);

	return UPLOAD_AUTH_MAX;
}

int upload_auth (Protocol *p, const char *user, Share *share,
                 upload_auth_t *info)
{
	int ret;

	if (!share)
		return UPLOAD_AUTH_NOTSHARED;

	if (share_update_entry (share))
		return UPLOAD_AUTH_STALE;

	if ((ret = test_max_uploads (p, user, share, info)) != UPLOAD_AUTH_ALLOW)
		return ret;

	return UPLOAD_AUTH_ALLOW;
}

/*****************************************************************************/

void upload_disable ()
{
	disabled = TRUE;
	max_uploads = upload_status ();
}

void upload_enable ()
{
	disabled = FALSE;
	max_uploads = upload_status ();
}

int upload_status ()
{
	if (disabled)
		return 0;

	/* sync with config */
	max_uploads = config_get_int (gift_conf, "sharing/max_uploads=-1");

	return max_uploads;
}

unsigned long upload_availability (void)
{
	unsigned long av = 0;
	int           status;

	status = upload_status ();

	if (status == 0)
		return av;

	/* hardcode an availability of 9 (arbitrarily high number) in the event
	 * that there is no limit imposed */
	if (status < 0)
		av = 9;
	else
		av = MAX (0, status - upload_length (NULL));

	return av;
}
