/*
 * $Id: meta_mpeg.c,v 1.3 2003/07/08 15:11:12 jasta Exp $
 *
 * This file is a mess.  Blame the contributor, np_ <napalm@arnet.com.ar>.
 *
 * Modified by the giFT project developers.  Original ownership of the code
 * (that is, where np_ yanked it from) is unknown.
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
#include "meta_mpeg.h"

#include <math.h>

/*****************************************************************************/

typedef struct
{
	unsigned int width;                 /* width in pixels */
	unsigned int height;                /* height in pixels */
	double       fps;                   /* frames per second */
	unsigned int duration;              /* duration in milliseconds */
	unsigned int bitrate;               /* bitrate in kbps */
	char        *codec;                 /* video compression codec */
} MpegData;

/*****************************************************************************/

static double round_double(double num)
{
	return (floor (num + 0.5));
}

/*****************************************************************************/

#if 0
static uint32_t fread_le (FILE *file, int bytes)
{
	int x;
	uint32_t result = 0;

	assert (bytes <= sizeof (result));

	for (x = 0; x < bytes; x++)
		result |= (getc(file) << (x*8));

	return result;
}
#endif

static uint32_t fread_be (FILE *file, int bytes)
{
	int x;
	uint32_t result = 0;

	assert (bytes <= sizeof (result));

	for (x = bytes - 1; x >= 0; x--)
		result |= (getc(file) << (x*8));

	return result;
}

/*****************************************************************************/

static int parse_mpeg (FILE *file, MpegData *data)
{
	int version = 0;                    /* MPEG-1/2; our return value */
	uint32_t temp;

	temp = fread_be (file, 4);

	if (temp == 0x000001BA)
	{
		/* figure out if this is an MPEG-1 or MPEG-2*/
		temp = (uint32_t)(fgetc (file));

		if ((temp & 0xF0) == 0x20)
			version = 1;
		else if ((temp & 0xC0) == 0x40)
			version = 2;
		else
			return 0;

		if(version == 1)
		{
			fseek (file, 4L, SEEK_CUR);
			data->bitrate = (unsigned int)(round_double((double)((fread_be(file, 3) & 0x7FFFFE) >> 1) * 0.4));
		}
		else
		{
			fseek (file, 5L, SEEK_CUR);
			data->bitrate = (unsigned int)(round_double((double)((fread_be(file, 3) & 0xFFFFFC) >> 2) * 0.4));

			/* stuffing bytes */
			temp = fgetc (file) & 0x07;
			if(temp != 0)
				fseek (file, (long) temp, SEEK_CUR);
		}

		temp = fread_be (file, 4);
		while(temp != 0x000001BA && temp != 0x000001E0)
		{
			if (feof (file))           /* shouldn't happen */
				return version;

			if (temp == 0x00000000)    /* skip past zero padding */
			{
				while ((temp & 0xFFFFFF00) != 0x00000100)
				{
					if (feof (file))   /* shouldn't happen */
						return version;

					temp <<= 8;
					temp |= fgetc (file);
				}
			}
			else
			{
				temp = fread_be (file, 2);
				fseek (file, (long)temp, SEEK_CUR);
				temp = fread_be (file, 4);
			}
		}

		temp = fread_be (file, 4);
		while(temp != 0x000001B3)
		{
			if (feof (file))           /* no seq. header; shouldn't happen */
				return version;

			temp <<= 8;
			temp |= fgetc (file);
		}
	}
	else                               /* video stream only */
	{
		fseek (file, 4L, SEEK_SET);
	}


	temp = fread_be (file, 3);
	data->width = (temp & 0xFFF000) >> 12;
	data->height = temp & 0x000FFF;

	switch (fgetc (file) & 0x0F)
	{
	 case 1:  data->fps = 23.976;  break;
	 case 2:  data->fps = 24.000;  break;
	 case 3:  data->fps = 25.000;  break;
	 case 4:  data->fps = 29.970;  break;
	 case 5:  data->fps = 30.000;  break;
	 case 6:  data->fps = 50.000;  break;
	 case 7:  data->fps = 59.940;  break;
	 case 8:  data->fps = 60.000;  break;
	}

	/* if this is a video-only stream... */
	if (data->bitrate == 0)
	{
		/* get bitrate from here */
		temp = (fread_be (file, 3) & 0xFFFFC0) >> 6;

		/* variable bitrate */
		if (temp != 0x3FFFF)
			data->bitrate = (unsigned int)(round_double((double)temp * 0.4));
	}
	else
	{
		fseek (file, 3L, SEEK_CUR);
	}

	if (version != 1)
	{
		temp = fgetc (file);

		if (temp & 0x02)
		{
			fseek(file, 63L, SEEK_CUR);
			temp = fgetc (file);
		}

		if (temp & 0x01)
			fseek (file, 64L, SEEK_CUR);

		temp = fread_be (file, 4);
		if (temp == 0x000001B5)
		{
			if (version == 0)
				version = 2;

			fseek (file, 1L, SEEK_CUR);

			/* extensions specify MSBs of width/height */
			temp = fread_be (file, 2);
			data->width  |= (temp & 0x0180) << 5;
			data->height |= (temp & 0x0060) << 7;

			fseek (file, 2L, SEEK_CUR);

			/* and a numerator/denominator multiplier for fps */
			temp = fgetc (file);
			if ((temp & 0x60) && (temp & 0x1F))
				data->fps = (unsigned int)(round_double((double)data->fps * (temp & 0x60)/(temp & 0x1F)));
		}
		else if (version == 0)
			version = 1;
	}

	return version;
}

/*****************************************************************************/

static BOOL video_file_analyze (Share *share, const char *path)
{
	FILE *file;
	int version;
	char fmt[10];
	MpegData data;

	memset (&data, 0, sizeof (data));

	if (!(file = fopen (path, "rb")))
		return FALSE;

	fmt[0] = 0;

	switch ((version = parse_mpeg (file, &data)))
	{
	 case 1:  strcpy (fmt, "MPEG-1");  break;
	 case 2:  strcpy (fmt, "MPEG-2");  break;
	}

	if (data.bitrate == 0 && data.duration != 0)
	{
		fseek (file, 0L, SEEK_END);
		data.bitrate = (unsigned int)(round_double((double)ftell(file) / data.duration * 8));
	}
	else if (data.duration == 0 && data.bitrate != 0)
	{
		fseek (file, 0L, SEEK_END);
		data.duration = (unsigned int)(round_double((double)ftell(file) / data.bitrate * 8));
	}

	fclose (file);

	if (fmt[0])
		share_set_meta (share, "format", fmt);

	if (data.width > 0)
		share_set_meta (share, "width", stringf ("%u", data.width));

	if (data.height > 0)
		share_set_meta (share, "height", stringf ("%u", data.height));

	if (data.fps > 0)
		share_set_meta (share, "fps", stringf ("%.03f", data.fps));

	if (data.duration > 0)
		share_set_meta (share, "duration", stringf ("%u", data.duration / 1000));

	if (data.bitrate > 0)
		share_set_meta (share, "bitrate", stringf ("%u", data.bitrate * 1000));

	/* this is never set, apparently */
	if (data.codec)
	{
		share_set_meta (share, "codec", data.codec);
		free (data.codec);
	}

	return TRUE;
}

/*****************************************************************************/

BOOL meta_mpeg_run (Share *share, const char *path)
{
	return video_file_analyze (share, path);
}
