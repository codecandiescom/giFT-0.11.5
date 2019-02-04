/*
 * $Id: meta.c,v 1.26 2003/08/11 18:02:01 jasta Exp $
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

#include "plugin/share.h"

#include "lib/file.h"
#include "lib/mime.h"

#include "meta.h"
#include "meta/meta_mp3.h"
#include "meta/meta_ogg.h"
#include "meta/meta_image.h"
#include "meta/meta_avi.h"
#include "meta/meta_mpeg.h"

#include <ctype.h>

/*****************************************************************************/

typedef BOOL (*MetaHandlerFn) (Share *share, const char *path);

struct meta_handler
{
	char         *mime;                /* mime type to match */
	size_t        cmplen;              /* number of bytes to compare in mime */
	MetaHandlerFn handler;
}
meta_handlers[] =
{
	{ "audio/mpeg",      0, meta_mp3_run   },
#if 0
	{ "video/mpeg",      0, meta_mpeg_run  },   /* too broken to use */
#endif
	{ "audio/x-vorbis",  0, meta_ogg_run   },
	{ "image/",          6, meta_image_run },
	{ "video/x-msvideo", 0, meta_avi_run   },
	{ NULL,              0, NULL           },
};

/*****************************************************************************/

static BOOL run_handler (Share *share, const char *host_path)
{
	struct meta_handler *handler;
	BOOL ret;

	assert (share->mime != NULL);
	assert (host_path != NULL);

	for (handler = meta_handlers; handler->mime != NULL; handler++)
	{
		int cmp;

		if (handler->cmplen > 0)
			cmp = strncmp (handler->mime, share->mime, handler->cmplen);
		else
			cmp = strcmp (handler->mime, share->mime);

		/* matched, call the ahndler and abort */
		if (cmp == 0)
		{
			ret = handler->handler (share, host_path);
			return ret;
		}
	}

#if 0
	GIFT_TRACE (("no handler found for %s (%s)", host_path, share->mime));
#endif

	return FALSE;
}

BOOL meta_run (Share *share)
{
	char *host_path;
	BOOL  ret;

	if (!share)
		return FALSE;

	if (!(host_path = file_host_path (share->path)))
		return FALSE;

	/* if file->mime already exists it means that we loaded it from the cache,
	 * avoid recalculating it when not necessary */
	if (!share->mime)
	{
		/* HEx: you do not need to attach into mime.c at all anymore.  Figure
		 * the mime type out right here any way that you want. */
		if (!(share->mime = mime_type (host_path)))
		{
			free (host_path);
			return FALSE;
		}
	}

	ret = run_handler (share, host_path);
	free (host_path);

	/* why arent we supposed to return ret here?  what the hell? */
	return TRUE;
}
