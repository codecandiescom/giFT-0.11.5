/*
 * $Id: conf.c,v 1.32 2003/10/31 13:14:22 jasta Exp $
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

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <ctype.h>

#include "libgift.h"

#include "conf.h"
#include "file.h"
#include "parse.h"

/*****************************************************************************/

static void config_headers_read (Config *conf);

/*****************************************************************************/
/* helpers */

static char *config_parse_keypath (char *keypath, char **header_out,
                                   char **key_out)
{
	char *header;
	char *key;
	char *value;
	char *defvalue;

	if (!keypath)
		return NULL;

	if ((defvalue = strchr (keypath, '=')))
		defvalue++;

	header = strdup (keypath);
	if (!(key = strchr (header, '/')))
	{
		free (header);
		return NULL;
	}

	*key++ = 0;

	if ((value = strchr (key, '=')))
		*value++ = 0;

	if (header_out)
		*header_out = header;

	if (key_out)
		*key_out = key;

	return defvalue;
}

/*****************************************************************************/

static ConfigHeader *config_header_new (char *line)
{
	ConfigHeader *confhdr;

	confhdr = malloc (sizeof (ConfigHeader));
	confhdr->name = strdup (line);
	confhdr->keys = NULL;

	return confhdr;
}

/*****************************************************************************/

/* if the file on disk changes, reload it */
static void config_update (Config *conf)
{
	struct stat st;

	if (!conf)
		return;

	if (!file_stat (conf->path, &st))
	{
		GIFT_ERROR (("unable to stat %s: %s", conf->path, GIFT_STRERROR()));
		return;
	}

	if (conf->mtime != st.st_mtime)
	{
		GIFT_TRACE (("resynching %s", conf->path));

		conf->mtime = st.st_mtime;

		/* sync with disk */
		config_headers_read (conf);
	}
}

/*****************************************************************************/

static int cmp_config_header (ConfigHeader *a, char *b)
{
	if (!a || !a->name || !b)
		return -1;

	return strcmp (a->name, b);
}

static char *config_lookup (Config *conf, ConfigHeader **confhdr_out,
                            char *header, char *key)
{
	ConfigHeader *confhdr;
	List         *lheader;

	if (!conf)
		return NULL;

	config_update (conf);

	if (confhdr_out)
		*confhdr_out = NULL;

	lheader = list_find_custom (conf->headers, header,
	                            (CompareFunc) cmp_config_header);

	if (!lheader)
		return NULL;

	confhdr = lheader->data;

	if (confhdr_out)
		*confhdr_out = confhdr;

	return dataset_lookup (confhdr->keys, key, STRLEN_0 (key));
}

/*****************************************************************************/
/* data insertion methods */

void config_set_str (Config *conf, char *keypath, char *value)
{
	char *header, *key, *defvalue;
	ConfigHeader *confhdr;

	if (!conf || !keypath)
		return;

	defvalue = config_parse_keypath (keypath, &header, &key);

	/* find the header */
	config_lookup (conf, &confhdr, header, key);

	if (!confhdr)
	{
		conf->confhdr = config_header_new (header);
		conf->headers = list_append (conf->headers, conf->confhdr);

		confhdr = conf->confhdr;
	}

	dataset_insertstr (&confhdr->keys, key, value);

	free (header);
}

void config_set_int (Config *conf, char *keypath, int value)
{
	char str[20];

	if (!conf)
		return;

	sprintf (str, "%i", value);

	config_set_str (conf, keypath, str);
}

/*****************************************************************************/
/* data retrieval methods */

char *config_get_str (Config *conf, char *keypath)
{
	char *header, *key, *defvalue;
	char *value;

	defvalue = config_parse_keypath (keypath, &header, &key);

	/* call the private locater function */
	value = config_lookup (conf, NULL, header, key);

	if (!value && defvalue)
	{
		config_set_str (conf, keypath, defvalue);
		value = config_lookup (conf, NULL, header, key);
	}

	free (header);

	return value;
}

int config_get_int (Config *conf, char *keypath)
{
	char *value;

	value = config_get_str (conf, keypath);

	if (!value)
		return 0;

	return atoi (value);
}

/*****************************************************************************/

static void config_headers_clear (Config *conf)
{
	List *next;

	if (!conf)
		return;

	while (conf->headers)
	{
		ConfigHeader *confhdr = conf->headers->data;

		next = conf->headers->next;

		dataset_clear (confhdr->keys);
		free (confhdr->name);
		free (confhdr);

		/* free the actual list element */
		free (conf->headers);

		conf->headers = next;
	}
}

/*****************************************************************************/

/* determines whether or not this line is a comment.  blank whitelines are
 * also considered comments. */
static int line_comment (Config *conf, char *line)
{
	char *ptr;

	if (!line)
		return TRUE;

	if (conf->comments)
	{
		/* hack comments off */
		if ((ptr = strchr (line, '#')))
			*ptr = 0;
	}

	/* remove all trailing/leading whitespace */
	string_trim (line);

	/* check if it's empty...after all this both a comment and a blank line
	 * will be considered exactly the same */
	return string_isempty (line) ? TRUE : FALSE;
}

/*****************************************************************************/

/* synchs the configuration structure with the keys on disk.  This function
 * preserves all original formatting. */
void config_write (Config *conf)
{
	char *line = NULL;
	char *out_line, *out_line0;
	char *curr_hdr = NULL;
	char  tmp_file[PATH_MAX];
	FILE *tmp_f;

	if (!conf)
		return;

	/* make sure we have the conf file open */
	if (!conf->file)
	{
		if (!(conf->file = fopen (conf->path, "r")))
		{
			GIFT_ERROR (("Can't open %s: %s", conf->path, GIFT_STRERROR ()));
			return;
		}
	}

	/* create a temporary file for writing out the new conf file */
	snprintf (tmp_file, sizeof (tmp_file) - 1, "%s.tmp", conf->path);

	if (!(tmp_f = fopen (tmp_file, "w")))
	{
		GIFT_ERROR (("Can't open %s: %s", tmp_file, GIFT_STRERROR ()));
		fclose (conf->file);
		conf->file = NULL;
		return;
	}

	while (file_read_line (conf->file, &line))
	{
		/* duplicate this current line so that we may mangle it and still
		 * preserve the original for writing */
		out_line0 = out_line = STRDUP (line);

		/* attempt to identify one of three types that this line could be and
		 * parse it
		 * NOTE: the condition below is funky.  If there is an equal operator
		 * inside the comment it will move to evaluation of the key lookup
		 * code, which can check for "#key = ..." and uncomment it.  If
		 * that exact condition is not found, it will simply write the
		 * comment as normal */
		if (!strchr (out_line, '=') && line_comment (conf, out_line))
		{
			out_line = NULL;
		}
		else if (*out_line == '[')
		{
			char *ptr;

			/* track which current header we are one */
			if ((ptr = strchr (out_line + 1, ']')))
			{
				*ptr = 0;

				free (curr_hdr);
				curr_hdr = STRDUP (out_line + 1);
			}

			out_line = NULL;
		}
		else
		{
			char *key_lookup;
			char *key;
			char *value;


			key = string_sep (&out_line, "=");

			string_trim (key);

			/* check for the special condition "#key = ..." */
			if (*key == '#')
			{
				/* check if the key is sane.  if not, there's pretty much
				 * no way in hell this is going to succeed a key lookup, and
				 * will this be written as a comment */
				if (!strchr (key, ' '))
					key++;
			}

			if (!curr_hdr || !key || !(*key))
			{
				GIFT_TRACE (("removing garbage"));
				free (out_line0);
				continue;
			}

			/* it must be a key, look for it */
			key_lookup = malloc (strlen (curr_hdr) + strlen (key) + 2);
			sprintf (key_lookup, "%s/%s", curr_hdr, key);

			if (!(value = config_get_str (conf, key_lookup)))
				out_line = NULL;
			else
			{
				/* write the new output */
				out_line =
					malloc (strlen (key) + strlen (value) + 12);
				sprintf (out_line, "%s = %s\n", key, value);

				/* destroy the old duplication and reset it to this one */
				free (out_line0);
				out_line0 = out_line;
			}

			free (key_lookup);
		}

		/* if out line is provided, we wanted to change something...otherwise
		 * we wanna use the original */
		fprintf (tmp_f, "%s", (out_line ? out_line : line));
		free (out_line0);
	}

	fclose (tmp_f);
	fclose (conf->file);
	conf->file = NULL;

	/* move the new conf file to the updated */
	file_mv (tmp_file, conf->path);
}

/*****************************************************************************/

static void config_keys_read (Config *conf)
{
	char  *line = NULL;
	size_t line_len = 0;
	char  *key;
	char  *value;

	if (!conf)
		return;

	while (file_read_line (conf->file, &line))
	{
		/* figure out how much we read so that it may be rewinded if
		 * needed */
		line_len = strlen (line);

		if (line_comment (conf, line))
			continue;

		if (*line == '[')
		{
			free (line);

			/* we just started reading the next header, rewind the file and
			 * return to the header reading function
			 * TODO -- this is quite possibly a bad idea */
			if (fseek (conf->file, -((off_t) line_len + 1), SEEK_CUR) == -1)
			{
				GIFT_ERROR (("fseek: %s", GIFT_STRERROR ()));
				return;
			}

			return;
		}

		key = line;

		if (!(value = strchr (key, '=')))
			continue;

		*value++ = 0;

		string_trim (key);
		string_trim (value);

		dataset_insertstr (&conf->confhdr->keys, key, value);
	}
}

static void config_headers_read (Config *conf)
{
	char *line = NULL;

	if (!conf)
		return;

	config_headers_clear (conf);

	if (!(conf->file = fopen (conf->path, "r")))
		return;

	while (file_read_line (conf->file, &line))
	{
		char *ptr;

		/* ignore blank lines */
		if (line_comment (conf, line))
			continue;

		if (*line != '[')
			continue;

		ptr = strchr (line, ']');

		if (!ptr)
			continue;

		*ptr = 0;

		conf->confhdr = config_header_new (line + 1);
		conf->headers = list_append (conf->headers, conf->confhdr);

		config_keys_read (conf);
	}

	fclose (conf->file);
	conf->file = NULL;
}

/*****************************************************************************/

Config *config_new (char *file)
{
	return config_new_ex (file, TRUE);
}

Config *config_new_ex (char *file, int comments_allowed)
{
	Config     *conf;
	struct stat st;

	if (!file_stat (file, &st))
		return NULL;

	if (!(conf = MALLOC (sizeof (Config))))
		return NULL;

	conf->path     = STRDUP (file);
	conf->mtime    = st.st_mtime;
	conf->comments = comments_allowed;

	config_headers_read (conf);

	return conf;
}

void config_free (Config *conf)
{
	if (!conf)
		return;

	free (conf->path);

	if (conf->file)
		fclose (conf->file);

	config_headers_clear (conf);

	free (conf);
}

/*****************************************************************************/

Config *gift_config_new (char *module)
{
	char *file;

	if (!module)
		return NULL;

	if (strcmp (module, "giFT"))
		file = gift_conf_path ("%s/%s.conf", module, module);
	else
	{
		/* check if the old deprecated gift.conf exists... */
		if ((file_exists ((file = gift_conf_path ("gift.conf")))))
		{
			GIFT_WARN (("deprecated configuration found at %s: looking for "
			            "giftd.conf...", file));
		}

		file = gift_conf_path ("giftd.conf");
	}

	return config_new (file);
}

/*****************************************************************************/

static char *make_host_path (char *buf, size_t buf_size)
{
	char *host_copy;
	char *ret;

	/* ugh, we have to make a copy of the buffer, then copy it right
	 * back in */
	if (!(host_copy = file_host_path (buf)))
		return NULL;

	/* ugh, and a strlen, sigh */
	if (buf_size > strlen (host_copy))
		ret = strcpy (buf, host_copy);
	else
		ret = NULL;

	free (host_copy);

	return ret;
}

static char *conf_path_r (char *buf, size_t buf_size, const char *fmt,
                          va_list args)
{
	String sobj;
	char  *ldir;
	BOOL   failure;

	ldir = platform_local_dir ();
	assert (ldir != NULL);

	string_init (&sobj);
	string_set_buf (&sobj, buf, buf_size, 0, FALSE);

	failure  = ! BOOL_EXPR (string_append (&sobj, ldir));
	failure |= ! BOOL_EXPR (string_appendc (&sobj, '/'));
	failure |= ! BOOL_EXPR (string_appendvf (&sobj, fmt, args));

	string_finish (&sobj);

	/* not enough space provided, fail completely */
	if (failure)
		return NULL;

	/* convert the buffer into a host path, that is something that the
	 * local machine can open */
	return make_host_path (buf, buf_size);
}

char *gift_conf_path_r (char *buf, size_t buf_size, const char *fmt, ...)
{
	va_list args;
	char *ret;

	va_start (args, fmt);
	ret = conf_path_r (buf, buf_size, fmt, args);
	va_end (args);

	return ret;
}

char *gift_conf_path (const char *fmt, ...)
{
	va_list args;
	static char pathbuf[PATH_MAX];
	char *ret;

	va_start (args, fmt);
	ret = conf_path_r (pathbuf, sizeof (pathbuf), fmt, args);
	va_end (args);

	return ret;
}

char *gift_conf_pathkey (Config *config, char *key, char *def, char *file)
{
	static char retval[PATH_MAX];
	char *keypath;
	char *path;
	char *hpath;

	if (!(keypath = stringf_dup ("%s=%s", key, def)))
		return NULL;

	path = file_expand_path (config_get_str (config, keypath));
	free (keypath);

	if (!path)
		return NULL;

	/* convert to host-style paths, so C:\Foo\Bar for Windows, whereas the
	 * configuration key would have been in UNIX-style. */
	if (!(hpath = file_host_path (path)))
		return NULL;

	/*
	 * Make a local copy of the string so that we don't need to tramble on
	 * stringf's buffer.
	 *
	 * NOTE:
	 * We are hardcoding "/" here and NOT converting to host order again.
	 * This is very bad and needs to be fixed.
	 */
	snprintf (retval, sizeof (retval) - 1, "%s/%s", hpath,
	          STRING_NOTNULL(file));
	free (hpath);

	/* make sure elements exist */
	file_create_path (retval, 0755);

	return retval;
}

/*****************************************************************************/

#if 0
int main ()
{
	Config *conf;

	platform_init ();
	printf ("conf_path: %s\n", gift_conf_path ("gift.conf"));

	conf = config_new ("/home/jasta/.giFT/gift.conf");

	printf ("set root...\n");
	config_set_str (conf, "sharing/root", "/data/temp_mp3s:/data/.porn");

	printf ("get root...%s\n",
			config_get_str (conf, "sharing/root"));

	printf ("write...\n");
	config_write (conf);

	config_free (conf);

	return 0;
}
#endif

#if 0
int main ()
{
	Config *conf;

	if (!platform_init ())
	{
		printf ("bastard\n");
		return 0;
	}

	conf = config_new (gift_conf_path ("gift.conf"));
	assert (conf != NULL);

	printf ("pathkey: %s\n",
	        gift_conf_pathkey (conf, "download/incoming",
	                           gift_conf_path ("incoming"),
	                           stringf ("%s.%s", "file", "ext")));

	config_free (conf);

	return 0;
}
#endif
