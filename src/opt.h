/*
 * $Id: opt.h,v 1.6 2003/06/04 01:34:23 jasta Exp $
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

#ifndef __OPT_H
#define __OPT_H

/*****************************************************************************/

/**
 * @file opt.h
 *
 * @brief High-level option handling interface that uses getopt and friends
 *        when available.
 */

/*****************************************************************************/

/**
 * Raw structure for specifying the options to be passed to the interface.
 */
typedef struct
{
	enum
	{
		OPT_STR = 0,                   /* char *  */
		OPT_LST,                       /* Array * */
		OPT_LNG,                       /* long    */
		OPT_INT,                       /* int     */
		OPT_CHR,                       /* char    */
		OPT_BOL,                       /* BOOL    */
		OPT_FLG,                       /* BOOL    */
		OPT_CNT,                       /* int     */
		OPT_END
	} type;                            /**< type of option */

	void *data;                        /**< storage location for the value */

	char *longo;                       /**< long option name */
	char  shorto;                      /**< short option name */
	char *envo;                        /**< environment variable name */

	char *usage;                       /**< usage line for this opt */
} giftopt_t;

/*****************************************************************************/

/**
 * Handle the supplied arguments given the options described by \em opts.
 * Note that argc and argv will be modified so that all processed arguments
 * are removed.
 *
 * @return Boolean success or failure.
 */
BOOL opt_parse (int *argc, char ***argv, giftopt_t *opts);

/**
 * Display a usage summary of all options.  This interface is purely optional
 * and should be used in conjunction with a larger help routine.  Use after
 * opt_parse fails.
 */
void opt_usage (giftopt_t *opts);

/**
 * Special interface to opt_usage that will capture and return the text
 * buffer that would normally be sent to stdout.  This is really just a hack
 * so that giFT can optionally use a Windows MessageBox, as the original code
 * did.
 */
char *opt_usage_capt (giftopt_t *opts);

/*****************************************************************************/

#endif /* __OPT_H */
