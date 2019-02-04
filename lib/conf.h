/*
 * $Id: conf.h,v 1.17 2003/10/31 13:14:23 jasta Exp $
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

#ifndef __CONF_H
#define __CONF_H

/*****************************************************************************/

/**
 * @file conf.h
 *
 * @brief Configuration file routines.
 *
 * Provides routines that can be used to manipulate INI-style configuration
 * files.  These routines properly preserve formatting and comments within
 * the original files.
 */

/*****************************************************************************/

/**
 * Holds the name and all children of a conf file "[header]".
 */
typedef struct
{
	char    *name;                     /**< header name */
	Dataset *keys;                     /**< dataset holding key/value pairs */
} ConfigHeader;

/**
 * Basic config structure, you should avoid manipulating this structure
 * directly if possible.
 */
typedef struct
{
	char         *path;                /**< fully qualified path to .conf */
	FILE         *file;                /**< open file pointer */
	time_t        mtime;               /**< last known modification time of
	                                    *   the file path, used to reread when
	                                    *   on disk changes are detected */
	int           comments;            /**< see ::config_new_ex */
	List         *headers;             /**< list of ConfigHeader's */
	ConfigHeader *confhdr;             /**< header "cursor", do not touch */
} Config;

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Loads a configuration file into memory.
 *
 * @param file  Fully qualified path of the conf file.
 *
 * @return Dynamically allocated Config pointer.
 */
LIBGIFT_EXPORT
  Config *config_new (char *file);

/**
 * Extended constructor.  See ::config_new.
 *
 * @param file
 * @param comments_allowed
 *
 *   If FALSE, comments will not be parsed out of configuration file.
 *   Literal '#' characters will be saved.
 */
LIBGIFT_EXPORT
  Config *config_new_ex (char *file, int comments_allowed);

/**
 * Destroys the memory associated with a Config pointer.
 *
 * @param conf Location of the Config structure.
 */
LIBGIFT_EXPORT
  void config_free (Config *conf);

/**
 * Attempts to synch the possibly formatted configuration file on disk with
 * the data loaded into memory.
 *
 * @param conf Location of the Config structure.
 */
LIBGIFT_EXPORT
  void config_write (Config *conf);

/**
 * Sets a configuration key in memory using a string value.
 *
 * @param conf    Location of the Config structure.
 * @param keypath Key path in the form "header/key".
 * @param value   Value to assign to the key when found.
 */
LIBGIFT_EXPORT
  void config_set_str (Config *conf, char *keypath, char *value);

/**
 * Sets a configuration key in memory using a signed integer value.
 *
 * @see config_set_str
 */
LIBGIFT_EXPORT
  void config_set_int (Config *conf, char *keypath, int value);

/**
 * Gets a configuration key's value from memory as a string.
 *
 * @note When reading data from the configuration structure the key path
 * format may optionally include append "=DEFAULT".  If the key was not found
 * to exist in the structure, "DEFAULT" will be returned as if it did.
 *
 * @param conf    Location of the Config structure.
 * @param keypath Key path in the form "header/key".
 *
 * @return Configuration value.
 *
 * @retval !NULL
 *   pointer to internally used memory, do not write to this pointer or God
 *   will kill an innocent puppy.
 * @retval NULL
 *   failure to lookup the requested key (and no default was provided).
 */
LIBGIFT_EXPORT
  char *config_get_str (Config *conf, char *keypath);

/**
 * Gets a configuration key's value from memory converted to a signed integer.
 *
 * @see config_get_str
 */
LIBGIFT_EXPORT
  int config_get_int (Config *conf, char *keypath);

/*****************************************************************************/

/**
 * Extension specific to giFT that is used to load configuration modules
 * from the ~/.giFT/ directory path.
 *
 * @note Requires ::platform_init before usage.
 *
 * @param module  Module name to load (one of giFT, OpenFT, or ui).
 *
 * @see config_new
 */
LIBGIFT_EXPORT
  Config *gift_config_new (char *module);

/**
 * Completely unrelated extension that merely interprets the supplied
 * printf format string and prepends the platform-specific local dir, which
 * will be $HOME/.giFT by default on UNIX hosts.  For thread-safety, see
 * ::gift_conf_path_r.
 *
 * @note Requires ::platform_init before usage.
 *
 * @param fmt  printf format string.
 * @param ...  c'mon, you know.
 *
 * @return Pointer to the internal static buffer used to build the
 *         configuration path.
 */
LIBGIFT_EXPORT
  char *gift_conf_path (const char *fmt, ...);

/**
 * ::gift_conf_path with thread-safety.  Uses the supplied buffer for storing
 * the resulting path.  On success, the calling argument `buf' will be
 * returned.
 */
LIBGIFT_EXPORT
  char *gift_conf_path_r (char *buf, size_t buf_size, const char *fmt, ...);

/**
 * Return an expanded path from the configuration file with a fallback result
 * from ::gift_conf_path.  Example:
 *
 * \code
 * gift_conf_pathkey (conf, "search/env_path", gift_conf_path ("OpenFT/db"),
 *                    "master.idx");
 * \endcode
 *
 * @param conf
 * @param key
 * @param def
 * @param file
 *
 * @return Pointer to local memory from ::config_get_str or ::stringf.
 */
LIBGIFT_EXPORT
  char *gift_conf_pathkey (Config *conf, char *key, char *def, char *file);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __CONF_H */
