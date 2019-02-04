/*
 * $Id: meta_mp3.c,v 1.2 2003/07/08 15:11:12 jasta Exp $
 *
 * This code was stolen from Gnapster and is outdated and inaccurate.
 * Someone please rewrite/update this.
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

#include "meta.h"
#include "meta_mp3.h"
#include "meta_tag.h"

#ifdef WIN32
# include <io.h> /* read() */
#endif /* WIN32 */

/*****************************************************************************/

/* TODO -- make sure this is correct later...Gnapster's code sucks */
#define PACK(x) ((x[0] << 24) | (x[1] << 16) | (x[2] << 8) | (x[3]))

/*****************************************************************************/

#define VBR_FRAME_FLAG 0x0001

typedef struct
{
	unsigned char  emphasis : 2;
	unsigned char  original : 1;
	unsigned char  copyright : 1;
	unsigned char  mode_ext : 2;
	unsigned char  mode : 2;
	unsigned char  private_bit : 1;
	unsigned char  padding_bit : 1;
	unsigned char  frequency : 2;
	unsigned char  bitrate_index : 4;
	unsigned char  protection_bit : 1;
	unsigned char  layer : 2;
	unsigned char  id : 1;
	unsigned char  index : 1;
	unsigned short frame_sync : 11; /* all bits set */
} MP3Header;

/*****************************************************************************/

static unsigned short bitrates[2][3][15] =
{
	{
		{ 0, 32, 48, 56, 64,  80,  96,  112, 128, 144, 160, 176, 192, 224, 256 },
		{ 0, 8,  16, 24, 32,  40,  48,  56,  64,  80,  96,  112, 128, 144, 160 },
		{ 0, 8,  16, 24, 32,  40,  48,  56,  64,  80,  96,  112, 128, 144, 160 }
	},
	{
		{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
		{ 0, 32, 48, 56, 64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 384 },
		{ 0, 32, 40, 48, 56,  64,  80,  96,  112, 128, 160, 192, 224, 256, 320 }
	},
};

static unsigned short frequencies[2][2][3] =
{
	{
		{ 11025, 12000, 8000 },
		{ 0,     0,     0 },
	},
	{
		{ 22050, 24000, 16000 },
		{ 44100, 48000, 32000 }
	}
};

/*****************************************************************************/

/* TODO: should return ssize_t */
static int read_byte (int fd, uint32_t *hdr)
{
	unsigned char c;
	ssize_t       n;

	if ((n = read (fd, &c, 1)) <= 0)
		return (int)n;

	*hdr = (*hdr << 8) | c;

	return (int)n;
}

static unsigned int get_bits (uint32_t *hdr, int bits)
{
	unsigned int x;

	x = (*hdr >> (32 - bits));

	*hdr <<= bits;

	return x;
}

#if 0
/* this function submitted by: mblomenkamp@web.de */
static int vbr_frame_count (unsigned char *hdr)
{
	int h_id, h_mode, h_flags;

	h_id   = (hdr[1] >> 3) & 0x01;
	h_mode = (hdr[3] >> 6) & 0x03;

	if (h_id)
		hdr += (h_mode == 3) ? 17 + 4 : 32 + 4;
	else
		hdr += (h_mode == 3) ? 9 + 4 : 17 + 4;

	if (strncmp (hdr, "Xing", 4))
		return 0;

	hdr += 4;

	h_flags = PACK (hdr);
	hdr += 4;

	return (h_flags & VBR_FRAME_FLAG) ? PACK (hdr) : 0;
}
#endif

static int find_mp3_header (int fd, uint32_t *hdr)
{
	int offs, n;

	offs = 0;

	while ((n = read_byte (fd, hdr)))
	{
		if ((((*hdr >> 16) & 0xffe0) == 0xffe0) &&
			(((*hdr >> 16) & 0x00ff) != 0x00ff))
			return (offs - 3);

		/* sanity check */
		if (offs > 300000)
			break;

		offs += n;
	}

	return -1;
}

static MP3Header *get_mp3_header (uint32_t hdr)
{
	MP3Header *h;

	if (!(h = malloc (sizeof (MP3Header))))
		return NULL;

	memset (h, 0, sizeof (MP3Header));

	/* skip frame sync */   get_bits (&hdr, 11);
	h->index              = get_bits (&hdr, 1);
	h->id                 = get_bits (&hdr, 1);
	h->layer              = get_bits (&hdr, 2);
	h->protection_bit     = get_bits (&hdr, 1);
	h->bitrate_index      = get_bits (&hdr, 4);
	h->frequency          = get_bits (&hdr, 2);
	h->padding_bit        = get_bits (&hdr, 1);
	h->private_bit        = get_bits (&hdr, 1);
	h->mode               = get_bits (&hdr, 2);
	h->mode_ext           = get_bits (&hdr, 2);
	h->copyright          = get_bits (&hdr, 1);
	h->original           = get_bits (&hdr, 1);
	h->emphasis           = get_bits (&hdr, 2);

	if (!h->mode)
		h->mode_ext = 0;

	return h;
}

static int process_mp3_header (FileShare *file, int fd, uint32_t hdr)
{
	MP3Header     *h;
	unsigned short bitrate;
	unsigned short freq;
	time_t         dur = 0;

	if (!(h = get_mp3_header (hdr)))
		return FALSE;

	/* sanity check so that we dont overrun the bitrates/frequencies table */
	if (h->id > 1 || h->layer == 0 || h->layer > 2 || h->bitrate_index > 14)
	{
		free (h);
		return FALSE;
	}

	bitrate = bitrates[h->id][3 - h->layer][h->bitrate_index];
	freq    = frequencies[h->index][h->id][h->frequency];

	/* calculate total running time (dur) */
	if (freq > 0)
	{
		int fsize;
		int fnum;

		fsize = (((h->id ? 144000 : 72000) * bitrate) / freq);
		fnum  = (file->size / (fsize + 1)) - 1;
		dur   = (time_t) (fnum * (h->id ? 1152 : 576) / freq);
	}

	free (h);

	share_set_meta (file, "bitrate",   stringf ("%li", (long)(bitrate * 1000)));
	share_set_meta (file, "frequency", stringf ("%hu", freq));
	share_set_meta (file, "duration",  stringf ("%i", (int)dur));

	return TRUE;
}

static int process_header (FileShare *file, const char *path)
{
	FILE         *f;
	unsigned long offs;
	uint32_t      hdr = 0;

	if (!(f = fopen (path, "rb")))
	{
		GIFT_ERROR (("can't open %s: %s", path, GIFT_STRERROR ()));
		return FALSE;
	}

	if ((offs = find_mp3_header (fileno (f), &hdr)) < 0)
	{
		fclose (f);
		return FALSE;
	}

	process_mp3_header (file, fileno (f), hdr);

	fclose (f);

	return TRUE;
}

/*****************************************************************************/

BOOL meta_mp3_run (Share *share, const char *path)
{
	process_header (share, path);

	/* process id3v1/id3v2 tags */
	meta_tag_run (share, path);

	return TRUE;
}
