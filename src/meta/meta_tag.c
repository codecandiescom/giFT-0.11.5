/*
 * $Id: meta_tag.c,v 1.2 2003/06/27 10:06:59 hipnod Exp $
 *
 * Copyright (C) 20003 giFT project (gift.sourceforge.net)
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
#include "meta_tag.h"

#include <ctype.h>

/*****************************************************************************/

/*
 * This file parses id3v1 and id3v2 tags.
 *
 * TODO:
 *		- parse 16-bit Unicode strings and convert those that are
 *		  really just big ASCII strings (or is that ISO-8859-1?)
 *		- MusicMatch jukebox tagging support
 *		- Lyrics3 tagging support
 *		- compressed frames; compressed metaframes (CDM)
 *
 * If anyone wants to do these things, please feel free.
 */

/*****************************************************************************/

#define ID3V2_HDR_LEN                10
#define ID3V2_FOOTER_LEN             10

#define ID3V2_EXT_HDR_LEN            6
#define ID3V2_FRAME_HDR_MAX          10

/* maximum amount of memory used in reading frames */
#define DEFAULT_MAX_FRAME_LEN        131072

/* improve readability */
#define TAG_FILE(tag)                (tag)->file

/*****************************************************************************/

/* #define DEBUG_META_TAG 1 */

#ifdef DEBUG_META_TAG
#define META_TAG_TRACE(args)         GIFT_TRACE(args)
#else
#define META_TAG_TRACE(args)
#endif

/*****************************************************************************/

struct tag_file
{
	FILE              *f;
	char              *name;
	off_t              size;
	Dataset           *keys;
	struct id3v1      *id3v1;
	struct id3v2      *id3v2;
#if 0
	struct musicmatch *musicmatch;         /* TODO */
	struct lyrics3    *lyrics3;            /* TODO */
#endif
	Share             *share;
};

struct id3v1
{
	struct tag_file *file;
	off_t            start;
};

struct id3v2
{
	struct tag_file *file;
	off_t            start;              /* start position of tag */
	size_t           max_len;            /* max buffer size */

	size_t           data_len;
	size_t           ext_hdr_len;        /* length of the ext hdr */
	size_t           size;               /* data_len + hdrs */
	size_t           data_offset;        /* offset in tag, from tag->v2_start */
	size_t           fake_offset;        /* equal to data_offset except when
	                                      * tag is unsynchronized */
	BOOL             unsynched;          /* whole tag unsynch-encoded */
	BOOL             compressed;
	BOOL             has_ext_hdr;        /* has extended header */
	BOOL             experimental;
	BOOL             has_footer;         /* has a 10-byte footer */

	unsigned int     version;
};

struct id3v2_frame
{
	struct id3v2      *tag;           /* tag this frame belongs to */
	char               id[5];

	uint8_t           *contents;
	size_t             size;
	size_t             offset;

	BOOL               unsynched;
	BOOL               compressed;
	BOOL               encrypted;
	BOOL               grouped;       /* grouped with previous tag */
	BOOL               has_data_len;  /* has data length indicator */
};

/* callback for handling each frame */
typedef void (frame_parse_t) (struct id3v2 *tag, struct id3v2_frame *frame);

typedef struct frame_handler
{
	char           *v2_str;              /* 3 char id from id3v2.2  */
	char           *v3_str;              /* 4 char id from id3v2.3+ */
	frame_parse_t  *parse;               /* parse function */
} frame_handler_t;

/*****************************************************************************/

static void read_v2_frames   (struct id3v2 *tag);
static void read_v1          (struct id3v1 *tag);

static void set_meta         (struct tag_file *file, char *id, char *key,
                              char *value);

/*****************************************************************************/

#define ID3V2_FRAME_HANDLER(name) \
	static void name (struct id3v2 *tag, struct id3v2_frame *frame)

ID3V2_FRAME_HANDLER (id3v2_parse_TLEN);
ID3V2_FRAME_HANDLER (id3v2_parse_COMM);
ID3V2_FRAME_HANDLER (id3v2_parse_TIT2);
ID3V2_FRAME_HANDLER (id3v2_parse_TALB);
ID3V2_FRAME_HANDLER (id3v2_parse_TRCK);
ID3V2_FRAME_HANDLER (id3v2_parse_TPE1);
ID3V2_FRAME_HANDLER (id3v2_parse_TYER);

/*****************************************************************************/

/* list of handled id3v2 frames */
static frame_handler_t frame_handlers[] =
{
	{ "TLE", "TLEN", id3v2_parse_TLEN }, /* duration */
	{ "COM", "COMM", id3v2_parse_COMM }, /* comment */
	{ "TT2", "TIT2", id3v2_parse_TIT2 }, /* title */
	{ "TAL", "TALB", id3v2_parse_TALB }, /* album */
	{ "TRK", "TRCK", id3v2_parse_TRCK }, /* track */
	{ "TP1", "TPE1", id3v2_parse_TPE1 }, /* artist */
	{ "TYE", "TYER", id3v2_parse_TYER }, /* year */
	{ NULL, NULL, NULL }
};

/*****************************************************************************/

/* get bytes in big-endian ordering */
static uint32_t get_bytes (unsigned char **p, size_t len)
{
	int             i;
	uint32_t        bytes;
	unsigned char  *ptr;

	assert (len <= 4);
	bytes = 0;
	ptr   = *p;

	for (i = 0; i < len; i++)
		bytes = (bytes << 8) | (*ptr++);

	*p = ptr;
	return bytes;
}

/*
 * Read 'len' "sync-safe" bytes, that were encoded by making
 * sure the leading bit of each byte is 0.
 */
static size_t get_sync_safe (unsigned char **ptr, size_t len)
{
	size_t          decoded;
	unsigned char  *p;

	assert (len == 4);
	p = *ptr;

	decoded = ((p[0] & 0x7f) << 21) | ((p[1] & 0x7f) << 14) |
	          ((p[2] & 0x7f) << 7)  | ((p[3] & 0x7f));

	*ptr += len;
	return decoded;
}

/* this will be used in scanning for lyrics3 and musicmatch tags */
#if 0
/* look for a particular byte pattern in the file */
static off_t scan_signature (struct tag_file *tag, char *signature, off_t start)
{
	int    c;
	char  *p;
	off_t  pos;

	if (fseek (tag->f, start, SEEK_SET) != 0)
		return -1;

	pos = start;
	p   = signature;

	while (*p && (c = fgetc (tag->f)) != EOF)
	{
		pos++;

		if (c == *p)
			p++;
		else
			p = signature;
	}

	/* if we reached the end of the signature we found it */
	return (*p == 0 ? pos - 3 : -1);
}
#endif

/*****************************************************************************/
/* ID3v1 support */

static struct id3v1 *id3v1_alloc (struct tag_file *file)
{
	struct id3v1 *tag;

	if (!(tag = MALLOC (sizeof (struct id3v1))))
		return NULL;

	tag->file = file;
	return tag;
}

static void id3v1_free (struct id3v1 *tag)
{
	if (!tag)
		return;

	if (TAG_FILE(tag) && TAG_FILE(tag)->id3v1 == tag)
		TAG_FILE(tag)->id3v1 = NULL;

	free (tag);
}

static char *v1_str (unsigned char *p, size_t len)
{
	static char   v1_buf[64];
	size_t        i;

	for (i = 0; i < len; i++)
	{
		unsigned char c = p[i];

		/* check if it was terminated, and i think 0xff is not valid */
		if (c == 0 || c == 0xff)
			break;

		v1_buf[i] = p[i];
	}

	v1_buf[i] = 0;
	string_trim (v1_buf);

	return v1_buf;
}

/* The ID3v1 tag is always 128 bytes long and somewhere before
 * the end of the file */
static void read_v1 (struct id3v1 *tag)
{
	char            buf[512];
	unsigned char  *p;
	size_t          comment_len;
	char           *str;

	if (fread (buf, 1, 128, TAG_FILE(tag)->f) != 128)
		return;

	if (strncmp (buf, "TAG", 3) != 0)
		return;

	/* if the first char is not printable or empty, assume a bogus TAG */
	p = buf + 3;
	if (!isprint (*p) && *p != 0)
		return;

	tag->start = ftell (TAG_FILE(tag)->f) - 128;

	set_meta (TAG_FILE(tag), NULL, "title",   v1_str (p, 30)); p += 30;
	set_meta (TAG_FILE(tag), NULL, "artist",  v1_str (p, 30)); p += 30;
	set_meta (TAG_FILE(tag), NULL, "album",   v1_str (p, 30)); p += 30;
	set_meta (TAG_FILE(tag), NULL, "year",    v1_str (p, 4));  p += 4;

	/* check for track number, which is indicated by having byte
	 * 29 of the comment field nulled (and a sane track number) */
	if (p[28] == 0 && p[29] > 0 && p[29] != 0xff)
		comment_len = 29;
	else
		comment_len = 30;

	set_meta (TAG_FILE(tag), NULL, "comment", v1_str (p, comment_len));
	p += comment_len;

	if (comment_len == 29)
	{
		str = stringf_dup ("%d", *p);
		set_meta (TAG_FILE(tag), NULL, "track", str);
		free (str);
		p++;
	}

	/* TODO: genre table? */
}

/*****************************************************************************/
/* ID3v2 support */

struct id3v2 *id3v2_alloc (struct tag_file *file, size_t max_len)
{
	struct id3v2 *tag;

	if (!(tag = MALLOC (sizeof (struct id3v2))))
		return NULL;

	tag->file = file;

	/* set the maximum frame size to read at any one time */
	if (max_len == 0)
		max_len = DEFAULT_MAX_FRAME_LEN;

	tag->max_len = max_len;
	return tag;
}

void id3v2_free (struct id3v2 *tag)
{
	if (!tag)
		return;

	if (TAG_FILE(tag) && TAG_FILE(tag)->id3v2 == tag)
		TAG_FILE(tag)->id3v2 = NULL;

	/* frames aren't stored, so there's nothing else to free */
	free (tag);
}

static struct id3v2_frame *id3v2_frame_alloc (struct id3v2 *tag, char *id)
{
	struct id3v2_frame *frame;

	if (!(frame = MALLOC (sizeof (struct id3v2_frame))))
		return NULL;

	frame->tag = tag;
	strcpy (frame->id, id);

	return frame;
}

static void id3v2_frame_free (struct id3v2_frame *frame)
{
	if (!frame)
		return;

	free (frame->contents);
	free (frame);
}

static int id3v2_major_version (struct id3v2 *tag)
{
	return (tag->version & 0xff00) >> 8;
}

#ifdef DEBUG_META_TAG /* XXX */
static int id3v2_minor_version (struct id3v2 *tag)
{
	return (tag->version & 0x00ff);
}
#endif

/*****************************************************************************/

/*
 * Read some data from the tag, reverse any unsynchronization replacing
 * sequences of [ff 00] with [ff], and copy the data to a buffer.
 *
 * NOTE: This method doesn't unsync sequences straddling the boundaries of
 * reads, so each read_resync() _must_ completely encompass all possible
 * unsynchronized sequences within a given subrange of the tag. Otherwise
 * the tag will not be read correctly.
 */
static size_t read_resync (struct id3v2 *tag, char *buf, size_t len)
{
	int     c;
	int     cprev;
	size_t  n;

	if (tag->data_offset >= tag->size)
		return 0;

	n     = 0;
	cprev = 0;

	while (tag->data_offset < tag->size && n < len)
	{
		if ((c = fgetc (TAG_FILE(tag)->f)) == EOF)
			break;

		tag->data_offset++;

		/* unsync 0xff00 sequence to 0xff (and also, 0xff0000 to 0xff00) */
		if (cprev == 0xff && c == 0x00)
		{
			cprev = 0;
			continue;
		}

		/* we read a byte */
		n++;

		if (buf)
			buf[n-1] = c;

		if (n >= len)
			break;

		cprev = c;
	}

	/* update the logical position in the tag */
	tag->fake_offset += n;

	return n;
}

/* read some data from the tag, possibly unsyncing it, into the supplied
 * buffer and update tag->data_offset */
static size_t id3_read (struct id3v2 *tag, void *buf, size_t need_len)
{
	size_t unsynced_len;

	if (!tag->unsynched || id3v2_major_version (tag) >= 4)
	{
		/*
		 * If the tag does not need unsynchronization, or if it
		 * needs unsync but is version 4, we can just do fread().
		 */
		unsynced_len = fread (buf, 1, need_len, TAG_FILE(tag)->f);

		tag->data_offset += unsynced_len;
		tag->fake_offset += unsynced_len;

		return unsynced_len;
	}

	/*
	 * The tag needs unsynchronization, and we've got to do it the
	 * hard way: one character at a time.
	 *
	 * tag->data_offset is handled by read_resync().
	 */
	unsynced_len = read_resync (tag, (char *)buf, need_len);
	return unsynced_len;
}

/*
 * Seek to a particular spot in the tag.
 *
 * If the tag is unsynchronized and the id3 major version is 2 or 3, we
 * have to fake doing a seek by doing id3_read(). We do this because in
 * those versions of the standard, all sizes in the tag except the tag size
 * are _logical_ sizes, and the real sizes can only be obtained after
 * unsyncing the whole tag.
 */
static int id3_seek (struct id3v2 *tag, size_t offset, int whence)
{
	size_t ret;
	size_t len;

	assert (whence == SEEK_SET);

	if (!tag->unsynched || id3v2_major_version (tag) >= 4)
	{
		if ((ret = fseek (TAG_FILE(tag)->f, (long)tag->start + offset,
		                  whence)) == 0)
		{
			tag->data_offset = offset;
			tag->fake_offset = offset;
		}

		return ret;
	}

	/* only do this for tags at the start of the file */
	if (tag->start != 0)
		return -1;

	/* can't seek backwards */
	if (tag->fake_offset > offset)
		return -1;

	len = offset - tag->fake_offset;
	ret = id3_read (tag, NULL, len);

	if (ret != len)
		return -1;

	return 0;
}

/* UNTESTED */
/* TODO: limit extended header size to a sane limit */
static BOOL skip_ext_header (struct id3v2 *tag, size_t start)
{
	unsigned char    buf[ID3V2_EXT_HDR_LEN];
	unsigned char   *p;
	size_t           newpos;

	if (!TAG_FILE(tag)->f)
		return FALSE;

	if (id3_seek (tag, start, SEEK_SET) != 0)
		return FALSE;

	if (id3_read (tag, buf, ID3V2_EXT_HDR_LEN) != ID3V2_EXT_HDR_LEN)
		return FALSE;

	p = buf;

	/*
	 * v2.2.0-2.3.0: header size is 4 byte int
	 * v2.4.0: header size is sync-safe
	 */
	if (id3v2_major_version (tag) >= 4)
		tag->ext_hdr_len = get_sync_safe (&p, 4);
	else
		tag->ext_hdr_len = get_bytes (&p, 4);

	newpos = start + tag->ext_hdr_len;

	/*
	 * Sanity check the extended header size: if we have unsynchronize
	 * the header, make sure it isn't humongous. We don't want to fgetc()
	 * on 20MB, say.
	 */
	if (tag->unsynched && id3v2_major_version (tag) < 4)
	{
		if (tag->ext_hdr_len > tag->max_len)
		    return FALSE;
	}

	/*
	 * Skip the header, as it doesn't have any frames we care
	 * about at the moment (crc32, update flag, ...)
	 */
	if (id3_seek (tag, newpos, SEEK_SET) != 0)
		return FALSE;

	return TRUE;
}

static BOOL unknown_tag_flags (struct id3v2 *tag, int flags)
{
	assert (id3v2_major_version (tag) >= 2 && id3v2_major_version (tag) <= 4);

	/* v2: xx000000. bit 5 represents some weird compression scheme
	 * using a CDM frame that is not supported. */
	if (id3v2_major_version (tag) == 2)
		return (flags & 0x3f) != 0;

	/* v3: xxx00000 */
	if (id3v2_major_version (tag) == 3)
		return (flags & 0x1f) != 0;

	/* v4: xxxx0000 */
	return (flags & 0x0f) != 0;
}

static void set_tag_flags (struct id3v2 *tag, int flags)
{
	tag->unsynched    = FALSE;
	tag->compressed   = FALSE;
	tag->has_ext_hdr  = FALSE;
	tag->experimental = FALSE;
	tag->has_footer   = FALSE;

	tag->unsynched = (flags & 0x80) != 0;

	if (id3v2_major_version (tag) == 2)
		tag->compressed = (flags & 0x40) != 0;
	else
		tag->has_ext_hdr = (flags & 0x40) != 0;

	tag->experimental = (flags & 0x20) != 0;
	tag->has_footer   = (flags & 0x10) != 0;
}

static BOOL parse_v2_header (struct id3v2 *tag, unsigned char *hdr,
                             char *signature)
{
	int              i;
	int              not_found;
	unsigned char   *h;
	size_t           sig_len;
	unsigned int     flags;

	h = hdr;
	not_found = 0;

	sig_len = strlen (signature);

	/* sanity check the values in the header */
	not_found = strncmp (h, signature, sig_len);
	h += sig_len;

	/* {major, minor} version: should _not_ equal 0xff */
	for (i = 0; i < 2; i++)
		not_found |= *h++ == 0xff;

	/* flags: must have none of the lower 4 bits set (according to
	 * version 4 of the standard) */
	not_found |= (*h++ & 0xf);

	/* the size of the tag: must have highest bit clear */
	for (i = 0; i < 4; i++)
		not_found |= (*h++ & 0x80);

	if (not_found)
		return FALSE;

	META_TAG_TRACE (("---: [%s] found \"%s\"\n", TAG_FILE(tag)->name,
	                 signature));

	h = hdr + sig_len;

	/* get the version field */
	tag->version = get_bytes (&h, 2);

	META_TAG_TRACE (("version = ID3v2.%u.%u\n", id3v2_major_version (tag),
	                 id3v2_minor_version (tag)));

	/* support v2.2.0-v2.4.x */
	if (id3v2_major_version (tag) < 2 || id3v2_major_version (tag) > 4)
		return FALSE;

	flags = *h++;

	/* if the tag has any weird flags set, bail */
	if (unknown_tag_flags (tag, flags))
	{
		META_TAG_TRACE (("unknown tag flags: %x\n", flags));
		return FALSE;
	}

	set_tag_flags (tag, flags);

	META_TAG_TRACE (("[%s]: unsync_tag: %i extended: %i experimental: %i "
	                 "has_footer: %i\n", TAG_FILE(tag)->name, tag->unsynched,
	                 tag->has_ext_hdr, tag->experimental, tag->has_footer));

	/*
	 * The length of the whole tag is encoded as a "sync-safe"
	 * integer, where the highest bit of each byte is clear, yielding
	 * a 28-bit integer with a max tag length of 256 MB.
	 */
	tag->data_len = get_sync_safe (&h, 4);
	tag->size = tag->data_len + ID3V2_HDR_LEN;

	if (tag->has_footer)
		tag->size += ID3V2_FOOTER_LEN;

	return TRUE;
}

static void read_v2 (struct id3v2 *tag, char *signature)
{
	unsigned char    header[ID3V2_HDR_LEN];
	off_t            pos;

	if (!TAG_FILE(tag)->f)
		return;

	if (fread (header, 1, ID3V2_HDR_LEN, TAG_FILE(tag)->f) != ID3V2_HDR_LEN)
		return;

	if (!parse_v2_header (tag, header, signature))
		return;

	if ((pos = ftell (TAG_FILE(tag)->f)) == -1)
		return;

	/* store the tag start position */
	tag->start = pos - ID3V2_HDR_LEN;

	/* put the real and virtual offsets after the header */
	tag->data_offset = ID3V2_HDR_LEN;
	tag->fake_offset = ID3V2_HDR_LEN;

	/* only accept version 4 tags anywhere but the start */
	if (tag->start != 0 && id3v2_major_version (tag) != 4)
		return;

	/*
	 * If this was a footer we read (indicated by looking for
	 * backwards "ID3"), then restart the tag search at the
	 * beginning of the tag.
	 */
	if (!strncmp (signature, "3DI", 3))
	{
		off_t newpos = pos - tag->size;

		if (fseek (TAG_FILE(tag)->f, newpos, SEEK_SET) != 0)
			return;

		/* restart the read at the new position */
		read_v2 (tag, "ID3");
		return;
	}

	/* can't handle compressed tags yet */
	if (tag->compressed)
		return;

	META_TAG_TRACE (("taglength=%u (0x%x)\n", tag->data_len, tag->data_len));

	if (tag->has_ext_hdr && !skip_ext_header (tag, ID3V2_HDR_LEN))
		return;

	read_v2_frames (tag);
}

/*****************************************************************************/
/* Frame reading */

static size_t frame_hdr_len (struct id3v2 *tag)
{
	if (id3v2_major_version (tag) == 2)
		return 6;

	return ID3V2_FRAME_HDR_MAX;
}

static size_t frame_id_len (struct id3v2 *tag)
{
	if (id3v2_major_version (tag) == 2)
		return 3;

	return 4;
}

static BOOL is_frameid_char (char c)
{
	if (c >= 'A' && c <= 'Z')
		return TRUE;
	else if (c >= '0' && c <= '9')
		return TRUE;

	return FALSE;
}

static uint8_t unknown_frame_flags (struct id3v2 *tag, int flags1, int flags2)
{
	assert (id3v2_major_version (tag) >= 2 &&
	        id3v2_major_version (tag) <= 4);

	if (id3v2_major_version (tag) == 2)
		return 0;

	/* v2.3.0: 11100000 11100000 where 1's are significant */
	if (id3v2_major_version (tag) == 3)
		return (flags1 & 0x1f) | (flags2 & 0x1f);

	/* v2.4.0: 01110000 01001111, changed to avoid false framesyncs */
	return (flags1 & 0x8f) | (flags2 & 0xb0);
}

static void set_frame_flags (struct id3v2_frame *frame, int flags1, int flags2)
{
	struct id3v2 *tag = frame->tag;

	assert (id3v2_major_version (tag) >= 2 &&
	        id3v2_major_version (tag) <= 4);

	/* v2.2.0: no flags */
	if (id3v2_major_version (tag) == 2)
		return;

	if (id3v2_major_version (tag) == 3)
	{
		frame->compressed = (flags2 & 0x80) != 0;
		frame->encrypted  = (flags2 & 0x40) != 0;
		frame->grouped    = (flags2 & 0x20) != 0;
		return;
	}

	frame->grouped      = (flags2 & 0x40) != 0;
	frame->compressed   = (flags2 & 0x08) != 0;
	frame->encrypted    = (flags2 & 0x04) != 0;
	frame->unsynched    = (flags2 & 0x02) != 0;
	frame->has_data_len = (flags2 & 0x01) != 0;
}

static BOOL frame_resize_buffer (struct id3v2_frame *frame, size_t len)
{
	unsigned char *newbuf;

	if (len <= frame->size)
		return TRUE;

	if (!(newbuf = realloc (frame->contents, len)))
		return FALSE;

	frame->size     = len;
	frame->contents = newbuf;

	return TRUE;
}

static frame_handler_t *find_handler (frame_handler_t *p, char *id)
{
	if (!id)
		return NULL;

	while (p->parse != NULL)
	{
		if (!STRCMP (id, p->v2_str) || !STRCMP (id, p->v3_str))
			return p;

		p++;
	}

	return NULL;
}

/* read the frame headers and store their offsets in the file */
static BOOL read_frame (struct id3v2 *tag, struct id3v2_frame *frame,
                        frame_handler_t *handler, size_t len)
{
	ssize_t  n;

	/* can't handle compressed or encrypted frames */
	if (!handler || frame->encrypted || frame->compressed)
		return FALSE;

	/* check if this frame would exceed memory allotment
	 * (+1 to allow for null terminator) */
	if (len + 1 > tag->max_len)
	{
		META_TAG_TRACE (("%s EXCEEDS max len (%u > %u)\n",
		                 frame->id, len, tag->max_len));
		return FALSE;
	}

	if (!frame_resize_buffer (frame, len + 1))
		return FALSE;

	/*
	 * Version 4 of the standard allows for avoiding all the unsync
	 * problems, but we have to unsync the frame contents here
	 * instead of dynamically in id3_read().
	 */
	if ((tag->unsynched || frame->unsynched) && id3v2_major_version (tag) >= 4)
		n = read_resync (tag, frame->contents, len);
	else
		n = id3_read (tag, frame->contents, len);

	if (n != len)
	{
		META_TAG_TRACE (("FREAD failed: %d returned (%d)\n", n, len));
		return FALSE;
	}

	/* null terminate the buffer */
	frame->contents[len] = 0;

	/* call the handler */
	handler->parse (tag, frame);

	return TRUE;
}

static void read_v2_frames (struct id3v2 *tag)
{
	unsigned char        buf[ID3V2_FRAME_HDR_MAX];
	unsigned char        frame_id[5];
	size_t               id_len;
	size_t               hdr_len;
	size_t               pos;
	struct id3v2_frame  *frame;

	if (!TAG_FILE(tag)->f)
		return;

	id_len  = frame_id_len  (tag);
	hdr_len = frame_hdr_len (tag);

	META_TAG_TRACE (("frame data size=%lu\n",
	                 (long)tag->size - tag->data_offset));

	/* get the current logical position in the tag */
	pos = tag->fake_offset;

	while (tag->data_offset < tag->size)
	{
		unsigned char    *p;
		size_t            data_len;
		int               i;
		frame_handler_t  *handler;

		if (id3_seek (tag, pos, SEEK_SET) != 0)
			return;

		if (tag->data_offset >= tag->size)
			break;

		/* read the frame header */
		if (id3_read (tag, buf, hdr_len) != hdr_len)
			return;

		memcpy (frame_id, buf, id_len);
		frame_id[id_len] = 0;

		/* check for a frame id (possibly padding) without ascii characters */
		for (i = 0; i < id_len; i++)
			if (!is_frameid_char (frame_id[i]))
			    return;

		p = buf + id_len;

		/*
		 * v2.2.0 - 3 bytes length field
		 * v2.3.0 - 4 bytes ""
		 * v2.4.0 - sync-safe
		 */
		switch (id3v2_major_version (tag))
		{
			case 2: data_len = get_bytes     (&p, 3); break;
			case 3: data_len = get_bytes     (&p, 4); break;
			case 4: data_len = get_sync_safe (&p, 4); break;
			default: assert (0);
		}

		/*
		 * Could try to continue parsing here. The most likely occurrence
		 * though is that the tag is corrupt and we're in the middle of
		 * garbage.
		 */
		if (unknown_frame_flags (tag, buf[8], buf[9]))
		{
			META_TAG_TRACE (("FUCK! bad flags: %x %x\n", buf[8], buf[9]));
			break;
		}

		/* increment the logical position in the tag */
		pos += hdr_len + data_len;

		META_TAG_TRACE (("[%d bytes left]\n", tag->size - tag->data_offset));

		frame = id3v2_frame_alloc (tag, frame_id);
		set_frame_flags (frame, buf[8], buf[9]);

		META_TAG_TRACE (("###: %s LEN: %u [%lu]\n", frame_id, data_len,
		                 ftell (TAG_FILE(tag)->f)));

		META_TAG_TRACE (("%s: group: %i compress: %i encrypt: %i "
		                 "unsync: %i data_len: %i\n", frame->id,
		                 frame->grouped, frame->compressed, frame->encrypted,
		                 frame->unsynched, frame->has_data_len));

		handler = find_handler (frame_handlers, frame_id);

		read_frame (tag, frame, handler, data_len);
		id3v2_frame_free (frame);
	}
}

/*****************************************************************************/
/* Frame parsing */

static uint8_t id3v2_frame_get_uint8 (struct id3v2_frame *frame)
{
	if (frame->offset >= frame->size)
		return 0;

	return frame->contents[frame->offset++];
}

static char *id3v2_frame_get_str (struct id3v2_frame *frame)
{
	char *p;

	if (frame->offset >= frame->size)
		return NULL;

	p = &frame->contents[frame->offset];

	/* the frame gets null-terminated when read, so this should be safe */
	frame->offset += STRLEN_0 (p);

	return p;
}

static char *id3v2_frame_get_str_with_encoding (struct id3v2_frame *frame)
{
	uint8_t encoding = id3v2_frame_get_uint8 (frame);

	/* only support ISO-8859-1 encoding (marked with zero byte)
	 * NOTE: this assumes that is the default encoding being used */
	if (encoding != 0)
		return NULL;

	return id3v2_frame_get_str (frame);
}

static char *id3v2_frame_get_lang (struct id3v2_frame *frame)
{
	static char lang[4];
	int         i;
	size_t      end;

	end = MIN (frame->offset + 3, frame->size);

	/* check overflow */
	if (frame->offset + 3 < frame->offset)
		end = frame->size;

	for (i = 0; i < 3; i++)
	{
		if (frame->offset >= frame->size)
			return NULL;

		if (!(lang[i] = id3v2_frame_get_uint8 (frame)))
		    break;

		if (!isalnum (lang[i]))
			break;
	}

	lang[i] = 0;

	/* skip the three language characters */
	frame->offset = end;

	return lang;
}

/*****************************************************************************/
/* Individual ID3v2 frame handling */

static void id3v2_parse_TLEN (struct id3v2 *tag, struct id3v2_frame *frame)
{
	int    millis;
	int    n;
	char  *p;
	char   buf[128];

	if (!(p = id3v2_frame_get_str_with_encoding (frame)))
		return;

	META_TAG_TRACE (("TLEN: contents=%s [len=%d]\n", p, frame->size));

	if ((n = sscanf (p, "%d", &millis)) != 1)
		return;

	snprintf (buf, sizeof (buf) - 1, "%u", millis / 1000);
	set_meta (TAG_FILE(tag), frame->id, "duration", buf);
}

static void id3v2_parse_COMM (struct id3v2 *tag, struct id3v2_frame *frame)
{
	uint8_t encoding = id3v2_frame_get_uint8 (frame);
	char   *lang     = id3v2_frame_get_lang  (frame);
	char   *descr    = id3v2_frame_get_str   (frame);

	/* ISO-8859-1 == 0 */
	if (encoding != 0)
		return;

	META_TAG_TRACE (("COMM: lang=%s description=%s\n", lang, descr));
	lang = descr; /* prevent 'unused variable' warnings */

	set_meta (TAG_FILE(tag), frame->id, "comment",
	          id3v2_frame_get_str (frame));
}

static void id3v2_parse_TIT2 (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (TAG_FILE(tag), frame->id, "title",
	          id3v2_frame_get_str_with_encoding (frame));
}

static void id3v2_parse_TALB (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (TAG_FILE(tag), frame->id, "album",
	          id3v2_frame_get_str_with_encoding (frame));
}

static void id3v2_parse_TRCK (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (TAG_FILE(tag), frame->id, "track",
	          id3v2_frame_get_str_with_encoding (frame));
}

static void id3v2_parse_TPE1 (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (TAG_FILE(tag), frame->id, "artist",
	          id3v2_frame_get_str_with_encoding (frame));
}

static void id3v2_parse_TYER (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (TAG_FILE(tag), frame->id, "year",
	          id3v2_frame_get_str_with_encoding (frame));
}

/*****************************************************************************/

static void tag_file_close (struct tag_file *tagfile)
{
	if (!tagfile)
		return;

	free (tagfile->name);

	if (tagfile->f)
	{
		fclose (tagfile->f);
		tagfile->f = NULL;
	}

	id3v2_free (tagfile->id3v2);
	id3v1_free (tagfile->id3v1);

	dataset_clear (tagfile->keys);
	tagfile->keys = NULL;

	free (tagfile);
}

static struct tag_file *tag_file_open (Share *share, const char *path,
                                       size_t max_len)
{
	struct tag_file *file;
	struct stat      st;

	if (stat (path, &st) != 0)
		return NULL;

	if (!(file = MALLOC (sizeof (struct tag_file))))
		return NULL;

	file->id3v2 = id3v2_alloc (file, max_len);
	file->id3v1 = id3v1_alloc (file);

	if (!file->id3v1 || !file->id3v2)
	{
		tag_file_close (file);
		return NULL;
	}

	if (!(file->f = fopen (path, "rb")))
	{
		tag_file_close (file);
		return NULL;
	}

	file->name  = STRDUP (file_basename ((char *)path));
	file->size  = st.st_size;
	file->share = share;

	return file;
}

static void check_id3v1 (struct id3v1 *tag, off_t pos)
{
	if (fseek (TAG_FILE(tag)->f, pos, SEEK_SET) != 0)
		return;

	read_v1 (tag);
}

static void check_id3v2 (struct id3v2 *tag, char *signature, off_t pos)
{
	if (fseek (TAG_FILE(tag)->f, pos, SEEK_SET) != 0)
		return;

	read_v2 (tag, signature);
}

static void tag_file_read (struct tag_file *file)
{
	off_t start;
	off_t pos;

	start = file->size - 4096;

	if (file->size < 4096)
		start = 0;

	/* scan id3v1{,.1} tag first */
	pos = file->size - 128;
	check_id3v1 (file->id3v1, pos);

	/* TODO */
#if 0
	/* check musicmatch tags */
	pos = start;
	while ((pos = scan_signature (file, MUSIC_MATCH_MAGIC)) >= 0)
	{
		check_musicmatch (file->musicmatch, pos);
		pos += strlen (MUSIC_MATCH_MAGIC);
	}

	/* check lyrics3 tags */
	pos = start;
	if (!dataset_lookup (
	while ((pos = scan_signature (file, "LYRICSBEGIN")) >= 0)
	{
		check_lyrics3 (file->lyrics3, pos);
		pos += strlen ("LYRICSBEGIN");
	}
#endif

	/* read id3v2 tag at the start of the file */
	check_id3v2 (file->id3v2, "ID3", 0);

	/* read id3v2 tags at the end, looking for footer signature "3DI" */
	pos = file->size - ID3V2_HDR_LEN;

	/* footer of id3v2 tag should be just before id3v1 tag */
	if (file->id3v1->start > 0)
		pos -= file->id3v1->start;

	check_id3v2 (file->id3v2, "3DI", pos);
}

/*****************************************************************************/

/* set a meta key read from the file */
static void set_meta (struct tag_file *file, char *id, char *key, char *val)
{
	char   *old;
	char   *max_str;
	size_t  max_len;
	size_t  len;

	if (!key || !val)
		return;

	/* only handle certain keys */
	if (!(max_str = dataset_lookupstr (file->keys, key)))
		return;

	max_len = ATOUL (max_str);

	/* trim the data */
	string_trim (val);

	old = share_get_meta (file->share, key);
	len = strlen (val);

	/* if the old id was larger, take that one instead */
	if (old && strlen (old) > len)
		return;

	/* don't set empty meta-information */
	if (string_isempty (val))
		return;

	/* cap the data at 128 chars */
	if (len > max_len)
		val[max_len] = 0;

	/* if id was supplied this is id3v2 metainfo */
	if (id)
		META_TAG_TRACE (("=== %s (%s): %s\n", id, key, val));
	else
		META_TAG_TRACE (("=== [v1] (%s): %s\n", key, val));

	share_set_meta (file->share, key, val);
}

static void add_key (struct tag_file *file, char *key, size_t max_len)
{
	char *str;

	if (!(str = stringf_dup ("%lu", (unsigned long)max_len)))
		return;

	dataset_insertstr (&file->keys, key, str);
	free (str);
}

void meta_tag_run (Share *share, const char *path)
{
	struct tag_file *file;

	if (!(file = tag_file_open (share, path, DEFAULT_MAX_FRAME_LEN)))
		return;

	/* insert the keys we are interested in, as well as the max length
	 * for each key */
	add_key (file, "duration", 128);
	add_key (file, "comment",  128);
	add_key (file, "title",    128);
	add_key (file, "album",    128);
	add_key (file, "track",    10);
	add_key (file, "artist",   128);
	add_key (file, "year",     6);

	tag_file_read (file);
	tag_file_close (file);
}

/*****************************************************************************/

#if 0
int main (int argc, char **argv)
{
	struct tag_file *tags;
	Share            share;
	int              i;

	for (i = 1; i < argc; i++)
	{
		printf ("checking %s\n", argv[i]);
		share_init (&share);

		if (!(tags = tag_file_open (&share, argv[i], 0)))
			continue;

		tag_file_read (tags);
		tag_file_close (tags);

		share_finish (&share);
	}

	return 0;
}
#endif
