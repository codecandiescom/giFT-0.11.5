/*
 * $Id: platform.c,v 1.76 2003/10/16 18:50:54 jasta Exp $
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

#include <errno.h>

#include "libgift.h"

/* only strictly required for UNIX, but we'll make Windows do it too */
#include "file.h"

#include "event.h"
#include "network.h"

#ifndef WIN32
# include <signal.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/utsname.h>
# include <sys/wait.h>
# ifndef AF_UNIX
#  define AF_UNIX AF_INET
# endif
#else /* WIN32 */
# include <process.h> /* _beginthreadex() */
# include <sys/timeb.h>
# define WS_MAJOR 2
# define WS_MINOR 0
# ifndef R_OK
#   define R_OK 04
# endif
# define GIFT_REG_KEY "Software\\giFT\\giFT"   /* NSIS defines this for us */
# define GIFT_REG_SUBKEY "instpath"            /* ... */
#endif /* !WIN32 */

/* BeOS doesn't define this, possibly others */
#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif

/**
 * If defined, do not use a separate process/thread for platform_child.
 * Instead, a blocking implementation will be used to simulate child
 * processes.
 *
 * You should probably leave this undefined unless you are compiling giFT
 * for an obscure system. If you are trying to debug a child process more
 * easily, the -x switch is more useful. This probably doesn't work when
 * using an OS that doesn't support select() on non-sockets such as win32.
 */
/* #define NO_CHILD */

/*****************************************************************************/

/**
 * Contains installed data, such as mime.types.
 *
 * UNIX:    this is defined by configure as DATA_DIR
 * Windows: The plugin_dir + /data
 */
static char *data_dir = NULL;

/**
 * Contains all dynamically loaded plugins that this giFT installation has
 * available.
 *
 * UNIX:    this is defined by configure as PLUGIN_DIR
 * Windows: Searches for the first valid directory in:
 *          1. HKEY_LOCAL_MACHINE\Software\giFT\giFT\instpath registry key
 *          2. Directory where giFT started from
 *          3. Current directory
 */
static char *plugin_dir = NULL;

/**
 * Contains the user's 'home' directory.
 *
 * UNIX:    assigned to $HOME or /home/$USER
 * Windows: Searches for the first valid directory in:
 *          1. HKEY_CURRENT_USER\Software\giFT\giFT\instpath registry key
 *          2. %HOME% directory, if %HOME% is defined in the environment.
 *          3. Directory where giFT started from.
 *          4. Current directory.
 */
static char *home_dir = NULL;

/**
 * Contains all user preferences (.conf files).
 *
 * UNIX:    assigned to $HOME/.giFT
 * Windows: Searches for the first valid directory in:
 *          1. home_dir
 *          2. home_dir + /.giFT
 */
static char *local_dir = NULL;

/*****************************************************************************/

/* uname string formatted like an HTTP User Agent
 * TODO: this shouldn't be preformatted like that */
static char *gift_version = NULL;

/* list of all active children keyed by either thread ID or pid (windows and
 * linux respectively) */
static Dataset *active_children = NULL;

#ifndef HAVE_SOCKETPAIR
int socketpair (int family, int type, int protocol, int sv[2]);
#endif /* !HAVE_SOCKETPAIR */

/*****************************************************************************/

static void child_exit (int signum);

/*****************************************************************************/
/* PLATFORM_INIT and PLATFORM_CLEANUP */

/* accessors */
char *platform_data_dir   (void) { return data_dir;   }
char *platform_plugin_dir (void) { return plugin_dir; }
char *platform_local_dir  (void) { return local_dir;  }
char *platform_home_dir   (void) { return home_dir;   }

#ifndef WIN32

static char *guess_home_dir (void)
{
	static char ph[PATH_MAX];
	char *home;

	/* determine the home directory */
	if (!(home = getenv ("HOME")))
	{
		char *user;

		/* minor complaint, fallback to $USER */
		GIFT_WARN (("$HOME not set, falling back to /home/$USER..."));

		if ((user = getenv ("USER")))
		{
	 		snprintf (ph, sizeof (ph) - 1, "/home/%s", user);

			if (access (ph, R_OK) == 0)
				home = ph;
		}
	}

	return home;
}

static BOOL unix_init (const char *home, const char *local,
                       const char *data, const char *plugin)
{
	/*
	 * Attach signal handlers.  At some point in the future it would be nice
	 * to check for the precense of signal and its quirks before using it...
	 */
	signal (SIGPIPE, SIG_IGN);
	signal (SIGCHLD, child_exit);

	/*
	 * Assign directory paths.  These may optionally be overriden by
	 * the parameters to platform_init.
	 */
	data_dir   = STRDUP ((data)   ? (data)   : (DATA_DIR));
	plugin_dir = STRDUP ((plugin) ? (plugin) : (PLUGIN_DIR));
	home_dir   = STRDUP ((home)   ? (home)   : (guess_home_dir()));

	/* this check should be generalized and applied to all of the
	 * directory paths... */
	if (!home_dir)
	{
		GIFT_FATAL (("unable to locate a sane home directory, consider "
		             "using --home-dir or passing the appropriate "
		             "parameters to libgift:platform_init"));
		return FALSE;
	}

	local_dir  = STRDUP ((local)  ? (local)  : (file_expand_path("~/.giFT")));

	return TRUE;
}

#else /* WIN32 */

/* read registry and returns a string if the key was found in the registry
   and the key is the name of a valid directory */
static char *read_reg_dir (HKEY key_h, LPCTSTR key, LPCTSTR subkey)
{
	static char dir[MAX_PATH];
	DWORD len = sizeof (dir) - 1;
	DWORD key_type;

	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, key, 0,
	                   KEY_READ, &key_h) != ERROR_SUCCESS)
		return NULL;

	if (RegQueryValueEx (key_h, subkey, NULL, &key_type, dir,
						 &len) != ERROR_SUCCESS)
	{
		RegCloseKey (key_h);
		return NULL;
	}

	if (key_type != REG_SZ)
		return NULL;

	if (access (dir, R_OK))
		return NULL;

	return dir;
}

static char *wsa_error (int err, int major, int minor)
{
	int          i;
	static char  msg[256];
	static char *p;

	typedef struct
	{
		int   err;
		char* errmsg;
	} WSAStartupError;

	static WSAStartupError wsa_errors[] =
	{
		{ WSASYSNOTREADY,
		  "The underlying network subsystem is not ready for network "
		  "communication (Error %d)."},
		{ WSAVERNOTSUPPORTED,
		  "Version %d.%d of Winsock is not provided by this particular"
		  "Winsock implementation (Error %d)."},
		{ WSAEINPROGRESS,
		  "A blocking Winsock 1.1 operation is in progress (Error %d)."},
		{ WSAEPROCLIM,
		  "The limit on the number of tasks supported by the Winsock "
		  "implementation has been reached (Error %d)."},
		{ WSAEFAULT,
		  "The lpWSAData is not a valid pointer (Error %d)."}
	};

	p = msg;
	snprintf (p, sizeof (msg), "Unknown error %d", err);

	for (i = 0; i < sizeof (wsa_errors); i++)
	{
		if (err == wsa_errors[i].err)
			snprintf (p, sizeof (msg), wsa_errors[i].errmsg, major, minor,
					  err);
	}

	return p;
}

static BOOL win32_init (const char *home, const char *local,
						const char *data, const char *plugin)
{
	WSADATA wsa_data;
	/* not sure why there is all this special stuff for debugging */
#ifdef LIBGIFT_DEBUG
	char    current_dir[_MAX_PATH] = {0};
	char   *home_tmp_dir;
#endif /* LIBGIFT_DEBUG */
	char    tmp[_MAX_PATH] = {0};
	char   *reg_dir;
	int     ret;

	/* initialize winsock2 library */
	if ((ret = WSAStartup (MAKEWORD (WS_MAJOR, WS_MINOR), &wsa_data)) != 0)
	{
		GIFT_ERROR ((wsa_error (ret, WS_MAJOR, WS_MINOR)));
		return FALSE;
	}

#ifdef LIBGIFT_DEBUG
	/* determine the current directory */
	GetCurrentDirectory (sizeof (current_dir), current_dir);
	if (current_dir)
		if (access (current_dir, R_OK))
			current_dir[0] = '\0';

	/* get the a HOME variable, if defined and accessable */
	home_tmp_dir = getenv ("HOME");
	if (home_tmp_dir)
		if (access (home_tmp_dir, R_OK))
			home_tmp_dir = NULL;
#endif /* LIBGIFT_DEBUG */

	/* initialize plugin dir */
	if (plugin)
	{
		/* cmd line override */
		plugin_dir = STRDUP (plugin);
	}
	else
	{
		if ((reg_dir = read_reg_dir (HKEY_LOCAL_MACHINE, GIFT_REG_KEY,
			GIFT_REG_SUBKEY)))
		{
			plugin_dir = STRDUP (reg_dir);
		}
#ifdef LIBGIFT_DEBUG
		else if (current_dir)
			plugin_dir = STRDUP (current_dir);
#endif /* LIBGIFT_DEBUG */
		else
		{
			GIFT_ERROR (("Can't find giFT directory. Please reinstall giFT: %s",
						GIFT_STRERROR()));
			return FALSE;
		}
	}

	/* initialize data_dir */
	if (data)
	{
		/* cmd line override */
		data_dir = STRDUP (data);
	}
	else
	{
		data_dir = malloc (strlen (plugin_dir) + 6); /* data/\0 */
		sprintf (data_dir, "%s/data", plugin_dir);
		if (access (data_dir, R_OK))
		{
			GIFT_ERROR (("Can't find %s directory. Please reinstall giFT.: %s",
						data_dir, GIFT_STRERROR()));
			return FALSE;
		}
	}


	/* initialize home_dir */
	if (home)
	{
		/* cmd line override */
		home_dir = STRDUP (home);
	}
	else
	{
		if ((reg_dir = read_reg_dir (HKEY_CURRENT_USER, GIFT_REG_KEY,
			GIFT_REG_SUBKEY)))
		{
			home_dir = STRDUP (reg_dir);
		}
#ifdef LIBGIFT_DEBUG
		else if (home_tmp_dir)
			home_dir = STRDUP (home_tmp_dir);
		else if (current_dir)
			home_dir = STRDUP (current_dir);
#endif /* LIBGIFT_DEBUG */
		else
		{
			GIFT_ERROR (("Can't find home directory. Please reinstall giFT."));
			return FALSE;
		}
	}

	/* initialize local_dir */
	if (local)
	{
		/* cmd line override */
		local_dir = STRDUP (local);
	}
	else
	{
		snprintf (tmp, sizeof (tmp), "%s/giftd.conf", home_dir);
		if (!access (tmp, R_OK))
			local_dir = STRDUP (home_dir);
		else
		{
			snprintf (tmp, sizeof (tmp), "%s/.giFT/giftd.conf", home_dir);
			if (!access (tmp, R_OK))
			{
				local_dir = malloc (strlen (home_dir) + 7); /* .giFT/\0 */
				sprintf (local_dir, "%s/.giFT", home_dir);
			}
			else
			{
				GIFT_ERROR (("Can't find giftd.conf Please reinstall giFT."));
				return FALSE;
			}
		}
	}

	return TRUE;
}

#endif /* !WIN32 */

BOOL platform_init (const char *home, const char *local,
                    const char *data, const char *plugin)
{
	BOOL ret;

	assert (active_children == NULL);
	active_children = dataset_new (DATASET_HASH);

#ifndef WIN32
	ret = unix_init (home, local, data, plugin);
#else /* WIN32 */
	ret = win32_init (home, local, data, plugin);
#endif /* !WIN32 */

	if (!ret)
	{
		GIFT_FATAL (("Unable to initialize giFT.  Please see above errors"
		             "for more information."));
	}

	return ret;
}

#ifndef WIN32
static int kill_child (ds_data_t *key, ds_data_t *value, void *udata)
{
	pid_t pid = *((pid_t *)key->data);

	kill (pid, SIGTERM);

	return DS_CONTINUE | DS_REMOVE;
}

/* cleanup all active child processes
 * NOTE: submitted by soapy@gmx.de */
void unix_cleanup (void)
{
	sigset_t sigset;
	sigset_t oldset;

	/* block signals to eliminate races accessing the list */
	sigfillset (&sigset);
	sigprocmask (SIG_BLOCK, &sigset, &oldset);

	/* deliver SIGTERM to all children */
	dataset_foreach_ex (active_children, DS_FOREACH_EX(kill_child), NULL);
	dataset_clear (active_children);

	/* unblock signals */
	sigprocmask (SIG_SETMASK, &oldset, NULL);
}
#endif /* !WIN32 */

void platform_cleanup (void)
{
#ifndef WIN32
	unix_cleanup ();
#else /* WIN32 */
	/* In a multithreaded environment, WSACleanup terminates Winsock operations
	 * for all threads. */
	if (WSACleanup () != 0)
		GIFT_ERROR ((platform_net_error()));
#endif /* !WIN32 */

	free (data_dir);
	free (plugin_dir);
	free (home_dir);
	free (local_dir);
}

/*****************************************************************************/
/* PLATFORM_GETTIMEOFDAY */

int platform_gettimeofday (struct timeval *tv, void *unused)
{
	int ret;

#ifndef WIN32
	ret = gettimeofday (tv, NULL);
#else /* WIN32 */
	struct _timeb tb;

	/* TODO: assign ret? */
	_ftime (&tb);

	tv->tv_sec  = tb.time;
	tv->tv_usec = tb.millitm * 1000;

	ret = 0;
#endif /* !WIN32 */

	return ret;
}

/*****************************************************************************/

static void subprocess_free (SubprocessData *sdata)
{
	if (sdata->data)
		free (sdata->data);

	free (sdata);
}

static void parent_wrapper (int fd, input_id id, SubprocessData *sdata)
{
	int ret = FALSE;

	/* call the parent function */
	if (sdata->pfunc)
		ret = sdata->pfunc (sdata, sdata->udata);

	/* cleanup if the parent func doesnt exist/returned FALSE */
	if (!ret)
	{
		input_remove_all (fd);
		subprocess_free (sdata);
	}
}

static SUBPROCESS (child_wrapper)
{
	SubprocessData *sdata;

	if (!(sdata = param))
		return FALSE;

	/* call the child function */
	(*sdata->cfunc) (sdata, sdata->udata);

	/* wake up the parent if its waiting */
	net_close (sdata->fd);

	/* free the data allocated previously */
	subprocess_free (sdata);

	return TRUE;
}

static int child_send (int fd, char *data, int len)
{
#ifndef NO_CHILD
	return net_send (fd, data, len);
#else /* NO_CHILD */
	return write (fd, data, len);      /* fd isnt a socket, use write */
#endif /* !NO_CHILD */
}

static int parent_recv (int fd, char *data, int len)
{
#ifndef NO_CHILD
	return recv (fd, data, len, 0);
#else /* NO_CHILD */
	return read (fd, data, len);       /* fd isnt a socket, use read () */
#endif /* !NO_CHILD */
}

int platform_child_sendmsg (SubprocessData *sdata, char *data, size_t len)
{
	String *s;
	int     ret;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return -1;

	/*
	 * Send the size of the message first
	 */
	string_appendu (s, (unsigned char *)&len, sizeof (size_t));
	string_appendu (s, data, len);

	ret = child_send (sdata->fd, s->str, s->len);

	string_free (s);

	return ret;
}

int platform_child_recvmsg (SubprocessData *sdata)
{
	int     ret;
	size_t  msg_size;
	char   *resized;

	sdata->len = 0;

	ret = parent_recv (sdata->fd, (char *)&msg_size, sizeof (size_t));

	if (ret <= 0)
		return ret;

	if (msg_size > 65535)
		return -1;

	if (sdata->data_len < msg_size)
	{
		if (!(resized = realloc (sdata->data, msg_size)))
			return -1;

		sdata->data     = resized;
		sdata->data_len = msg_size;
	}

	if ((ret = parent_recv (sdata->fd, sdata->data, msg_size)) != msg_size)
		return -1;

	sdata->len = msg_size;

	return msg_size;
}

static int child_socketpair (int *pfd)
{
#ifndef NO_CHILD
	return socketpair (AF_LOCAL, SOCK_STREAM, 0, pfd);
#else /* NO_CHILD */
	char tmpname[PATH_MAX];

# ifndef _MSC_VER
	snprintf (tmpname, sizeof (tmpname), "%s/gift_child-XXXXXX",
	          platform_local_dir());

	/* use a temporary file instead of a socketpair */
	if (!(pfd[1] = mkstemp (tmpname)))
		return -1;
# else /* _MSC_VER */
	/* TODO generize mkstemp () */
	char *p;
	FILE *fh;

	if (!(p = _tempnam (platform_local_dir (), "gift_child-"))) {
		return -1;
	}

	strcpy (tmpname, p);
	free (p);

	if (!(fh = fopen (tmpname, "wb")))
		return -1;

	if (!(pfd[1] = _fileno (fh)))
		return -1;
# endif /* !_MSC_VER */

	/* use a duplicate fd for reading */
	if ((pfd[0] = dup (pfd[1])) == -1)
	{
		close (pfd[1]);
		return -1;
	}

	/* remove the file so its deleted on close */
	if (remove (tmpname) == -1)
		GIFT_ERROR (("Error removing file %s: %s", tmpname, GIFT_STRERROR()));

	return 0;
#endif /* !NO_CHILD */
}

#ifndef NO_CHILD
/*
 * Create a new child context using fork [UNIX] or threads [win32]
 */
static int child_new (SubprocessData *pdata, int *pfd)
{
	SubprocessData *cdata;
	pid_t           cpid;

#ifdef WIN32
	HANDLE hThread;
#endif /* WIN32 */

#ifndef WIN32
	if ((cpid = fork()) == -1)
	{
		GIFT_ERROR (("fork: %s", GIFT_STRERROR()));
		return FALSE;
	}

	/* child */
	if (cpid == 0)
	{
		/* it's safe to use the parent's data, fork copies it */
		cdata = pdata;

		/* child is not allowed to read this descriptor */
		close (pfd[0]);

# ifdef HAVE_NICE
		nice (10);
# endif /* HAVE_NICE */

		/* restore SIGINT/SIGTERM behaviour, as we have no
		 * event loop in the child */
		signal (SIGTERM, SIG_DFL);
		signal (SIGINT, SIG_DFL);

		child_wrapper (cdata);

		_exit (0);
	}

	/* parent is not allowed to write this descriptor */
	close (pfd[1]);

	/* remember the child's pid so we may dispose of it on exit if it
	 * is still alive */
	dataset_insert (&active_children, &cpid, sizeof (cpid), "pid_t", 0);
#else /* WIN32 */
	/* copy the parent's data to avoid concurrent access */
	if (!(cdata = malloc (sizeof (SubprocessData))))
		return FALSE;

	memcpy (cdata, pdata, sizeof (SubprocessData));

	hThread = (HANDLE) _beginthreadex (NULL, (unsigned int) NULL,
	                                   child_wrapper, cdata, 0U,
	                                   (unsigned int *) &cpid);

	if (hThread == (HANDLE) -1 || hThread == (HANDLE) 0)
	{
		GIFT_ERROR (("_beginthreadex: %s", GIFT_STRERROR()));
		free (cdata);
		return FALSE;
	}

	/* So the system doesn't grind to a halt */
	SetThreadPriority (hThread, THREAD_PRIORITY_LOWEST);

	/* so we can look up the thread handle by the thread identifier why there
	 * is an ID *and* a handle is beyond me */
	dataset_insert (&active_children, &cpid, sizeof (cpid), hThread, 0);
#endif /* WIN32 */

	return TRUE;
}
#else /* NO_CHILD */
/*
 * Create a "child" by running it directly. The child fd points to a
 * temporary file.  The parent function will later read from that file.
 */
static int child_new (SubprocessData *pdata, int *pfd)
{
	SubprocessData *cdata;

	if (!(cdata = gift_memdup (pdata, sizeof (SubprocessData))))
		return FALSE;

	/* call the child directly -- this frees the child data */
	child_wrapper (cdata);

	/* the fd pair shares the pos ptr, so reset to the beginning */
	if (lseek (pfd[0], 0L, SEEK_SET) == -1)
	{
		GIFT_ERROR (("lseek: %s", GIFT_STRERROR()));
		return FALSE;
	}

	/* parent func will be called from select */
	return TRUE;
}
#endif /* !NO_CHILD */

/*
 * Create a child process.
 *
 * UNIX    - fork
 * Windows - threads
 */
int platform_child (ChildFunc c_func, ParentFunc p_func, void *udata)
{
	SubprocessData *pdata;
	int             pfd[2];

	if (!c_func)
		return FALSE;

	/* allocate the structure that will hold all the data we need to pass
	 * to the child/parent functions */
	if (!(pdata = MALLOC (sizeof (SubprocessData))))
		return FALSE;

	/* there needs to be some form of communication between child and parent,
	 * so we will establish it here */

	if (child_socketpair (pfd) == -1)
	{
		GIFT_ERROR (("socketpair: %s", platform_net_error()));
		free (pdata);
		return FALSE;
	}

	pdata->fd    = pfd[1];
	pdata->cfunc = c_func;
	pdata->pfunc = p_func;
	pdata->udata = udata;

	if (!child_new (pdata, pfd))
	{
		net_close (pfd[0]);
		net_close (pfd[1]);
		free (pdata);
		return FALSE;
	}

	pdata->fd = pfd[0];

	/* we are now the parent, start selecting the socket */
	input_add (pfd[0], pdata, INPUT_READ, (InputCallback)parent_wrapper, FALSE);

	return TRUE;
}

#ifndef WIN32
static void child_exit (int signum)
{
	waitpid (-1, NULL, WUNTRACED | WNOHANG);

#if 0
	pid_t    pid;
	sigset_t sigset;
	sigset_t oldset;

	/* block signals to eliminate races when accessing the list */
	sigfillset (&sigset);
	sigprocmask (SIG_BLOCK, &sigset, &oldset);

	/* exorcise zombies and remove their pids from our list */
	while ((pid = waitpid (-1, NULL, WUNTRACED | WNOHANG)) > 0)
		dataset_remove (active_children, &pid, sizeof (pid));

	if (pid == -1)
		GIFT_ERROR (("waitpid: %s", GIFT_STRERROR()));

	/* unblock signals */
	sigprocmask (SIG_SETMASK, &oldset, NULL);
#endif
}
#endif /* !WIN32 */

/*****************************************************************************/
/* PLATFORM_VERSION */

/* determine the giFT version string (includes platform specific data) */
char *platform_version (void)
{
	/* gift_version does not exist, so calculate it once */
	if (!gift_version)
	{
		size_t alloc_len;
		size_t len;
#ifndef WIN32
		struct utsname os;
#else /* WIN32 */
		OSVERSIONINFO vinfo;
		char         *win_str = "Windows";
#endif /* !WIN32 */

		alloc_len = 2 + strlen (GIFT_PACKAGE) + strlen (GIFT_VERSION);

		if (!(gift_version = malloc (alloc_len)))
			return NULL;

		len = snprintf (gift_version, alloc_len, "%s/%s",
		                GIFT_PACKAGE, GIFT_VERSION);

		/* if we have any extra OS-specific data to use, add it here
		 * NOTE: realloc'ing is less efficient, of course.  However, this is
		 * much easier to manipulate the above version string, as well as
		 * allowing us to return _something_ even if the OS fails to
		 * determine itself for whatever reason */
#ifndef WIN32
		if (uname (&os) == -1)
			return gift_version;

		alloc_len += 32;
		alloc_len += strlen (os.sysname);
		alloc_len += strlen (os.release);
		alloc_len += strlen (os.machine);

		if (!(gift_version = realloc (gift_version, alloc_len)))
			return NULL;

		len += snprintf (gift_version + len, alloc_len - len, " (%s %s %s)",
		                 os.sysname, os.release, os.machine);
#else /* WIN32 */
		vinfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

		if (!GetVersionEx (&vinfo))
			return gift_version;

		/* isn't the Windows API so pretty by comparison? */
		switch (vinfo.dwPlatformId)
		{
		 case VER_PLATFORM_WIN32_WINDOWS:
			if (vinfo.dwMajorVersion == 4 && vinfo.dwMinorVersion == 0)
				win_str = "95";
			else if (vinfo.dwMajorVersion == 4 && vinfo.dwMinorVersion == 90)
				win_str = "ME";
			else
				win_str = "98";
			break;
		 case VER_PLATFORM_WIN32_NT:
			if (vinfo.dwMajorVersion <= 4)
				win_str = "NT";
			else if (vinfo.dwMajorVersion == 5 && vinfo.dwMinorVersion == 0)
				win_str = "2000";
			else if (vinfo.dwMajorVersion == 5 && vinfo.dwMinorVersion == 1)
				win_str = "XP";
			break;
		}

		alloc_len += 64;
		alloc_len += strlen (win_str);
		alloc_len += strlen (vinfo.szCSDVersion);

		if (!(gift_version = realloc (gift_version, alloc_len)))
			return NULL;

		len += snprintf (gift_version + len, alloc_len - len,
		                 " (Windows %s %d.%d.%d %s)",
		                 win_str, vinfo.dwMajorVersion, vinfo.dwMinorVersion,
		                 vinfo.dwBuildNumber && 0xffff, vinfo.szCSDVersion);
#endif /* !WIN32 */
	}

	return gift_version;
}

/*****************************************************************************/
/* PLATFORM ERROR HANDLERS */

#ifdef WIN32
char *win32_format_message (const DWORD err)
{
	static char buf[2048];
	int         ret;

	/* just keep telling yourself -- Microsoft will be gone soon */
	ret =
	    FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                   NULL, errno, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
	                   (LPTSTR) &buf, sizeof (buf), NULL);

	if (ret == 0)
		snprintf (buf, sizeof(buf), "Unknown Windows error %d", err);

	return buf;
}
#endif /* WIN32 */

unsigned long platform_errno (void)
{
#ifndef WIN32
	return (unsigned long)errno;
#else /* WIN32 */
	return (unsigned long)GetLastError();
#endif /* !WIN32 */
}

char *platform_error (void)
{
#ifndef WIN32
	return strerror (errno);
#else /* WIN32 */
	return win32_format_message (GetLastError());
#endif /* !WIN32 */
}

unsigned long platform_net_errno (void)
{
#ifndef WIN32
	return (unsigned long)errno;
#else /* WIN32 */
	/* F*cking windows has a different error function for socket functions. */
	return (unsigned long)WSAGetLastError();
#endif /* !WIN32 */
}

#ifdef WIN32

char *win32_wsa_strerror (int wsaerrno) {
	static char buf[256];

	switch (wsaerrno)
	{
	 case 0:
		return "Success";
	 case WSAEACCES:                   /* 10013 */
		return "Permission denied.";
	 case WSAEADDRINUSE:               /* 10048 */
		return "Address already in use.";
	 case WSAEADDRNOTAVAIL:            /* 10049 */
		return "Cannot assign requested address.";
	 case WSAEAFNOSUPPORT:             /* 10047 */
		return "Address family not supported by protocol family.";
	 case WSAEALREADY:                 /* 10037 */
		return "Operation already in progress.";
	 case WSAECONNABORTED:             /* 10053 */
		return "Software caused connection abort.";
	 case WSAECONNREFUSED:             /* 10061 */
		return "Connection refused.";
	 case WSAECONNRESET:               /* 10054 */
		return "Connection reset by peer.";
	 case WSAEDESTADDRREQ:             /* 10039 */
		return "Destination address required.";
	 case WSAEFAULT:                   /* 10014 */
		return "Bad address.";
	 case WSAEHOSTDOWN:                /* 10064 */
		return "Host is down.";
	 case WSAEHOSTUNREACH:             /* 10065 */
		return "No route to host.";
	 case WSAEINPROGRESS:              /* 10036 */
		return "Operation now in progress.";
	 case WSAEINTR:                    /* 10004 */
		return "Interrupted function call.";
	 case WSAEINVAL:                   /* 10022 */
		return "Invalid argument.";
	 case WSAEISCONN:                  /* 10056 */
		return "Socket is already connected.";
	 case WSAEMFILE:                   /* 10024 */
		return "Too many open files.";
	 case WSAEMSGSIZE:                 /* 10040 */
		return "Message too long.";
	 case WSAENETDOWN:                 /* 10050 */
		return "Network is down.";
	 case WSAENETRESET:                /* 10052 */
		return "Network dropped connection on reset.";
	 case WSAENETUNREACH:              /* 10051 */
		return "Network is unreachable.";
	 case WSAENOBUFS:                  /* 10055 */
		return "No buffer space available.";
	 case WSAENOPROTOOPT:              /* 10042 */
		return "Bad protocol option.";
	 case WSAENOTCONN:                 /* 10057 */
		return "Socket is not connected.";
	 case WSAENOTSOCK:                 /* 10038 */
		return "Socket operation on nonsocket.";
	 case WSAEOPNOTSUPP:               /* 10045 */
		return "Operation not supported.";
	 case WSAEPFNOSUPPORT:             /* 10046 */
		return "Protocol family not supported.";
	 case WSAEPROCLIM:                 /* 10067 */
		return "Too many processes.";
	 case WSAEPROTONOSUPPORT:          /* 10043 */
		return "Protocol not supported.";
	 case WSAEPROTOTYPE:               /* 10041 */
		return "Protocol wrong type for socket.";
	 case WSAESHUTDOWN:                /* 10058 */
		return "Cannot send after socket shutdown.";
	 case WSAESOCKTNOSUPPORT:          /* 10044 */
		return "Socket type not supported.";
	 case WSAETIMEDOUT:                /* 10060 */
		return "Connection timed out.";
#if 0
	 case WSATYPE_NOT_FOUND:           /* 10109 */
		return "Class type not found.";
#endif
	 case WSAEWOULDBLOCK:              /* 10035 */
		return "Resource temporarily unavailable.";
	 case WSAHOST_NOT_FOUND:           /* 11001 */
		return "Host not found.";
	 case WSANOTINITIALISED:           /* 10093 */
		return "Successful WSAStartup not yet performed.";
	 case WSANO_DATA:                  /* 11004 */
		return "Valid name, no data record of requested type.";
	 case WSANO_RECOVERY:              /* 11003 */
		return "This is a nonrecoverable error.";
	 case WSASYSNOTREADY:              /* 10091 */
		return "Network subsystem is unavailable.";
	 case WSATRY_AGAIN:                /* 11002 */
		return "Nonauthoritative host not found.";
	 case WSAVERNOTSUPPORTED:          /* 10092 */
		return "Winsock.dll version out of range.";
	 case WSAEDISCON:                  /* 10101 */
		return "Graceful shutdown in progress.";
#ifdef _WINSOCK2API_

/* cygwin fixes */
# ifndef WSASYSCALLFAILURE
#  define WSASYSCALLFAILURE 10107
# endif
# ifndef WSATYPE_NOT_FOUND
#  define WSATYPE_NOT_FOUND 10109
# endif

	 case WSA_INVALID_HANDLE:          /* OS dependent */
		return "Specified event object handle is invalid.";
	 case WSA_INVALID_PARAMETER:       /* OS dependent */
		return "One or more parameters are invalid.";
#if 0
	 case WSAINVALIDPROCTABLE:         /* OS dependent */
		return "Invalid procedure table from service provider.";
	 case WSAINVALIDPROVIDER:          /* OS dependent */
		return "Invalid service provider version number.";
	 case WSAPROVIDERFAILEDINIT:       /* OS dependent */
		return "Unable to initialize a service provider.";
#endif
	 case WSA_IO_INCOMPLETE:           /* OS dependent */
		return "Overlapped I/O event object not in signaled state.";
	 case WSA_IO_PENDING:              /* OS dependent */
		return "Overlapped operations will complete later.";
	 case WSA_NOT_ENOUGH_MEMORY:       /* OS dependent */
		return "Insufficient memory available.";
	 case WSASYSCALLFAILURE:           /* OS dependent (10107) */
		return "System call failure.";
	 case WSA_OPERATION_ABORTED:       /* OS dependent */
		return "Overlapped operation aborted.";
	 case WSAENOMORE:                  /* 10102 */
		return "WSAENOMORE";
	 case WSAECANCELLED:               /* 10103 */
		return "WSAECANCELLED";
	 case WSAEINVALIDPROCTABLE:        /* 10104 */
		return "WSAEINVALIDPROCTABLE";
	 case WSAEINVALIDPROVIDER:         /* 10105 */
		return "WSAEINVALIDPROVIDER";
	 case WSAEPROVIDERFAILEDINIT:      /* 10106 */
		return "WSAEPROVIDERFAILEDINIT";
	 case WSASERVICE_NOT_FOUND:        /* 10108 */
		return "WSASERVICE_NOT_FOUND";
	 case WSATYPE_NOT_FOUND:           /* 10109 */
		return "WSATYPE_NOT_FOUND";
	 case WSA_E_NO_MORE:               /* 10110 */
		return "WSA_E_NO_MORE";
	 case WSA_E_CANCELLED:             /* 10111 */
		return "WSA_E_CANCELLED";
	 case WSAEREFUSED:                 /* 10112 */
		return "WSAEREFUSED";

#endif /* _WINSOCK2API_ */
	 default:
		snprintf (buf, sizeof (buf), "Unknown winsock error %d.", wsaerrno);
		return buf;
	}
}

#endif /* WIN32 */

char *platform_net_error (void)
{
#ifndef WIN32
	return strerror (errno);
#else /* WIN32 */
	/* F*cking windows has a different error function for socket functions. */
	return win32_wsa_strerror (WSAGetLastError());
#endif /* !WIN32 */
}

/*****************************************************************************/
/* socketpair function missing (of course) on windows */

#ifndef HAVE_SOCKETPAIR

#ifndef PF_LOCAL
# define PF_LOCAL AF_LOCAL
#endif

static int invalid_socket (int socket)
{
#ifdef WIN32
	return (socket == INVALID_SOCKET);
#else
	return (socket < 0);
#endif /* WIN32 */
}

#if !defined (WIN32) && !defined (closesocket)
# define closesocket close
#endif /* !WIN32 */

static int socketpair (int family, int type, int protocol, int sv[2])
{
	int insock, outsock, newsock;
	struct sockaddr_in sock_in, sock_out;
	int len;

	/* windowz only has AF_INET (we use that for AF_LOCAL too) */
	if (family != AF_LOCAL && family != AF_INET)
		return -1;

	/* STRAM and DGRAM sockets only */
	if (type != SOCK_STREAM && type != SOCK_DGRAM)
		return -1;

	/* yes, we all love windoze */
	if ((family == AF_LOCAL && protocol != PF_UNSPEC && protocol != PF_LOCAL)
	    || (family == AF_INET && protocol != PF_UNSPEC && protocol != PF_INET))
		return -1;

	/* create the first socket */
	newsock = socket (AF_INET, type, 0);
	if (invalid_socket (newsock))
	{
		GIFT_TRACE (("first socket call failed"));
		return -1;
	}

	/* bind the socket to any unused port */
	sock_in.sin_family      = AF_INET;
	sock_in.sin_port        = 0;
	sock_in.sin_addr.s_addr = INADDR_ANY;
	if (bind (newsock, (struct sockaddr *) &sock_in, sizeof (sock_in)) < 0)
	{
		GIFT_TRACE (("bind failed"));
		closesocket (newsock);
		return -1;
	}
	len = sizeof (sock_in);
	if (getsockname (newsock, (struct sockaddr *) &sock_in, &len) < 0)
	{
		GIFT_TRACE (("getsockname error"));
		closesocket (newsock);
		return -1;
	}

	/* For stream sockets, create a listener */
	if (type == SOCK_STREAM)
		listen (newsock, 2);

	/* create a connecting socket */
	outsock = socket (AF_INET, type, 0);
	if (invalid_socket (outsock))
	{
		GIFT_TRACE (("second socket call failed"));
		closesocket (newsock);
		return -1;
	}

	/* For datagram sockets, bind the 2nd socket to an unused address, too */
	if (type == SOCK_DGRAM)
	{
		sock_out.sin_family       = AF_INET;
		sock_out.sin_port        = 0;
		sock_out.sin_addr.s_addr = INADDR_ANY;
		if (bind (outsock, (struct sockaddr *) &sock_out, sizeof (sock_out)) < 0)
		{
			GIFT_TRACE (("bind failed"));
			closesocket (newsock);
			closesocket (outsock);
			return -1;
		}
		len = sizeof (sock_out);
		if (getsockname (outsock, (struct sockaddr *) &sock_out, &len) < 0)
		{
			GIFT_TRACE (("getsockname error"));
			closesocket (newsock);
			closesocket (outsock);
			return -1;
		}
	}

	/* Force IP address to loopback */
	sock_in.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	if (type == SOCK_DGRAM)
		sock_out.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

	/* Do a connect */
	if (connect (outsock, (struct sockaddr *) &sock_in, sizeof (sock_in)) < 0)
	{
		GIFT_TRACE (("connect error"));
		closesocket (newsock);
		closesocket (outsock);
		return -1;
	}

	if (type == SOCK_STREAM)
	{
		/* For stream sockets, accept the connection and close the listener */
		len = sizeof (sock_in);
		insock = accept (newsock, (struct sockaddr *) &sock_in, &len);
		if (invalid_socket (insock))
		{
			GIFT_TRACE (("accept error"));
			closesocket (newsock);
			closesocket (outsock);
			return -1;
		}
		closesocket (newsock);
	}
	else
	{
		/* For datagram sockets, connect the 2nd socket */
		if (connect (newsock, (struct sockaddr *) &sock_out, sizeof (sock_out)) < 0)
		{
			GIFT_TRACE (("connect error"));
			closesocket (newsock);
			closesocket (outsock);
			return -1;
		}
		insock = newsock;
	}

	/* set the descriptors */
	sv[0] = insock;
	sv[1] = outsock;

	/* we've done it */
	return 0;
}

#endif /* !HAVE_SOCKETPAIR */
