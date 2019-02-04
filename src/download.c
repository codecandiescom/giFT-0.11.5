/*
 * $Id: download.c,v 1.268 2003/12/26 04:39:23 jasta Exp $
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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "giftd.h"

#include "download.h"
#include "download_state.h"

#include "lib/parse.h"
#include "lib/event.h"
#include "lib/conf.h"
#include "lib/file.h"

#include "share_cache.h"

#include "if_transfer.h"

#include "plugin.h"                  /* plugin_lookup */

/*****************************************************************************/

/* TODO -- a great bit of this file is duplicated in upload.c.  This _must_
 * be cleaned up */

extern Config *gift_conf;

/*****************************************************************************/

/* NOTE: do not enable this unless you really know what you're doing, or
 * one of the developers has told you to :) */
/* #define PAD_FILE */

/* do not subdivide the transfer if the amount left is less than this */
#define DIVIDE_THRESHOLD      100000   /* bytes */

#define MIN_INACTIVE          45       /* 45 seconds */
#define MAX_INACTIVE          900      /* 15 minutes */
#define MIN_INCREMENT         2        /* number of seconds to inc each tick */

#define STATE_INTERVAL        5        /* write the statefile every 5s */

#define MAX_PERUSER_DOWNLOADS 1        /* download queue */

/*****************************************************************************/

static timer_id  download_timer = 0;
static List     *downloads      = NULL;

/*****************************************************************************/

#ifdef THROTTLE_ENABLE

static size_t    max_downstream        = 0;
static size_t    download_credits      = 0;
static timer_id  throttle_resume_timer = 0;

#define MAX_DOWNLOAD_BW max_downstream

#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

/* keep this files organization clean */
static Chunk *unused_chunk         (Transfer *transfer);
static void   activate_chunk       (Chunk *chunk, Source *source);
static void   handle_chunk_timeout (Chunk *chunk, Transfer *transfer);
static int    deactivate_chunk     (Chunk *chunk);
static BOOL   handle_source_queue  (Transfer *transfer, Source *source);

/*****************************************************************************/

static int download_report_progress (Transfer *transfer, void *udata)
{
	/* write the accompanying state file (for resume) */
	if (transfer->flag < STATE_INTERVAL)
		transfer->flag++;
	else
	{
		if (!(download_state_save (transfer)))
		{
			/* inform the user just how screwed they are */
			GIFT_ERROR (("failed to save state file!"));
			GIFT_WARN  (("no failsafe method implemented!"));
		}

		transfer->flag = 0;
	}

	if (DOWNLOAD(transfer)->paused)
		return TRUE;

	if_transfer_change (transfer->event, FALSE);

	list_foreach (transfer->chunks, (ListForeachFunc) handle_chunk_timeout,
	              transfer);

	return TRUE;
}

static int download_report (void *udata)
{
	list_foreach (downloads, (ListForeachFunc) download_report_progress, NULL);
	return TRUE;
}

/*****************************************************************************/

List *download_list ()
{
	return downloads;
}

#ifdef THROTTLE_ENABLE
static int throttle_update (void *arg)
{
	transfers_resume (downloads, MAX_DOWNLOAD_BW, &download_credits);
	return TRUE;
}
#endif /* THROTTLE_ENABLE */

/* returns the maximum amount of data to recv */
size_t download_throttle (Chunk *chunk, size_t len)
{
	off_t rem;

	/* get the number of bytes remaining for this chunk so that we can
	 * clamp len properly */
	rem = chunk->stop - (chunk->start + chunk->transmit);
	len = (size_t)(MIN (rem, len));

#ifdef THROTTLE_ENABLE
	return chunk_throttle (chunk, MAX_DOWNLOAD_BW, len, download_credits);
#else
	return len;
#endif /* THROTTLE_ENABLE */
}

/*****************************************************************************/

static int download_length (char *user, int active)
{
	return transfer_length (downloads, user, active);
}

/*****************************************************************************/

static int download_register (TCPC *c, if_event_id requested,
                              Transfer *transfer)
{
	/* every second dump stats */
	if (!download_timer)
	{
		download_timer = timer_add (1 * SECONDS,
		                            (TimerCallback) download_report, NULL);
	}

	if (!(transfer->event = if_transfer_new (c, requested, transfer)))
		return FALSE;

#ifdef THROTTLE_ENABLE
	max_downstream =
	    (size_t)config_get_int (gift_conf, "bandwidth/downstream=0");

	if (throttle_resume_timer == 0 && MAX_DOWNLOAD_BW > 0)
	{
		throttle_resume_timer =
			timer_add (THROTTLE_TIME, (TimerCallback) throttle_update, NULL);

		/* set initial download credits */
		download_credits = max_downstream * THROTTLE_TIME / SECONDS;
	}
#endif /* THROTTLE_ENABLE */

	downloads = list_append (downloads, transfer);

	return TRUE;
}

/* create a filename that can easily be identified as a "temporary"
 * download */
static char *uniq_file (Transfer *transfer)
{
	char  *uniq;
	struct timeval tv;

	if (!transfer || !transfer->filename)
		return NULL;

	if (!(uniq = malloc (strlen (transfer->filename) + 50)))
		return NULL;

	/*
	 * Construct a new suitably random filename path and check if it exists
	 * on disk before we proceed.  Continue retrying different time indexes
	 * until a valid entry can be found.
	 */
	do
	{
		if (platform_gettimeofday (&tv, NULL) != 0)
		{
			GIFT_ERROR (("gettimeofday failed: %s", GIFT_STRERROR()));
			return NULL;
		}

		sprintf (uniq, "%04lX%08lX%08lX.%s", tv.tv_sec % 0xffff, tv.tv_usec,
		         (unsigned long) transfer->total, transfer->filename);
	} while (file_exists (uniq));

	return uniq;
}

Transfer *download_new (TCPC *c, if_event_id requested,
                        char *uniq, char *filename, char *hash,
                        off_t size)
{
	Transfer *transfer;

	if (!(transfer = transfer_new (TRANSFER_DOWNLOAD, filename, hash, size)))
		return NULL;

	/* write immediately */
	transfer->flag = STATE_INTERVAL;

	if (uniq)
		DOWNLOAD(transfer)->uniq = STRDUP (uniq);
	else
	{
		/* create a unique temporary name
		 * NOTE: this is NOT a path!  It is simply a unique string that we
		 * can reference this transfer by in the incoming dir.  Will be used
		 * to construct path and state_path later */
		DOWNLOAD(transfer)->uniq = uniq_file (transfer);
	}

	if (!download_register (c, requested, transfer))
	{
		transfer_free (transfer);
		return NULL;
	}

	return transfer;
}

/*****************************************************************************/

static int find_next_queue (Transfer *transfer, Source **source)
{
	List *ptr;

	/* loop through all sources to find a user match */
	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		Source *cmp = ptr->data;

		if (!cmp || !cmp->user || *source == cmp)
			continue;

		if (cmp->status != SOURCE_QUEUED_LOCAL)
			continue;

		/* found a match, return it */
		if (!transfer_user_cmp (cmp, (*source)->user))
		{
			*source = cmp;
			return 0;
		}
	}

	return -1;
}

static BOOL activate_next_queued (Source *source)
{
	List  *waiting;
	Chunk *chunk;

	if (!source)
		return FALSE;

	/* look for a download with this user queued
	 * NOTE: when found, source will be set to the next queued source */
	waiting = list_find_custom (downloads, &source,
	                            (CompareFunc)find_next_queue);

	if (!waiting || !waiting->data)
	{
		/* this happens under normal operation, no need to log excessively */
#if 0
		GIFT_TRACE (("unable to find a waiting transfer in the queue..."));
#endif
		return FALSE;
	}

	if (!(chunk = unused_chunk (waiting->data)))
		return FALSE;

	/* unset the queued status */
	source_status_set (source, SOURCE_WAITING, NULL);
	activate_chunk (chunk, source);

	return TRUE;
}

static void handle_next_queue (Transfer *transfer)
{
	List     *ptr;

	GIFT_TRACE (("%s", transfer->filename));

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		Source *source = ptr->data;

		if (!source || !source->user)
			continue;

		activate_next_queued (source);
	}
}

static void cancel_sources (Transfer *transfer)
{
	Source   *source;
	Protocol *p;
	List     *ptr;
	List     *next;

	/* deactivate all chunks */
	ptr = transfer->chunks;

	while (ptr)
	{
		Chunk *chunk = ptr->data;

		next = ptr->next;

		/* this condition completely fucks with giFT.  The protocol needs
		 * to be notified that this chunk is going to be freed, but it has
		 * absolutely no way of doing so because there's no source (and thus
		 * no protocol) to reply to!  Pretty serious design flaw.  A temporary
		 * work around is to catch the condition and simply emulate a reply
		 * based on the sources list
		 *
		 * NOTE: this is CLEARLY temporary */
		if (chunk && !chunk->source)
		{
			if (!transfer->sources || !transfer->sources->data)
				GIFT_TRACE (("*sigh*...notify jasta"));
			else
			{
				source = list_nth_data (transfer->sources, 0);
				p = source->p;

				/* if p is NULL, the protocol is not even loaded,
				 * and we really cant notify anything. */
				if (p != NULL)
					p->download_stop (p, transfer, chunk, source, FALSE);
			}
		}

		/* notify the protocol that this chunk is finished
		 * NOTE: this function may attempt to remove itself from this list
		 * if the chunk is complete */
		deactivate_chunk (chunk);

		ptr = next;
	}
}

static void flush_sources (Transfer *transfer)
{
	List *ptr;

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		Protocol  *p;
		Source    *source = ptr->data;

		if (!(p = source->p))
			continue;

		/* tell the plugin this source is going away */
		p->source_remove (p, transfer, source);
	}
}

void download_free (Transfer *transfer, int premature)
{
	/* cancel all sources and flush all TCPCs */
	cancel_sources (transfer);

	if_transfer_finish (transfer->event);
	transfer->event = NULL;

	/* start any queued transfers that are waiting on these sources */
	handle_next_queue (transfer);

	/* notify plugin(s) about the removal of all sources */
	flush_sources (transfer);

	/* unregister */
	downloads = list_remove (downloads, transfer);

	transfer_free (transfer);

	if (!downloads)
	{
		timer_remove_zero (&download_timer);

#ifdef THROTTLE_ENABLE
		timer_remove_zero (&throttle_resume_timer);
#endif /* THROTTLE_ENABLE */
	}
}

/*****************************************************************************/

static char *calculate_final_path (Transfer *transfer)
{
	char        *final;
	char         fmt[PATH_MAX];
	unsigned int uniq = 0;

	do
	{
		snprintf (fmt, sizeof (fmt) - 1, "%s%s%s", transfer->filename,
		          (uniq ? "." : ""), (uniq ? ITOA (uniq) : ""));

		final = COMPLETED_PATH (("%s", fmt));
		uniq++;
	}
	while (file_exists (final));

	return STRDUP (final);
}

static char *checksum_file (char *path, char *type)
{
	HashAlgo *algo;
	Hash     *hash;
	char     *hashstr;

	if (!(algo = hash_algo_lookup (type)))
		return NULL;

	if (!(hash = hash_calc (algo, path)))
		return NULL;

	hashstr = hash_dsp (hash);
	hash_free (hash);

	/* TYPE:ASCII_DATA */
	return hashstr;
}

static BOOL verify_integrity (Transfer *transfer)
{
	char *hashstr;
	char *algo;
	int   cmp;

	if (!transfer->hash)
		return TRUE;

	/* some protocols do not have hashes for all files, so we should just
	 * pretend that the file verified successfully */
	if (!(algo = hashstr_algo (transfer->hash)))
	{
		GIFT_TRACE (("BUG: transfer->hash = %s", transfer->hash));
		return TRUE;
	}

	/* TODO: save this hash somewhere so we don't have to hash again if
	 * share_completed is on! */
	if (!(hashstr = checksum_file (transfer->path, algo)))
		return FALSE;

	cmp = strcmp (hashstr, transfer->hash);
	free (hashstr);

	return BOOL_EXPR (cmp == 0);
}

static char *move_file (char *src, char *dst)
{
	file_mv (src, dst);
	free (src);

	return dst;
}

static void reorganize_files (Transfer *transfer, int clean)
{
	char *path;
	char *state_path;

	if (clean)
	{
		/* move the file itself to its final uncorrupted resting place */
		if ((path = calculate_final_path (transfer)))
			transfer->path = move_file (transfer->path, path);

		file_unlink (DOWNLOAD(transfer)->state_path);
	}
	else
	{
		/* preserve the state file and temporary (corrupted) download as it
		 * may be useful when hunting bugs */
		path = STRDUP (INCOMING_PATH (("corrupted/%s",
		                               DOWNLOAD(transfer)->uniq)));
		transfer->path = move_file (transfer->path, path);

		state_path = STRDUP (INCOMING_PATH (("corrupted/%s.state",
		                                     DOWNLOAD(transfer)->uniq)));
		DOWNLOAD(transfer)->state_path =
		    move_file (DOWNLOAD(transfer)->state_path, state_path);
	}
}

static void update_interface (Transfer *transfer)
{
	if (!transfer)
		return;

	/* arrrggggghhh, this is shit */
	DOWNLOAD(transfer)->verifying = TRUE;
	if_transfer_change (transfer->event, TRUE);
	DOWNLOAD(transfer)->verifying = FALSE;
}

static void download_complete (Transfer *transfer)
{
	int clean;

	GIFT_TRACE (("transfer completed (%li), verifying data integrity...",
	             (long)transfer->total));

	/* cleanup file descriptors */
	file_close (transfer->f);
	transfer->f = NULL;

	/* let the interface protocol know whats going down */
	update_interface (transfer);

	/*
	 * attempt to verify that this transfer matches the hash provided by the
	 * protocol so that we don't end up resharing corrupted files.  please
	 * note that this implementation is temporary as it does not take into
	 * account protocols other than OpenFT.  will redesign this to be a part
	 * of protocol communication soon.
	 */
	if (!(clean = verify_integrity (transfer)))
	{
		GIFT_ERROR (("DETECTED CORRUPTION FOR %s!  RELEVANT FILES COPIED "
		             "TO CORRUPTED DIR!",
		             transfer->filename));
	}

	/* move all files associated with this transfer to a more appropriate
	 * location */
	reorganize_files (transfer, clean);

#if 0
	/* notify the scripting backend of this event */
	hook_event ("download_complete", FALSE,
	            HOOK_VAR_STR, transfer->path,
	            HOOK_VAR_STR, transfer->hash,
	            HOOK_VAR_STR, transfer->state_path,
	            HOOK_VAR_NUL, NULL);
#endif

	if (!clean)
	{
		/* if we have identified that this file corrupted, recreate a new
		 * transfer object so that we do not lose the original hash for the
		 * user...dont worry about adding sources yet. */
		download_new (NULL, 0, NULL, transfer->filename, transfer->hash,
					  transfer->total);
	}
	else
	{
		/* if completed files are to be reshared, add this now.  keep in mind
		 * that this will hash in the background by default */
		if (config_get_int (gift_conf, "sharing/share_completed=1"))
			share_add_entry (transfer->path);
	}

	/* cleanup the associated memory and handle the transfer queue */
	download_free (transfer, FALSE);
}

/*****************************************************************************/

static int chunk_pause (Chunk *chunk, Transfer *transfer)
{
	deactivate_chunk (chunk);
	return TRUE;
}

static int source_pause (Source *source, Transfer *transfer)
{
	source_status_set (source, SOURCE_PAUSED, NULL);
	return TRUE;
}

void download_pause (Transfer *transfer)
{
	DOWNLOAD(transfer)->paused = TRUE;

	list_foreach (transfer->chunks,  (ListForeachFunc) chunk_pause, transfer);
	list_foreach (transfer->sources, (ListForeachFunc) source_pause, transfer);
}

static int source_unpause (Source *source, Transfer *transfer)
{
	Chunk *chunk;

	source_status_set (source, SOURCE_WAITING, NULL);

	if (!(chunk = unused_chunk (transfer)))
		return TRUE;

	if (handle_source_queue (transfer, source))
		return TRUE;

	activate_chunk (chunk, source);

	return TRUE;
}

void download_unpause (Transfer *transfer)
{
	DOWNLOAD(transfer)->paused = FALSE;

	list_foreach (transfer->sources, (ListForeachFunc) source_unpause, transfer);
}

void download_stop (Transfer *transfer, int cancel)
{
	if (cancel)
	{
		if (transfer->f)
		{
			fclose (transfer->f);
			transfer->f = NULL;
		}

		GIFT_TRACE (("removing transfer files for %s",
		             DOWNLOAD(transfer)->uniq));

		file_unlink (transfer->path);
		file_unlink (DOWNLOAD(transfer)->state_path);
	}

	download_free (transfer, TRUE);
}

/*****************************************************************************/
/*
 * LOGIC ROUTINES FOR MULTISOURCED DOWNLOADING
 */

/* check to see if this chunk has reached its maximum amount of time w/o
 * seeing data */
static int data_timeout (Chunk *chunk)
{
	if (!chunk)
		return FALSE;

	/* data hasn't timed out because we havent requested it */
	if (chunk->source && chunk->source->status == SOURCE_QUEUED_LOCAL)
		return FALSE;

	/* starts at 0 */
	if (chunk->timeout_max < MIN_INACTIVE)
		chunk->timeout_max = MIN_INACTIVE;

	if (chunk->timeout_cnt >= chunk->timeout_max)
	{
		/* reset timeout */
		chunk->timeout_cnt = 0;

#if 0
		/* once this chunk reaches its maximum data timeout we should just drop
		 * the related source...im very sure its dead :) */
		if (chunk->timeout_max >= MAX_INACTIVE)
		{
			assert (chunk->source != NULL);
			assert (chunk->source->url != NULL);

			GIFT_TRACE (("removing %s: super timeout", chunk->source->url));
			download_remove_source (chunk->transfer, chunk->source);

			/* pretend we didn't timeout so that no further processing of this
			 * source will be done */
			return FALSE;
		}
#endif

		/* apply an incremental timeout in order to be polite */
		chunk->timeout_max += MIN_INCREMENT;

		if (chunk->source && chunk->source->status == SOURCE_QUEUED_REMOTE)
		{
			/* there is some whacked logic from the caller of this func that
			 * tests > MIN_INACTIVE + MIN_INCREMENT to deactivate the
			 * chunk...errrrggghh */
			chunk->timeout_max =
				MIN (chunk->timeout_max, MIN_INACTIVE + (MIN_INCREMENT * 2));
		}

		return TRUE;
	}

	/* increment timeout count or reset timeout values depending on whether
	 * or not data is was available */
	if (chunk->tmp_recv == 0)
		chunk->timeout_cnt++;
	else
	{
		/* valid data responded, make sure timeout is still at its default */
		chunk->timeout_cnt = 0;
		chunk->timeout_max = MIN_INACTIVE;
	}

	chunk->tmp_recv = 0;

	/* hasn't reached the maximum yet */
	return FALSE;
}

/*****************************************************************************/

/* find an unused division of the download (or create a new one)
 * NOTE: if the file is small enough, no chunk will be returned.  You must
 * wait for the chunks timeout to reassign if needed */
static Chunk *unused_chunk (Transfer *transfer)
{
	List  *ptr;
	Chunk *largest = NULL;
	unsigned long start = 0;
	unsigned long stop  = 0;
	unsigned long diff  = 0;
	unsigned long largest_diff = 0;

	/* first check to see if we don't already have a nulled chunk somewhere */
	for (ptr = transfer->chunks; ptr; ptr = ptr->next)
	{
		Chunk *tmp = ptr->data;

		/* found an unused chunk, no need to look further */
		if (!tmp->source)
			return tmp;

		/* locate the largest available chunk */
		if (tmp->start + tmp->transmit > tmp->stop)
		{
			GIFT_TRACE (("chunk overrun..."));
			continue;
		}

		diff = tmp->stop - tmp->start - tmp->transmit;

		if (!largest || diff > largest_diff)
		{
			largest      = tmp;
			largest_diff = diff;
		}
	}

	/* do not redivide a chunk less than the desired threshold */
	if (largest && largest_diff < DIVIDE_THRESHOLD)
		return NULL;

	if (!largest)
		stop = transfer->total;
	else
	{
		/* subdivide the largest inactive chunk */
		diff = largest_diff / 2;
		stop = largest->stop;
		largest->stop -= diff;
		start = largest->stop;

		largest->stop_change = TRUE;
	}

	/* no chunk was found unused, create a new one with the above divisions */
	return chunk_new (transfer, NULL, start, stop);
}

/*****************************************************************************/

static void activate_chunk (Chunk *chunk, Source *source)
{
	Protocol *p;

	if (!chunk || !source)
		return;

	assert (source->chunk == NULL);

	/* set this only when status has never changed */
	if (source->status == SOURCE_UNUSED)
		source_status_set (source, SOURCE_WAITING, NULL);

	/* setup the circular reference */
	chunk->source = source;
	source->chunk = chunk;

	chunk->tmp_recv = 0;

	if ((p = source->p))
	{
		BOOL ret;

		/* actually request to download this source now */
		ret = p->download_start (p, chunk->transfer, chunk, source);

		/* force something to go back to the user if the protocol is
		 * being lazy... */
		if (!ret && source->status != SOURCE_CANCELLED)
			source_status_set (source, SOURCE_CANCELLED, "Protocol error");
	}
}

static void remove_chunk (Chunk *chunk)
{
	Transfer *transfer;

	if (!chunk)
		return;

	transfer = chunk->transfer;

	transfer->chunks = list_remove (transfer->chunks, chunk);
	chunk_free (chunk);
}

/* returns TRUE if the chunk has completed and was removed from the list
 * NOTE: you must not continue to use chunk if this function returns TRUE */
static int deactivate_chunk (Chunk *chunk)
{
	Transfer *transfer;
	Source   *source;
	Protocol *p;
	int       completed;

	/* already deactivated */
	if (!chunk || !chunk->source)
		return FALSE;

	transfer  = chunk->transfer;
	source    = chunk->source;
	completed = chunk->start + chunk->transmit >= chunk->stop;

	/* this chunk was prematurely deactivated, punish the source by reducing
	 * its priority */
	if (!completed)
	{
		/* TODO -- actually use a priority rating and sort by it :) */
		transfer->sources = list_remove (transfer->sources, source);
		transfer->sources = list_append (transfer->sources, source);
	}

	if (completed)
		source_status_set (chunk->source, SOURCE_COMPLETE, NULL);
	else if (chunk->source->status == SOURCE_ACTIVE)
		source_status_set (chunk->source, SOURCE_CANCELLED, NULL);

	/* only set this if the protocol never gave us any other change */
	if (chunk->source->status == SOURCE_WAITING)
		source_status_set (chunk->source, SOURCE_CANCELLED, NULL);

	chunk->tmp_recv  = 0;
	chunk->suspended = FALSE;

	if ((p = source->p))
	{
		/*
		 * Notify the protocol that they need to clean up.
		 *
		 * NOTE: We do this regardless of whether or not the chunk is
		 * completed just in case the protocol didn't wanna clean itself up.
		 */
		p->download_stop (p, transfer, chunk, source, completed);
	}

	/* cleanup the data links */
	chunk->source->chunk  = NULL;
	chunk->source         = NULL;
	chunk->udata          = NULL;      /* maybe we should force plugins to do
	                                    * this? */

	/* this chunk completed, make sure we remove it from the chunks list */
	if (completed)
	{
		remove_chunk (chunk);
		return TRUE;
	}

	return FALSE;
}

/*****************************************************************************/

void relocate_source (Source *source)
{
	Transfer *transfer;
	Chunk    *chunk;

	/* what the fuck...this happened once when the source was somehow
	 * added twice...i have NO idea how this happened, but giFT shouldn't
	 * be crashing because of it */
	if (!source)
		return;

	assert (source->chunk);

	transfer = source->chunk->transfer;

	/* this source is moving on, disable it's old placement
	 * NOTE: this will handle removal from the chunk list if completed */
	if (!deactivate_chunk (source->chunk))
	{
		/* this chunk did NOT complete...do not attempt to relocate this
		 * source (but do not remove it either) */
		return;
	}

	/* locate a new chunk to use */
	if (!(chunk = unused_chunk (transfer)))
		return;

	activate_chunk (chunk, source);
}

/*****************************************************************************/

static int find_inactive (Source *a, void *b)
{
	/* local queues should NOT be handled automagically.  see download_free */
	if (a->status == SOURCE_QUEUED_LOCAL)
		return -1;

	/* if there is no chunk associated, return 0 to indicate we have found
	 * an inactivate source */
	return (a->chunk != NULL);
}

static Source *find_inactive_source (Transfer *transfer)
{
	List *ls;

	ls = list_find_custom (transfer->sources, NULL,
						   (CompareFunc) find_inactive);

	return (ls ? ls->data : NULL);
}

/* this function is actually called above from download_report */
static void handle_chunk_timeout (Chunk *chunk, Transfer *transfer)
{
	Source *source;

	if (!chunk)
		return;

	/* check to see if this chunk's segment timed out */
	if (!data_timeout (chunk))
		return;

#if 0
	GIFT_TRACE (("%p: chunk %i-%i (%i) timed out...", transfer,
	             (int) chunk->start, (int) chunk->stop, (int) chunk->transmit));
#endif

	source_status_set (chunk->source, SOURCE_TIMEOUT, NULL);

	/* if this is not the first time this source has timed out, cancel it
	 * in order to possibly breathe new life into the TCPC */
	if (chunk->timeout_max > MIN_INACTIVE + MIN_INCREMENT)
		deactivate_chunk (chunk);

	/* check to see if we actually have something to use before we proceed */
	if (!(source = find_inactive_source (transfer)))
		return;

	/* we have found a new source for this chunk, make sure it's cancelled.
	 * NOTE: this function is safe to call multiple times in this fashion, so
	 * dont worry :) */
	deactivate_chunk (chunk);

	/* activate w/ the new source */
	activate_chunk (chunk, source);
}

/*****************************************************************************/

static Source *locate_source_custom (Transfer *transfer, void *data,
                                     CompareFunc cmpfn)
{
	List *link;

	link = list_find_custom (transfer->sources, data, cmpfn);

	return list_nth_data (link, 0);
}

static int cmp_source (Source *source, Source *cmp)
{
	return source_cmp (source, cmp);
}

static Source *locate_source (Transfer *transfer, Source *source)
{
	return locate_source_custom (transfer, source, (CompareFunc)cmp_source);
}

static int cmp_source_by_url (Source *source, char *url)
{
	return strcmp (source->url, url);
}

static Source *locate_source_by_url (Transfer *transfer, const char *url)
{
	return locate_source_custom (transfer, (char *)url,
	                             (CompareFunc)cmp_source_by_url);
}

static BOOL handle_source_queue (Transfer *transfer, Source *source)
{
	if (download_length (source->user, TRUE) < MAX_PERUSER_DOWNLOADS)
		return FALSE;

	source_status_set (source, SOURCE_QUEUED_LOCAL, NULL);

	/*
	 * Attempt to work around an assumption source_status_set makes
	 * about source->chunk.
	 */
	if (!source->chunk)
		if_transfer_change (transfer->event, TRUE);

	return TRUE;
}


/*****************************************************************************/

/*
 * This function removes the source from the transfer, disassociates with the
 * chunk, calls both PROTOCOL_SOURCE_REMOVE and PROTOCOL_TRANSFER_CANCEL in
 * protocol space and cleans up everything but the chunk associated.
 */
static BOOL remove_source (Transfer *transfer, Source *source)
{
	Protocol *p;

	if (!source)
		return FALSE;

	GIFT_TRACE (("%s", source->url));

	if ((p = source->p))
	{
		/*
		 * Notify the protocol that this source is going to be removed so
		 * that it can remove any associated data.  Remember that
		 * deactivate_chunk was already called, so p->download_stop was also
		 * used before this call.
		 *
		 * Note: I don't think p->download_stop was called already since
		 * deactivate_chunk is only called below. This is clearly the wrong
		 * behaviour but since I don't know what side effects changing it would
		 * have I'm leaving it that way for now. -- mkern
		 */
		p->source_remove (p, transfer, source);
	}

	/*
	 * Make sure the chunk is aware of this change, this will notify the
	 * protocol of the chunk change as well.
	 */
	deactivate_chunk (source->chunk);
	if_transfer_delsource (transfer->event, source);

	/* remove from the list(s) and free */
	transfer->sources = list_remove (transfer->sources, source);
	source_free (source);

	return TRUE;
}

void download_remove_source (Transfer *transfer, Source *source)
{
	Source *dup, *new_source;
	Chunk  *chunk;
	BOOL    was_queued;

	/*
	 * Keep a pointer to the chunk so we can assign another source below.
	 * assert that we aren't called with a completed chunk.
	 */
	if ((chunk = source->chunk))
		assert (chunk->start + chunk->transmit < chunk->stop);

	/* create a new source first because remove_source frees the source */
	dup = source_new (source->user, source->hash, source->size, source->url);

	was_queued = BOOL_EXPR(source->status == SOURCE_QUEUED_LOCAL);

	remove_source (transfer, source);

	/* reactivate any queued sources for this user if it wasn't queued */
	if (!was_queued)
		activate_next_queued (dup);

	source_free (dup);

	/*
	 * Find a new source for the chunk if activate_next_queued didn't
	 * assign one.
	 */
	if (chunk && !chunk->source)
	{
		/* activate chunk with new source */
		if ((new_source = find_inactive_source (transfer)))
			activate_chunk (chunk, new_source);
	}
}

/* called from the interface protocol */
void download_remove_source_by_url (Transfer *transfer, char *url)
{
	Source *source;

	if (!transfer || !url)
		return;

	if ((source = locate_source_by_url (transfer, url)))
		download_remove_source (transfer, source);
}

/*****************************************************************************/

/* insert a source into the transfer list and activate it using either one
 * of the previously nulled chunks or create a new one for this source */
static int add_source (Transfer *transfer, Source *source)
{
	Chunk    *chunk;
	Source   *old_source;
	Protocol *p;

	if (!transfer || !source)
		return FALSE;

	if ((old_source = locate_source (transfer, source)))
	{
		if (old_source->status == SOURCE_ACTIVE ||
		    old_source->status == SOURCE_QUEUED_LOCAL)
		{
#if 0
			GIFT_TRACE (("ignoring duplicate source %s", source->url));
#endif
			return FALSE;
		}

		GIFT_TRACE (("replacing %s (chunk=%p) with %s...",
		             old_source->url, old_source->chunk, source->url));

		/* if this fails, we shouldnt try to replace the source */
		if (!remove_source (transfer, old_source))
		{
			GIFT_ERROR (("remove_source failed"));
			return FALSE;
		}
	}

	if ((p = source->p))
	{
		/* notify the plugin, but abort if the protocol requests it */
		if (!p->source_add (p, transfer, source))
			return FALSE;
	}

	transfer->sources = list_append (transfer->sources, source);

	if (DOWNLOAD(transfer)->paused)
	{
		source_status_set (source, SOURCE_PAUSED, NULL);
		return TRUE;
	}

	if_transfer_addsource (transfer->event, source);

	if (!(chunk = unused_chunk (transfer)))
		return TRUE;

	/* if need be, mark this source as queued and process it later */
	if (handle_source_queue (transfer, source))
		return TRUE;

	activate_chunk (chunk, source);

	return TRUE;
}

/* called from the interface protocol */
void download_add_source (Transfer *transfer,
                          char *user, char *hash, char *url)
{
	Source *source;

	if (!transfer)
		return;

#if 0
	assert (strcmp (transfer->hash, hash) == 0);
#endif

	if (!(source = source_new (user, hash, transfer->total, url)))
		return;

	if (!add_source (transfer, source))
	{
		source_free (source);
		return;
	}

#if 0
	/* raise the notify
	 * TODO -- this should have a much better API */
	if_transfer_list (NULL, 0, IFEVENT_BROADCAST, download_list (), "DOWNLOAD",
	                  stringf ("%i", 0));
#endif
}

/*
 * Create a new source if it didn't exist previously.
 *
 * This is a hack to be able to add sources dynamically
 * from plugins.
 */
BOOL download_make_source (Transfer *transfer,
                           char *user, char *hash, char *url)
{
	List     *ptr;
	Source   *tmp;
	Protocol *p;

	if (!(tmp = source_new (user, hash, transfer->total, url)))
		return FALSE;

	if (!(p = tmp->p))
		return FALSE;

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		Source *source = ptr->data;

		if (!source)
			continue;

		/* forget it if the source is already there */
		if (p->source_cmp (p, tmp, source) == 0)
		{
			source_free (tmp);
			return FALSE;
		}
	}

	source_free (tmp);

	/* this may fail, but doesn't affect return value */
	download_add_source (transfer, user, hash, url);

	return TRUE;
}

/*****************************************************************************/
/*
 * CHUNK RECORDING
 */

static int open_output (Transfer *transfer)
{
	assert (DOWNLOAD(transfer)->uniq != NULL);

	/* transfer->path may have already been supplied by a recovered state
	 * file, so we probably shouldnt mess w/ it */
	if (!transfer->path)
		transfer->path = STRDUP (INCOMING_PATH (("%s", DOWNLOAD(transfer)->uniq)));

	/* i'm not really sure why judge did this...if anyone has a problem
	 * with it, blame him */
	if (!(transfer->f = fopen (transfer->path, "rb+")) &&
		!(transfer->f = fopen (transfer->path, "wb")))
	{
		GIFT_ERROR (("Can't open %s for writing: %s", transfer->path,
		            GIFT_STRERROR()));
		return FALSE;
	}

	GIFT_TRACE (("%s", transfer->path));

	return TRUE;
}

/*****************************************************************************/

/* pads the file up to seek_pos */
static int pad_file (Transfer *transfer, off_t seek_pos)
{
#ifdef PAD_FILE
	off_t diff;
#endif

	/* no need to pad */
	if (seek_pos <= transfer->max_seek)
		return TRUE;

#ifndef PAD_FILE

	transfer->max_seek = seek_pos;

#else /* !PAD_FILE */

	/* seek to the current end */
	if (fseek (transfer->f, transfer->max_seek, SEEK_SET) == -1)
	{
		GIFT_ERROR (("fseek: %s", GIFT_STRERROR()));
		return FALSE;
	}

	/* actually null pad the file now */
	while ((diff = (seek_pos - transfer->max_seek)) > 0)
	{
		size_t n;   /* bytes written */
		size_t len;

		/* calculate the size of this write */
		len = MIN (diff, sizeof (null_pad));

		if ((n = fwrite (null_pad, sizeof (char), len, transfer->f)) < len)
		{
			GIFT_ERROR (("fwrite: %s", GIFT_STRERROR()));
			return FALSE;
		}

		/* adjust max seek */
		transfer->max_seek += n;
	}

#endif /* !PAD_FILE */

	return TRUE;
}

/* actually write chunk data to disk */
static int write_data (Transfer *transfer, Chunk *chunk,
                       char *segment, size_t len)
{
	off_t         seek_pos;
	signed long   remainder;
	size_t        written;

	if (!transfer || !chunk)
		return -1;

	seek_pos  = chunk->start + chunk->transmit;
	remainder = chunk->stop - seek_pos;

	/* make sure we aren't going to write past our alotted chunk */
	if (len > (size_t) remainder)
		len = (size_t) remainder;

	/* incremental padding
	 * NOTE: this code makes room in the file for a new chunk division */
	if (!(pad_file (transfer, seek_pos)))
		return -1;

	/* TODO - error handling */
	if (fseek (transfer->f, seek_pos, SEEK_SET) == -1)
	{
		GIFT_ERROR (("fseek: %s", GIFT_STRERROR()));
		return -1;
	}

	if ((written = fwrite (segment, sizeof (char), len, transfer->f)) < len)
	{
		GIFT_ERROR (("fwrite: %s", GIFT_STRERROR()));
		return -1;
	}

	chunk->transmit    += len; /* this chunk */
	chunk->tmp_recv    += len; /* temporary transmit recording */
	transfer->transmit += len; /* total transfer stats */

	remainder -= len;

	/* new seek max */
	if (seek_pos == transfer->max_seek)
		transfer->max_seek += len;

	/* -1 is reserved as an error from this function, so ensure that remainder
	 * is either 0 or greater (as this condition is not an error) */
	return (remainder >= 0 ? remainder : 0);
}

/* writes the data to disk, also handles chunk completion/failures */
void download_write (Chunk *chunk, unsigned char *segment, size_t len)
{
	Transfer *transfer  = chunk->transfer;
	int       remainder;

	/* protocol closed the TCPC (the chunk may have completed
	 * successfully) */
	if (!segment || len == 0)
	{
		relocate_source (chunk->source);
		return;
	}

	/* this cant possibly be receiving valid data
	 * TODO - assert? */
	if (!chunk->source)
		return;

	/* open the file for writing if we have not done so already */
	if (!transfer->f &&	!open_output (transfer))
	{
		download_pause (transfer);
		return;
	}

	if ((remainder = write_data (transfer, chunk, segment, len)) < 0)
	{
		/* write_data returns negative on fseek or fwrite errors...uh oh */
		GIFT_TRACE (("pausing %s: %s", transfer->filename,
		             GIFT_STRERROR ()));

		/* there is no fallback solution to this problem, so die rather than
		 * corrupt data */
		abort();

		download_pause (transfer);
		return;
	}

	if (chunk->source->status != SOURCE_ACTIVE)
		source_status_set (chunk->source, SOURCE_ACTIVE, NULL);

#ifdef THROTTLE_ENABLE
	if (MAX_DOWNLOAD_BW > 0)
		download_credits = len < download_credits ? download_credits - len : 0;
#endif /* THROTTLE_ENABLE */

	/* check to see if a chunk has completed */
	if (remainder == 0)
	{
		/* transfer has completed [successfully] */
		if (transfer->transmit >= transfer->total)
		{
			/* verify file and notify front-ends */
			download_complete (transfer);
			return;
		}

		/* put this source to work for us again */
		relocate_source (chunk->source);
	}
}

/*****************************************************************************/

BOOL download_sync (Transfer *transfer)
{
	int ret;

	/* this happens as download_state_save makes a few silly assumptions
	 * about whether or not the file should be synced */
	if (!transfer->f)
		return TRUE;

	if ((ret = fflush (transfer->f)) != 0)
	{
		GIFT_ERROR (("unable to fflush %s: %s",
		             transfer->path, GIFT_STRERROR()));
		return FALSE;
	}

#ifdef HAVE_FSYNC
	if ((ret = fsync (fileno (transfer->f))) != 0)
	{
		GIFT_ERROR (("unable to sync data to disc: %s", GIFT_STRERROR()));
		return FALSE;
	}
#endif /* HAVE_FSYNC */

	return TRUE;
}
