/*
 * $Id: meta_image.c,v 1.3 2003/06/28 08:42:01 jasta Exp $
 *
 * submitted by: asm@dtmf.org
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

#include "meta.h"
#include "meta_image.h"

#ifdef USE_IMAGEMAGICK
#include <magick/api.h>
#endif

/*****************************************************************************/

#ifdef USE_IMAGEMAGICK
static int set_image_info (FileShare *file, char *path, ExceptionInfo *excp)
{
	Image     *img;
	ImageInfo *info;

	/* create context */
	if (!(info = CloneImageInfo (NULL)))
		return FALSE;

	strcpy (info->filename, path);

	/* open image */
	if (!(img = ReadImage (info, excp)))
	{
		GIFT_ERROR (("ImageMagick can't process image %s: %s (%s)",
		             info->filename, excp->reason, excp->description));
		DestroyImageInfo (info);
		return FALSE;
	}

	share_set_meta (file, "width",  stringf ("%i", (int)img->columns));
	share_set_meta (file, "height", stringf ("%i", (int)img->rows));

	DestroyImage (img);
	DestroyImageInfo (info);
	return TRUE;

}
#endif /* USE_IMAGEMAGIC */

BOOL meta_image_run (Share *share, const char *path)
{
#ifdef USE_IMAGEMAGICK
	ExceptionInfo excp;

	/* initialize */
	InitializeMagick (NULL);
	GetExceptionInfo (&excp);

	/* set the info giFT cares about */
	set_image_info (share, (char *)path, &excp);

	/* clean up */
	DestroyExceptionInfo (&excp);
	DestroyMagick ();
#endif /* USE_IMAGEMAGICK */

	return TRUE;
}
