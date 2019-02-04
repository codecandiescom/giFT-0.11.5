/*
 * $Id: giftd.c,v 1.24 2003/10/16 18:50:55 jasta Exp $
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

#include "plugin/protocol.h"

#include "opt.h"

#include "lib/file.h"                  /* file_exists() */
#include "lib/mime.h"                  /* mime_init(), mime_cleanup() */
#include "download.h"                  /* download_stop() */
#include "download_state.h"            /* download_state_recover() */
#include "upload.h"                    /* upload_stop() */
#include "plugin.h"                    /* plugin_init(), plugin_start() */
#include "if_port.h"                   /* if_port_init() */
#if 0
#include "if_httpd.h"                  /* if_httpd_init() */
#endif
#include "share_cache.h"               /* share_update_index(), --index-only */

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif /* HAVE_SIGNAL_H */

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif /* HAVE_LIMITS_H */

#ifdef USE_LTDL
# include <ltdl.h>
#endif /* USE_LTDL */

/*****************************************************************************/
/* included for gift_version() */

#ifdef USE_IMAGEMAGICK
# include <magick/api.h>
# include <magick/version.h>
#endif

#ifdef USE_PERL
# include <EXTERN.h>
# ifndef _SEM_SEMUN_UNDEFINED
#  define HAS_UNION_SEMUN
# endif
# undef MIN
# undef MAX
# include <perl.h>
# include <XSUB.h>
#endif /* USE_PERL */

#ifndef USE_LTDL
# ifdef PLUGIN_OPENFT
#  include "OpenFT/ft_openft.h"
# endif /* PLUGIN_OPENFT */
# ifdef PLUGIN_GNUTELLA
#  include "Gnutella/gt_gnutella.h"
# endif /* PLUGIN_GNUTELLA */
#endif

/*****************************************************************************/

/* TODO: get this from autoconf */
#ifdef WIN32
# define PLUGIN_PATHPFX ""
# define PLUGIN_PATHSFX ".dll"
#else /* !WIN32 */
# define PLUGIN_PATHPFX "lib"
# define PLUGIN_PATHSFX ".la"
#endif /* WIN32 */

/*****************************************************************************/

/* provide these just in case <stdlib.h> fails to do so */
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS   0
# define EXIT_FAILURE   1
#endif /* !EXIT_SUCCESS */

/*****************************************************************************/

static const char *msg_usage =
"\n"
"Usage: %s [options]\n"
"\n"
"%s"
"\n";

static const char *msg_copyleft =
"Copyright (c) 2001-2003 giFT project (http://giftproject.org/)\n"
"This is free software; see the source for copying conditions.  There is NO\n"
"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
"\n";

static const char *msg_setuperr =
"\n"
"*** ERROR: Your setup is incomplete ***\n"
"\n"
"You will need to run gift-setup and be sure that you read absolutely\n"
"every configuration option (no, really).  Some default configuration\n"
"values are considered illegal, and will raise this error message.  If you\n"
"suspect that you have configured giFT properly, consult the conf files in\n"
"%s for diagnostic purposes.\n"
"\n"
"If you are still having problems you should consult the QUICKSTART guide\n"
"available from the standard giFT distribution.\n"
"\n";

static const char *msg_iferr =
"Failed to load interface subsystem\n"
"\n"
"NOTE:\n"
"There may be another giFT daemon running on this host.  Check to see if the\n"
"interface port (%d) is currently in use by another process.\n";

/*****************************************************************************/

Config       *gift_conf  = NULL;       /* crappy interface to gift.conf  */

/*****************************************************************************/

static BOOL   cl_help      = FALSE;
static BOOL   cl_version   = FALSE;
static BOOL   cl_daemon    = FALSE;
static BOOL   cl_idxonly   = FALSE;
static Array *cl_opt       = NULL;
static Array *cl_proto     = NULL;
static BOOL   cl_quiet     = FALSE;
static int    cl_verbose   = 0;
static char  *cl_logfile   = NULL;
static char  *cl_homedir   = NULL;
static char  *cl_localdir  = NULL;
static char  *cl_datadir   = NULL;
static char  *cl_plugindir = NULL;

static giftopt_t giftopts[] =
{
	{ OPT_FLG, &cl_help,      "help",       'h', NULL, "Show this help message" },
	{ OPT_FLG, &cl_version,   "version",    'V', NULL, "Output version information and exit" },
	{ OPT_FLG, &cl_daemon,    "daemon",     'd', NULL, "Fork to the background" },
	{ OPT_FLG, &cl_idxonly,   "index-only", 'x', NULL, "Update the shares database and exit" },
	{ OPT_LST, &cl_opt,       "opt",        'o', NULL, "Run-time configuration override" },
	{ OPT_LST, &cl_proto,     "protocol",   'p', NULL, "Load the specified plugin (overrides conf)" },
	{ OPT_FLG, &cl_quiet,     "quiet",      'q', NULL, "Disable logging" },
	{ OPT_CNT, &cl_verbose,   "verbose",    'v', NULL, "Increase the verbosity level (default=0)" },
	{ OPT_STR, &cl_logfile,   "log-file",   'l', NULL, "Specify an alternative logging path" },
	{ OPT_STR, &cl_homedir,   "home-dir",     0, "GIFT_HOME_DIR",   "Override home dir (default $HOME)" },
	{ OPT_STR, &cl_localdir,  "local-dir",    0, "GIFT_LOCAL_DIR",  "Override local dir (default $HOME/.giFT)" },
	{ OPT_STR, &cl_datadir,   "data-dir",     0, "GIFT_DATA_DIR",   "Override compile-time DATA_DIR" },
	{ OPT_STR, &cl_plugindir, "plugin-dir",   0, "GIFT_PLUGIN_DIR", "Override compile-time PLUGIN_DIR" },
	{ OPT_END }
};

/*****************************************************************************/

static BOOL parse_proto_path (char *path, char *namebuf, size_t namebuf_size);

/*****************************************************************************/

static void message (char *str)
{
#ifndef _WINDOWS
	fprintf (stdout, str);
#else
	MessageBox (0, str, "giFT", MB_OK | MB_ICONINFORMATION);
#endif
}

static void error (char *str)
{
#ifndef _WINDOWS
	fprintf (stderr, str);
#else
	MessageBox (0, str, "giFT Error", MB_OK | MB_ICONERROR);
#endif
}

/*****************************************************************************/

static void gift_usage (char *argv0, int exitval)
{
	char *usage;

	usage = opt_usage_capt (giftopts);
	assert (usage != NULL);

	message (stringf (msg_usage, argv0, usage));
	free (usage);

	exit (exitval);
}

static void add_avail_plugins (String *s)
{
#ifdef USE_LTDL
	DIR           *dir;
	struct dirent *de;
	static char    pname[64];
#endif /* USE_LTDL */

	string_appendf (s,
	                "\n"
	                "Available plugins:\n");

#ifdef USE_LTDL
	if (!(dir = file_opendir (platform_plugin_dir())))
	{
		string_appendf (s, "\tCan't open %s: %s\n",
		                platform_plugin_dir(), GIFT_STRERROR());
		return;
	}

	while ((de = file_readdir (dir)))
	{
		Protocol *p;
		char     *path;
		char     *d_name_sfx;
		size_t    sfx_len;
		BOOL      ret;

		if (strcmp (de->d_name, ".") == 0 || strcmp (de->d_name, "..") == 0)
			continue;

		/* require the plugin prefix and suffix so that we know the parsing
		 * will succeed */
		if (strncmp (de->d_name, PLUGIN_PATHPFX, strlen (PLUGIN_PATHPFX)))
			continue;

		sfx_len    = strlen (PLUGIN_PATHSFX);
		d_name_sfx = de->d_name + strlen (de->d_name) - sfx_len;

		if (strncmp (d_name_sfx, PLUGIN_PATHSFX, sfx_len))
			continue;

		/* we are duping because god knows what the plugin does with
		 * stringf internally... */
		path = stringf_dup ("%s/%s", platform_plugin_dir(), de->d_name);

		/* extract the actual plugin name, such as "OpenFT", so that we
		 * can use ::plugin_init */
		ret = parse_proto_path (path, pname, sizeof (pname));
		assert (ret == TRUE);

		if ((p = plugin_init (path, pname)))
		{
			string_appendf (s, "\t%s ", p->name);

			if (p->version_str)
				string_append (s, p->version_str);
			else
				string_append (s, "[no defined version]");

			string_append (s, "\n");
		}
	}

	file_closedir (dir);
#else /* !USE_LTDL */
# ifdef PLUGIN_OPENFT
	string_appendf (s, "\tOpenFT %i.%i.%i-%i\n", OPENFT_MAJOR, OPENFT_MINOR,
	                OPENFT_MICRO, OPENFT_REV);
# endif /* PLUGIN_OPENFT */
# ifdef PLUGIN_GNUTELLA
	string_appendf (s, "\tGnutella %s\n", GT_VERSION);
# endif /* PLUGIN_GNUTELLA */

#endif /* USE_LTDL */
}

static void gift_version (void)
{
	String *s;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
	{
		GIFT_ERROR (("unable to allocate version buffer"));
		return;
	}

	string_appendf (s, "%s %s ", "giftd", VERSION);   /* PACKAGE=giFT */
	string_append (s, "(Built " BUILD_DATE ")\n\n");

	string_append (s, msg_copyleft);

	string_append (s, "Compile-time support:\n");
#ifdef USE_IMAGEMAGICK
	string_appendf (s, "\tImageMagick %d.%d.%d\n",
	                (MagickLibVersion >> 8) & 0xf,
	                (MagickLibVersion >> 4) & 0xf,
	                (MagickLibVersion) & 0xf);
#endif
#ifdef USE_LIBVORBIS
	string_appendf (s, "\tOgg Vorbis %i.%i\n", 1, 0);
#endif
#ifdef USE_PERL
	string_appendf (s, "\tPerl %i.%i.%i scripting support\n",
	                PERL_REVISION, PERL_VERSION, PERL_SUBVERSION);
#endif
#ifdef USE_LTDL
	string_appendf (s, "\tDynamic library loading (via ltdl)\n");
#endif
#ifndef WIN32
	string_append (s,
	               "\n"
	               "Compile-time environment paths:\n"
	               "\tDATA_DIR=" DATA_DIR "\n"
	               "\tPLUGIN_DIR=" PLUGIN_DIR "\n");
#endif /* !WIN32 */

	if (cl_homedir || cl_localdir || cl_datadir || cl_plugindir)
	{
		string_append (s,
					   "\n"
					   "Run-time environment path overrides:\n");

		if (cl_homedir)
			string_appendf (s, "\tHOME_DIR=%s\n", cl_homedir);

		if (cl_localdir)
			string_appendf (s, "\tLOCAL_DIR=%s\n", cl_localdir);

		if (cl_datadir)
			string_appendf (s, "\tDATA_DIR=%s\n", cl_datadir);

		if (cl_plugindir)
			string_appendf (s, "\tPLUGIN_DIR=%s\n", cl_plugindir);
	}

	add_avail_plugins (s);

	message (s->str);
	string_free (s);

	exit (EXIT_SUCCESS);
}

static BOOL gift_parse_argv (int argc, char **argv)
{
	if (!(opt_parse (&argc, &argv, giftopts)) || cl_help)
	{
		gift_usage (argv[0], !cl_help);
		return cl_help;
	}

	return TRUE;
}

static BOOL gift_handle_argv (int argc, char **argv)
{
	/* TODO */
	if (cl_opt)
		GIFT_FATAL (("sorry, -o isn't implemented yet"));

	if (cl_version)
		gift_version ();

	if (cl_daemon)
	{
#ifndef WIN32
		pid_t cpid;

		if ((cpid = fork ()) != 0)
		{
			GIFT_INFO (("giFT: %u", cpid));
			_exit (0);
		}

		setsid ();

		close (STDIN_FILENO);
		close (STDOUT_FILENO);
		close (STDERR_FILENO);
#endif /* !WIN32 */
	}
#ifdef WIN32
	else
	{
		/*
		 * cl_daemon is required to be on when using the Windows port.  The
		 * main reason for doing this is to encourage users to launch from
		 * giFTtray instead of directly.  Specifying -d does nothing but
		 * allow you to run the software.
		 */
		error ("Please use giFTtray to start giFT or use the -d option.");
		gift_usage (argv[0], EXIT_FAILURE);
	}
#endif /* WIN32 */


	return TRUE;
}

/*****************************************************************************/

static int init_ltdl (void)
{
#ifdef USE_LTDL
	if (lt_dlinit ())
	{
		GIFT_ERROR (("lt_dlinit failed...bummer"));
		return FALSE;
	}
#endif /* USE_LTDL */

	return TRUE;
}

static Config *check_config (void)
{
	Config *conf;

	/* the result of gift_config_new is not checked because we can rely on
	 * config_get_int returning 0 if passed an invalid parameter (see below) */
	conf = gift_config_new ("giFT");

	if (!config_get_int (conf, "main/setup=0"))
	{
		error (stringf (msg_setuperr, gift_conf_path ("")));
		exit (EXIT_FAILURE);
	}

	return conf;
}

static int gift_init (void)
{
	/*
	 * Initialize the logging system first so that the subsequent systems may
	 * use it internally.  We cannot complete this step until the platform
	 * code has been initialized and we can access the configuration/data
	 * paths.
	 */
	if (!log_init (GLOG_STDERR, "giftd", 0, 0, NULL))
		return FALSE;

	/*
	 * Initialize the platform-specific code.  Most importantly we need the
	 * environment paths to be initialized so that we may use gift_conf_path
	 * to direct the log output.
	 */
	if (!platform_init (cl_homedir, cl_localdir, cl_datadir, cl_plugindir))
		return FALSE;

	/*
	 * Intialize the mime code. This will read mime.types into memory
	 * so we can have fast lookup on file extensions -> mimetype.
	 */
	if (!mime_init ())
		return FALSE;

	/*
	 * Initialize the event system so that we can begin adding sockets and
	 * timers.  Really all this does is zero a couple of arrays, so it had
	 * better not fail.
	 */
	event_init ();

	/*
	 * Initialize the dynamic library loader if necessary.
	 */
	if (!init_ltdl ())
		return FALSE;

	/*
	 * Setup signal handlers to gracefully exit at the users request.  Please
	 * note that we are simply aborting the event loop, allowing the logic in
	 * gift_main to handle the graceful part of the quit.
	 */
#ifdef HAVE_SIGNAL
	signal (SIGINT, event_quit);
	signal (SIGTERM, event_quit);
#endif /* HAVE_SIGNAL */
	/*
	 * Load the main configuration file into memory before command line
	 * arguments are parsed.  This is done so that --opt may appropriately
	 * override options here.
	 */
	if (!(gift_conf = check_config ()))
		return FALSE;

	/*
	 * Hooray!
	 */
	return TRUE;
}

/*****************************************************************************/

static void clear_transfers (void)
{
	List *ptr;

	/* these functions do not build the list, they are simple accessors, so
	 * this is safe and cheap */
	while ((ptr = download_list ()))
		download_stop (ptr->data, FALSE);

	while ((ptr = upload_list ()))
		upload_stop (ptr->data, FALSE);
}

static int gift_finish (void)
{
	clear_transfers ();                /* also updates to the interfaces */
	if_port_cleanup ();                /* unload interface protocol handlers */
#if 0
	if_httpd_finish ();                /* unload http interface handlers */
#endif
	share_clear_index ();              /* clear local shares held in mem */
	plugin_unloadall ();               /* cleanly destroy all plugins */
	mime_cleanup ();                   /* cleanup mime data structures */
	platform_cleanup ();               /* im getting sick of this */
	config_free (gift_conf);           /* redundant documentation */
	log_cleanup ();                    /* so i think ill stop */

#ifdef USE_LTDL
	lt_dlexit ();
#endif /* USE_LTDL */

	/* TODO: DO A LOT MORE!!!!!!! */
	return TRUE;
}

/*****************************************************************************/

static void init_perl (void)
{
#ifdef USE_PERL
	perl_init ();
	perl_autoload ();
#endif /* USE_PERL */
}

static int init_interface (void)
{
	int ret;

	if (!(ret = if_port_init (INTERFACE_PORT)))
		GIFT_FATAL ((msg_iferr, (int)INTERFACE_PORT));

#if 0
	if (!(ret = if_httpd_init (1212)))
		GIFT_FATAL ((msg_iferr, (int)1212));
#endif

	return ret;
}

static BOOL parse_proto_path (char *path, char *namebuf, size_t namebuf_size)
{
	char *proto;
	char *name;

	/* we need to hack out the actual protocol name from the path */
	if ((name = strrchr (path, '/')))
		name++;
	else
		name = path;

	if (strncmp (name, "lib", 3) == 0)
		name += 3;

	if (!(proto = strchr (name, '.')))
		return FALSE;

	gift_strncpy (namebuf, name, MIN ((namebuf_size - 1), (proto - name)));

	return TRUE;
}

static Protocol *load_plugin (char *proto)
{
	Protocol   *p;
	char       *path = NULL;
	char       *name = NULL;
#ifdef USE_LTDL
	static char pathbuf[PATH_MAX];
	static char namebuf[64];
#endif /* USE_LTDL */

#ifndef USE_LTDL
	name = proto;
#else /* USE_LTDL */
	/* try the protocol as through it was a loadable path first, otherwise we
	 * will be constructing the path from the configured environment */
	if (file_exists (proto))
	{
		path = proto;
		name = namebuf;

		if (!(parse_proto_path (path, namebuf, sizeof (namebuf))))
		{
			GIFT_ERROR (("unable to parse plugin name out of %s", path));
			return NULL;
		}
	}
	else
	{
		/* construct the full path */
		snprintf (pathbuf, sizeof (pathbuf) - 1, "%s/%s%s%s",
		          platform_plugin_dir(),
		          PLUGIN_PATHPFX, proto, PLUGIN_PATHSFX);

		path = pathbuf;
		name = proto;
	}
#endif /* !USE_LTDL */

	if (!(p = plugin_init (path, name)))
		return NULL;

	/*
	 * WARNING: HACK because we know init_shares immediately follows
	 * load_protocols and it only needs them initialized!  The design needs
	 * to be improved to have better control over how the plugins are
	 * initialized and started.
	 */
	if (!cl_idxonly)
		plugin_start (p);

	return p;
}

static int load_protocols (void)
{
	char *plist, *plist0;
	char *pname;

	/* command line plugins were specified, load these instead of the
	 * conf plugins */
	if (cl_proto)
	{
		while ((pname = array_shift (&cl_proto)))
			load_plugin (pname);

		array_unset (&cl_proto);
		return TRUE;
	}

	/* access the : separated list of all protocol plugins that the daemon
	 * should load */
	if (!(plist = config_get_str (gift_conf, "main/plugins")))
	{
		GIFT_WARN (("No plugins requested."));
		return FALSE;
	}

	/* config_get_str returns locally protected memory, and string_sep will
	 * need to modify (grr!) */
	plist0 = plist = STRDUP (plist);

	/* grab each protocol name and construct an appropriate absolute path
	 * for loading */
	while ((pname = string_sep (&plist, ":")))
		load_plugin (pname);

	/* tidy up */
	free (plist0);

	/* plugins loaded, ready to leech porn! */
	return TRUE;
}

static int init_shares (void)
{
	/* --index-only told us to update ~/.giFT/shares and exit, so that's
	 * exactly what we'll do */
	if (cl_idxonly)
	{
		share_write_index (NULL);
		exit (EXIT_SUCCESS);
	}

	/* automatic resync timer */
	share_init_timer (RESYNC_INTERVAL);

	/*
	 * Ensure that the shares database is up to date by rescanning the entire
	 * root path and checking mtimes of files cached and those on disk.  This
	 * function will also handle reading of the resulting shares database
	 * back into memory and syncing to protocols.  This operation should be
	 * assumed very expensive, but non-blocking due to a forking/threaded
	 * implementation on the back end.
	 */
	share_update_index ();

	/* cant really fail :) */
	return TRUE;
}

static int init_transfer (void)
{
	int n;

	/* reads .*.state files in the incomplete directory and attempts to
	 * reconstruct a transfer to resume at the position last known */
	n = download_state_recover ();

	GIFT_TRACE (("recovered %i state transfers", n));

	return TRUE;
}

static int gift_begin (void)
{
	LogOptions logopts = GLOG_NONE;
	char      *logfile = NULL;

	/*
	 * Determine the logging parameters from the command line arguments.
	 */
	if (!cl_quiet)
	{
		logopts |= GLOG_FILE;

		if (cl_verbose)
			logopts |= GLOG_STDERR;

		logfile = (cl_logfile) ? (cl_logfile) : (gift_conf_path ("giftd.log"));
	}

	/*
	 * Finally, switch over to the "true" logging facility.  Please note that
	 * calling log_init twice is guaranteed to be safe by log.c.  See
	 * gift_init for more information on the matter.
	 */
	if (!log_init (logopts, "giftd", 0, 0, logfile))
		return FALSE;

	/*
	 * And god said, let there be giFT!
	 */
	GIFT_INFO (("%s %s (%s %s) started", "giftd", VERSION, __DATE__,
	            __TIME__));

	/*
	 * Initialize the interface protocol handler.  This is used so that giFT
	 * clients may interact with the daemon.  In the very near future we will
	 * also provide a default web interface for modifying more complex
	 * parameters via a standardized HTTP/HTML interface.
	 */
	if (!init_interface ())
		return FALSE;

	/*
	 * Load all dynamic protocols and call the startup routines.  At some
	 * point it would be nice to divide this into two steps so that protocols
	 * may be initialized, queried for information, then the daemon unload
	 * and return.  Yet another TODO for my pile...
	 */
	if (!load_protocols ())
		return FALSE;

	/*
	 * Automatically update (and read) ~/.giFT/shares.  This is done to force
	 * users to keep their share index up to date.
	 */
	if (!init_shares ())
		return FALSE;

	/*
	 * Start up previous downloads (from the state records).  Currently no
	 * other initialization is required to use the transfer subsystem.
	 */
	if (!init_transfer ())
		return FALSE;

	/*
	 * Initialize the optional perl extension interface.  At present, this
	 * interface is extremely crude and not really useful to anyone.  I am
	 * hoping that in the future I can extend this to produce truly
	 * flexible/dynamic functionality for giFT.
	 */
	init_perl ();

	/*
	 * Enter the main event loop and block.  We will stay right here until
	 * the program is asked to terminate itself.
	 */
	event_loop ();

	return TRUE;
}

/*****************************************************************************/

int gift_main (int argc, char **argv)
{
	/*
	 * Parse the command line arguments and assign the cl_* variables to
	 * meaningful values so that we can play with them in gift_init.
	 */
	if (!gift_parse_argv (argc, argv))
	{
		fprintf (stderr, "Fatal error processing command line arguments.");
		return EXIT_FAILURE;
	}

	/*
	 * Begin with initializing all the libgiFT subsystems we will be using,
	 * which is of course all the ones that can be initialized considering
	 * that this is giFT :)
	 *
	 * Do not confuse this with libgift_init, which is a separate helper to
	 * simplify this process when all options are not required.
	 */
	if (!gift_init ())
	{
		/* uh-oh. */
		fprintf (stderr, "Fatal error initializing libgift.");
		return EXIT_FAILURE;
	}

	/*
	 * Handle all parsed command line arguments.  Input errors are considered
	 * fatal inside gift_handle_argv, so we really dont need to check the
	 * return value...
	 */
	if (!gift_handle_argv (argc, argv))
		return EXIT_FAILURE;

	/*
	 * Startup the main giFT system, including the interface protocol
	 * handler, share indexing system, and resume saved download states.  All
	 * configured protocols will be loaded here as well.
	 */
	if (!gift_begin ())
	{
		GIFT_FATAL (("Unable to startup the giFT core, *sigh*"));
		return EXIT_FAILURE;
	}

	/*
	 * Cleanup on isle 4...
	 */
	gift_finish ();

	/*
	 * Consider this a return to main.
	 */
	return EXIT_SUCCESS;
}

/*****************************************************************************/

/*
 * _WINDOWS is defined when compiling as a "native" win32 app, presumably set
 * by MSVC++ but who knows.  We will just wrap the real main underneath this
 * mess.
 */
#ifndef _WINDOWS

int main (int argc, char **argv)
{
	return gift_main (argc, argv);
}

#else /* _WINDOWS */

void __cdecl _setargv (void);

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prev_instance,
                    LPSTR cmd_line, int cmd_show)
{
	/* access __argc and __argv, because of course they arent provided to
	 * this special main */
	_setargv ();
	return gift_main (__argc, __argv);
}

#endif /* !_WINDOWS */
