/*
 * $Id: file.c,v 1.43 2003/12/23 03:44:39 jasta Exp $
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

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libgift.h"

#include "file.h"
#include "conf.h"

#ifdef WIN32
# include <direct.h> /* mkdir () */
# include <io.h>     /* mktemp() */
#endif /* WIN32 */

/*****************************************************************************/

BOOL file_stat (const char *file, struct stat *stbuf)
{
	if (!file || !stbuf)
		return FALSE;

	if (stat (file, stbuf))
		return FALSE;

	return TRUE;
}

BOOL file_exists (const char *file)
{
	struct stat st;

	if (!file_stat (file, &st))
		return FALSE;

	return (S_ISREG(st.st_mode));
}

BOOL file_direxists (const char *path)
{
	struct stat st;

	if (!file_stat (path, &st))
		return FALSE;

	return (S_ISDIR(st.st_mode));
}

/*****************************************************************************/

char *file_dirname (const char *file)
{
	char *dir;
	char *ptr;

	if (!(dir = STRDUP (file)))
		return NULL;

	if ((ptr = strrchr (dir, '/')))
		*ptr = 0;
	else
		*dir = 0;

	return dir;
}

char *file_basename (const char *file)
{
	char *ptr;

	if (!file)
		return NULL;

	if (!(ptr = strrchr (file, '/')))
		return ((char *)file);

	ptr++;

	/* TODO, make sure its not /full/path/ */

	return ptr;
}

/*****************************************************************************/

/* "secure" a path by requiring it to be absolute and contain no directory
 * entries matching '.' or '..' */
char *file_secure_path (const char *patharg)
{
	char *path;                        /* strdup copy of patharg */
	char *s_path;
	char *s_path0;

	if (!patharg)
		return NULL;

	if (*patharg != '/')
		return NULL;

	/* get our own memory here ;) */
	s_path0 = s_path = path = STRDUP (patharg);

	while ((s_path = string_sep_set (&path, "\\/")))
	{
#ifndef WIN32
		/* jasta: why would a '.' be a problem?
		 * rossta: its not, im just being overly cautious...ever had
		 * your name posted on bugtraq?
		 * jasta: sigh, yes :) */
		if (!strcmp (s_path, ".") || !strcmp (s_path, ".."))
#else /* WIN32 */
		/* skip paths that contain ., .., ..., etc.
		 * according to pretender:
		 * on win98 and winME '.../' == '../../' and '..../' == '../../../'
		 * using fopen(path,"r")
		 * win2k (and presumably NT & XP) do not exhibit this problem.
		 */
		if (s_path[0] == '.')
#endif /* !WIN32 */
		{
			/* TODO -- strmove should do this, damnit */
			if (path)
				string_move (s_path, path);
			else
				*s_path = 0;

			/* rewind so that we read the "next" path, which just so happens
			 * to be s_path right now :) */
			path = s_path;

			continue;
		}

		/* unset the \0
		 * NOTE: this is safe because path will be incremented by at least
		 * one each successful iteration */
		if (path)
			*(path - 1) = '/';
	}

	return s_path0;
}

/*****************************************************************************/

/* wrap fgets() to allow for next_arg to manipulate the string */
char *file_read_line (FILE *f, char **outbuf)
{
	char buf[1024];

	if (!f || !outbuf)
		return NULL;

	/* free each iteration...gaurenteed to cleanup after ourselves */
	if (*outbuf)
		free (*outbuf);

	if (fgets (buf, sizeof(buf) - 1, f))
	{
		*outbuf = strdup (buf);
		return *outbuf;
	}

	/* EOF || error */
	return NULL;
}

/*****************************************************************************/

char *file_expand_path (const char *patharg)
{
	char *path;                        /* strdup copy of patharg */
	static char newp[PATH_MAX];
	char *p1;

	if (!patharg)
		return NULL;

	path = strdup (patharg);

	/* hack the ~ out */
	if ((p1 = strchr (path, '~')) != NULL)
	{
		char *p0 = path;

		*p1++ = 0;

		if (*p0)
			snprintf (newp, sizeof (newp) - 1, "%s%s%s", p0,
			          platform_home_dir (), p1);
		else
			snprintf (newp, sizeof (newp) - 1, "%s%s", platform_home_dir (), p1);
	}
	else
		snprintf (newp, sizeof (newp) - 1, "%s", path);

	free (path);

	return newp;
}

int file_create_path (const char *patharg, int mode)
{
	char *path;                        /* strdup copy of patharg */
	char *p1;
#ifdef WIN32
	char *p2;
#endif
	int   ret;

	if (!patharg)
		return FALSE;

	/* must make sure we're working w/ our own memory here */
	p1 = path = strdup (patharg);

	/* chop off file name */
	p1 = strrchr (path, '/');
#ifdef WIN32
	p2 = strrchr (path, '\\');
#endif

#ifdef WIN32
	/* path may contain mixed '/' and '\\', need to find the last of either */
	p1 = (p1 > p2) ? p1 : p2;
#endif

	if (!p1)
	{
		/*
		 * patharg is just a filename so no directory creation is required
		 * -> we behave as if the directory exists
		 */
		free (path);
		return TRUE;
	}

	*p1 = 0;

	/* create path */
	ret = file_mkdir (path, mode);

	free (path);

	return ret;
}

static int portable_mkdir (const char *pathname, int mode)
{
	int ret;

#ifdef WIN32
	ret = mkdir (pathname);
#else /* !WIN32 */
	ret = mkdir (pathname, mode);
#endif /* WIN32 */

	return ret;
}

static BOOL is_path_delim (int c)
{
	if (c == '/')
		return TRUE;

#ifdef WIN32
	if (c == '\\')
		return TRUE;
#endif /* WIN32 */

	return FALSE;
}

/* this approach is quite terrible, rethink later */
BOOL file_mkdir (const char *patharg, int mode)
{
	size_t patharg_len;
	char  *path;                        /* strdup copy of patharg */
	char  *p1;
	int    ret;

	/* check for patharg=NULL and patharg="" */
	if ((patharg_len = gift_strlen (patharg)) == 0)
		return FALSE;

	/*
	 * Note: We are dealing with host paths here which makes things very
	 * messy. What we should do is make all file_* functions deal with unix
	 * paths and have giFT never see host paths.  Unfortunately this requires
	 * non trivial fixes all over the place.
	 */

	/* create copy with guaranteed trailing slash to simplify below loop */
	p1 = path = malloc (patharg_len + 2);
	sprintf (path, "%s/", patharg);

	/* skip leading slashes, does no harm on windows */
	while (is_path_delim (*p1))
		p1++;

	/* call mkdir for every path segment */
	while (*p1)
	{
		if (is_path_delim (*p1) == FALSE)
			p1++;
		else
		{
			char tmp;

			tmp = *p1;
			*p1 = '\0';
			portable_mkdir (path, mode);
			*p1 = tmp;

			/* ignore stupid shit like /path////////dir */
			while (is_path_delim (*p1))
				p1++;
		}
	}

	/* create final path element */
	portable_mkdir (path, mode);

	/* file_direxists() doesn't work properly if path contains trailing
	 * slashes, should this go into file_stat() where the problem actually
	 * is? */
	while (p1 > path && (is_path_delim (p1[-1])))
		p1--;

	*p1 = 0;

	/* the final result is whether or not the directory exists, not whether
	 * we were actually able to create it with this call */
	ret = file_direxists (path);

	/* free our copy */
	free (path);

	return ret;
}

/*****************************************************************************/

/* open a unique temporary file
 * NOTE: this is unused and needs completion */
FILE *file_temp (char **out, char *module)
{
	char  buf[PATH_MAX];
	char *path;
	FILE *ret;
#ifndef WIN32
	int fd;
#endif

	snprintf (buf, sizeof(buf) - 1, "%s",
	          gift_conf_path ("%s/%s.XXXX", module, module));

#ifndef WIN32
	if ((fd = mkstemp (buf)) < 0)
		return NULL;

	path = buf;

	ret = fdopen (fd, "w");
#else /* WIN32 */
	if (!(path = mktemp (buf)))
		return NULL;

	ret = fopen (path, "wb");
#endif /* !WIN32 */

	if (out)
		*out = STRDUP (path);

	return ret;
}

/*****************************************************************************/

BOOL file_rmdir (const char *path)
{
	DIR           *dir;
	struct dirent *d;
	struct stat    st;
	int            ret = TRUE;

	if (!path || !*path)
		return FALSE;

	if (!(dir = file_opendir (path)))
		return FALSE;

	while ((d = file_readdir (dir)))
	{
		char fullpath[PATH_MAX];

		/* skip '.' and '..' */
		if (!strcmp (d->d_name, ".") || !strcmp (d->d_name, ".."))
			continue;

		snprintf (fullpath, sizeof (fullpath) - 1, "%s/%s", path, d->d_name);

		if ((stat (fullpath, &st)) == -1)
		{
			GIFT_ERROR (("stat failed on %s: %s", fullpath, GIFT_STRERROR ()));
			ret = FALSE;
			continue;
		}

		if (S_ISDIR (st.st_mode))
		{
			file_rmdir (fullpath);
		}
		else
		{
			if (unlink (fullpath) == -1)
			{
				GIFT_ERROR (("unlink failed on %s: %s", fullpath,
				             GIFT_STRERROR ()));
			}

			ret = FALSE;
		}
	}

	file_closedir (dir);

	/* delete thyself */
	if ((rmdir (path)))
	{
		GIFT_ERROR (("rmdir failed on %s: %s", path, GIFT_STRERROR ()));
		ret = FALSE;
	}

	return ret;
}

/*****************************************************************************/

BOOL file_cp (const char *src, const char *dst)
{
	FILE  *src_f;
	FILE  *dst_f;
	char   buf[2048];
	size_t n;

	if (!(src_f = fopen (src, "rb")))
	{
		GIFT_ERROR (("unable to open %s (read): %s", src, GIFT_STRERROR ()));
		return FALSE;
	}

	if (!(dst_f = fopen (dst, "wb")))
	{
		GIFT_ERROR (("unable to open %s (write): %s", dst, GIFT_STRERROR ()));

		/* leaking fd's is bad */
		fclose (src_f);
		return FALSE;
	}

	while ((n = fread (buf, sizeof (char), sizeof (buf), src_f)) > 0)
	{
		if (fwrite (buf, sizeof (char), n, dst_f) < n)
		{
			GIFT_ERROR (("unable to write to %s: %s", dst, GIFT_STRERROR ()));

			/* we need to cleanup and get out */
			fclose (src_f);
			fclose (dst_f);

			/* remove the partially copied file */
			unlink (dst);
			return FALSE;
		}
	}

	/* cleanup */
	fclose (src_f);

	/* check to make sure we can flush */
	if (fclose (dst_f))
	{
		unlink (dst);
		return FALSE;
	}

	return TRUE;
}

BOOL file_mv (const char *src, const char *dst)
{
	/* try to optimize w/ single dev links first */
	if (rename (src, dst) == 0)
		return TRUE;

	if (!file_cp (src, dst))
		return FALSE;

	/* unlink the source */
	if (unlink (src) != 0)
	{
		GIFT_ERROR (("unable to unlink %s: %s", src,
		             GIFT_STRERROR ()));
	}

	return TRUE;
}

/*****************************************************************************/

/* slurp contents of a file (data will be allocated for you) */
BOOL file_slurp (const char *file, char **data, unsigned long *len)
{
	FILE          *f;
	char          *f_data;
	off_t          f_len;
	struct stat    st;

	if (!file || !data)
		return FALSE;

	*data = NULL;

	if (len)
		*len = 0;

	if (!(f = fopen (file, "rb")))
	{
		GIFT_ERROR (("Can't open %s: %s", file, GIFT_STRERROR ()));
		return FALSE;
	}

	if (fstat (fileno (f), &st) < 0)
	{
		GIFT_ERROR (("Can't stat %s: %s", file, GIFT_STRERROR ()));
		fclose (f);
		return FALSE;
	}

	f_len = st.st_size;

	f_data = malloc (f_len);

	if (fread (f_data, sizeof (char), (size_t) f_len, f) != (size_t) f_len)
	{
		GIFT_ERROR (("failed to read %s: %s", file, GIFT_STRERROR ()));
		free (f_data);
		fclose (f);
		return FALSE;
	}

	*data = f_data;

	if (len)
		*len = f_len;

	fclose (f);

	return TRUE;
}

/* write the contents of the data buffer to file */
BOOL file_dump (const char *file, const char *data, unsigned long len)
{
	FILE *f;

	if (!file || !data)
		return FALSE;

	if (!(f = fopen (file, "wb")))
	{
		GIFT_ERROR (("Can't create %s: %s", file, GIFT_STRERROR ()));
		return FALSE;
	}

	if (fwrite (data, sizeof (char), len, f) < len)
	{
		GIFT_ERROR (("Can't write to %s: %s", file, GIFT_STRERROR ()));
		fclose (f);
		unlink (file);
		return FALSE;
	}

	fclose (f);

	return TRUE;
}

/*****************************************************************************/

FILE *file_open (const char *path, const char *mode)
{
	if (!path || !mode)
		return NULL;

	return fopen (path, mode);
}

int file_close (FILE *f)
{
	if (!f)
		return 0;

	return fclose (f);
}

int file_unlink (const char *path)
{
	if (!path)
		return 0;

	return unlink (path);
}

/*****************************************************************************/

#ifndef HAVE_DIRENT_H
static void dh_free (DIR *dh)
{
	free (dh->finddata);
	free (dh->direntry);
	free (dh);
}
#endif /* !HAVE_DIRENT_H */

/* opendir () */
DIR *file_opendir (const char *dir)
{
#ifdef HAVE_DIRENT_H
	return opendir (dir);
#else /* !HAVE_DIRENT_H */
	char path[_MAX_PATH];
	DIR *dh;

	if (!dir)
		return NULL;

	if (!(dh = malloc (sizeof (DIR))))
		return NULL;

	dh->finddata = malloc (sizeof (struct _finddata_t));
	dh->direntry = malloc (sizeof (struct dirent));

	if (!dh->finddata || !dh->direntry)
	{
		dh_free (dh);
		return NULL;
	}

	/* glob matching all files */
	_snprintf (path, _MAX_PATH, "%s/%s", dir, "*");

	if ((dh->findfirst_handle = _findfirst (path, dh->finddata)) == -1)
	{
		dh_free (dh);
		return NULL;
	}

	/* set the direntry with a "flag" so that we know readdir should return
	 * this on first call */
	dh->firstentry = dh->direntry;
	dh->direntry->d_name = dh->finddata->name;

	return dh;
#endif /* HAVE_DIRENT_H */
}

/* readdir () */
struct dirent *file_readdir (DIR *dh)
{
#ifdef HAVE_DIRENT_H
	return readdir (dh);
#else /* !HAVE_DIRENT_H */
	struct dirent *de;
	int rv;

	if (!dh)
		return NULL;

	/* _findfirst is apparently equiv to opendir and the first readdir in one
	 * call, so we must track the first read and return here */
	if (dh->firstentry)
	{
		de = dh->firstentry;
		dh->firstentry = NULL;

		return de;
	}

	if ((rv = _findnext (dh->findfirst_handle, dh->finddata)) == -1)
		return NULL;

	de = dh->direntry;
	de->d_name = dh->finddata->name;

	return de;
#endif /* HAVE_DIRENT_H */
}

/* closedir () */
int file_closedir (DIR *dh)
{
#ifndef HAVE_DIRENT_H
	int rv;
#endif /* !HAVE_DIRENT_H */

	if (!dh)
		return -1;

#ifdef HAVE_DIRENT_H
	return closedir (dh);
#else /* !HAVE_DIRENT_H */
	rv = _findclose (dh->findfirst_handle);
	dh_free (dh);

	return rv;
#endif /* HAVE_DIRENT_H */
}

/*****************************************************************************/
/* path conversion routines */

char *file_host_path (const char *unix_path)
{
	char *host_path;
#ifdef WIN32
	char *ptr;
#endif

	if (!(host_path = STRDUP (unix_path)))
		return NULL;

#ifdef WIN32
	/* check to see if this a regular /C/Share/Path (rather than an
	 * UNC: //share/path) */
	if (unix_path[0] == '/' && unix_path[1] != '/')
	{
		/* /C/dir/file -> C:/dir/file */
		host_path[0] = unix_path[1];
		host_path[1] = ':';
	}

	for (ptr = host_path; *ptr; ptr++)
	{
		if (*ptr == '/')
			*ptr = '\\';
	}
#endif /* WIN32 */

	return host_path;
}

char *file_unix_path (const char *host_path)
{
	char *unix_path;
#ifdef WIN32
	char *ptr;
#endif

	if (!(unix_path = STRDUP (host_path)))
		return NULL;

#ifdef WIN32
	/* see above ;) */
	if (host_path[1] == ':')
	{
		/* C:\dir\file -> /C\dir\file */
		unix_path[0] = '/';
		unix_path[1] = host_path[0];
	}

	for (ptr = unix_path; *ptr; ptr++)
	{
		if (*ptr == '\\')
			*ptr = '/';
	}
#endif /* WIN32 */

	return unix_path;
}
