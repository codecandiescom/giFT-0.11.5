/*
 * $Id: opt.c,v 1.20 2003/09/28 03:11:02 jasta Exp $
 *
 * Generalized getopt and getopt_long abstraction.
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

#include "lib/array.h"

#include "opt.h"

#ifdef HAVE_GETOPT_H
# include "getopt.h"
#endif /* HAVE_GETOPT_H */

/*****************************************************************************/

/* ensure that USE_GETOPT is defined for USE_GETOPT_LONG */
#if defined(USE_GETOPT_LONG) && !defined(USE_GETOPT)
# define USE_GETOPT 1
#endif

/*****************************************************************************/

static const char *opttypes[] =
{
	"(str)   ",
	"(list)  ",
	"(long)  ",
	"(int)   ",
	"(char)  ",
	"(bool)  ",
	"(flag)  ",
	"(count) ",
	NULL
};

/*****************************************************************************/

#ifdef USE_GETOPT
static char *build_optstr (giftopt_t *opts)
{
	String *str;

	if (!(str = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	for (; opts->type != OPT_END; opts++)
	{
		BOOL add_colon;

		if (opts->shorto == 0)
			continue;

		string_appendc (str, opts->shorto);

		switch (opts->type)
		{
		 case OPT_STR:
		 case OPT_LST:
		 case OPT_LNG:
		 case OPT_INT:
		 case OPT_CHR:
		 case OPT_BOL:
			add_colon = TRUE;
			break;
		 default:
			add_colon = FALSE;
			break;
		}

		if (add_colon)
			string_appendc (str, ':');
	}

	return string_free_keep (str);
}
#endif /* USE_GETOPT */

static int opt_needarg (giftopt_t *opt)
{
	int needval;

	switch (opt->type)
	{
	 case OPT_STR:
	 case OPT_LST:
	 case OPT_CHR:
	 case OPT_LNG:
	 case OPT_INT:
	 case OPT_BOL:
		needval = 1;
		break;
	 default:
		needval = 0;
		break;
	}

	return needval;
}

#ifdef USE_GETOPT_LONG
static struct option *build_longopts (giftopt_t *opts)
{
	giftopt_t     *opt;
	struct option *longopt, *longopts;
	size_t         len = 0;

	/* iterate for the length */
	for (opt = opts; opt->type != OPT_END; opt++)
		len++;

	if (!(longopts = CALLOC ((len + 1), sizeof (struct option))))
		return NULL;

	/* build the long opts array */
	for (opt = opts, longopt = longopts; opt->type != OPT_END; opt++, longopt++)
	{
		longopt->name    = opt->longo;
		longopt->has_arg = opt_needarg (opt);
		longopt->flag    = NULL;
		longopt->val     = (int)opt->shorto;
	}

	return longopts;
}
#endif /* USE_GETOPT_LONG */

static giftopt_t *find_opt (giftopt_t *opts, int c, int longidx)
{
	/* no need to search for it, getopt_long provided */
	if (longidx >= 0)
		return (opts + longidx);

	/* we need to search for a suitable short opt */
	for (; opts->type != OPT_END; opts++)
	{
		if (opts->shorto == c)
			return opts;
	}

	return NULL;
}

static void set_opt_data (giftopt_t *opt, char *data)
{
	/* TODO: make sure that getopt guarantees safe usage of optarg here
	 * TODO: make sure none of this data could be unaligned */
	switch (opt->type)
	{
	 case OPT_STR:  (*((char **)opt->data)) = data;          break;
	 case OPT_LST:  array_push ((Array **)opt->data, data);  break;
	 case OPT_LNG:  (*((long *)opt->data))  = atol (data);   break;
	 case OPT_INT:  (*((int *)opt->data))   = atoi (data);   break;
	 case OPT_CHR:  (*((char *)opt->data))  = data[0];       break;
	 case OPT_BOL:  /* TODO */                               break;
	 case OPT_FLG:  (*((BOOL *)opt->data))  = 1;             break;
	 case OPT_CNT:  (*((int *)opt->data))++;                 break;
	 case OPT_END:  abort ();                                break;
	}
}

static int handle_opt (giftopt_t *opts, int c, int longidx)
{
	giftopt_t *opt;

	if (!(opt = find_opt (opts, c, longidx)))
		return FALSE;

	set_opt_data (opt, optarg);

	return TRUE;
}

static void handle_env_opt (giftopt_t *opts)
{
	for (; opts->type != OPT_END; opts++)
	{
		char *envdata;

		if (!opts->envo)
			continue;

		if ((envdata = getenv (opts->envo)))
			set_opt_data (opts, envdata);
	}
}

BOOL opt_parse (int *argc, char ***argv, giftopt_t *opts)
{
	BOOL           ret = TRUE;
#ifdef USE_GETOPT
	char          *optstr;
# ifdef USE_GETOPT_LONG
	struct option *longopts;
# endif /* USE_GETOPT_LONG */
#endif /* USE_GETOPT */

	handle_env_opt (opts);

	if (!(optstr = build_optstr (opts)))
		return FALSE;

#ifdef USE_GETOPT_LONG
	if (!(longopts = build_longopts (opts)))
	{
		free (optstr);
		return FALSE;
	}
#endif /* USE_GETOPT_LONG */

	while (1)
	{
		int c       = -1;
		int longidx = -1;

#if defined(USE_GETOPT_LONG)
		c = getopt_long (*argc, *argv, optstr, longopts, &longidx);
#elif defined(USE_GETOPT)
		c = getopt (*argc, *argv, optstr);
#endif

		if (c == -1)
			break;

		if (!(ret = handle_opt (opts, c, longidx)))
			break;
	}

#ifdef USE_GETOPT
	free (optstr);
# ifdef USE_GETOPT_LONG
	free (longopts);
# endif /* USE_GETOPT_LONG */
#endif /* USE_GETOPT */

#if 0
	/* adjust argc and argv */
	*argc = *argc - optind;
	*argv += optind;
#endif

	return ret;
}

static void opt_usage1 (String *s, giftopt_t *opt)
{
	static char usagebuf[33];
	String     *usage;
	int         needval = FALSE;

	if (!(usage = string_new (usagebuf, sizeof (usagebuf), 0, FALSE)))
		return;

	/* hmm, how are we supposed to show the user how to use an opt that isn't
	 * usable? */
	if (!opt->longo && !opt->shorto)
		return;

	needval = opt_needarg (opt);

	if (opt->shorto)
	{
		string_appendf (usage, " -%c", opt->shorto);

		if (needval)
			string_append (usage, "<VAL>");
		else
			string_append (usage, "     ");
	}
	else
	{
		/* pad so that the long opts dont look funky */
		string_append (usage, "        ");
	}

#ifdef USE_GETOPT_LONG
	if (opt->longo)
	{
		string_appendf (usage, " --%s", opt->longo);

		if (needval)
			string_append (usage, "=<VAL>");
	}
#endif /* USE_GETOPT_LONG */

	string_appendf (s, "%-27s %s %s\n", usage->str, opttypes[opt->type],
	                STRING_NOTNULL(opt->usage));

	string_free (usage);
}

char *opt_usage_capt (giftopt_t *opts)
{
	String *s;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	for (; opts->type != OPT_END; opts++)
		opt_usage1 (s, opts);

	return string_free_keep (s);
}

void opt_usage (giftopt_t *opts)
{
	char *s;

	if (!(s = opt_usage_capt (opts)))
		return;

	printf ("%s", s);
	free (s);
}

/*****************************************************************************/

#if 0
static int   help = 0;
static char *test = NULL;
static int   verb = 0;

static giftopt_t opts[] =
{
	{ OPT_FLG, &help, "help",    'h', NULL, "Show this help message" },
	{ OPT_STR, &test, "test",    't', NULL, "Sample string option" },
	{ OPT_CNT, &verb, "verbose", 'v', NULL, "Increase verbosity level" },
	{ OPT_END },
};

int main (int argc, char **argv)
{
	if (!opt_parse (&argc, &argv, opts) || help)
	{
		opt_usage (opts);
		return 1;
	}

	printf ("help = %d\n", help);
	printf ("test = %s\n", test);
	printf ("verb = %d\n", verb);

	return 0;
}
#endif
