/*
 * $Id: interface.h,v 1.16 2003/10/16 18:50:54 jasta Exp $
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

#ifndef __INTERFACE_H
#define __INTERFACE_H

/*****************************************************************************/

/**
 * @file interface.h
 *
 * @brief Low-level interface protocol manipulation routines.
 */

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Parsed high-level structure representing an unserialized interface
 * protocol packet.
 */
typedef struct
{
	/**
	 * @name Public
	 */
	char *command;                     /**< Command name (SEARCH, ITEM ...) */
	char *value;                       /**< Command value (in most cases this
	                                    *   is actually the id, if present) */

	/**
	 * @name Private
	 */
	Tree *tree;                        /**< Unserialized representation of
										*   the original packet */
} Interface;

/**
 * A single key/value association with a corresponding insertion in the main
 * Interface tree structure.
 */
typedef struct
{
	TreeNode *node;                    /**< Link to the node that holds this
										*   bit of interface data */
	char     *key;                     /**< "Real" key name, may include some
										*   trailing values encased in [] for
										*   uniqueness */
	char     *keydisp;                 /**< Displayed key name */
	char     *value;                   /**< Data value */
} InterfaceNode;

/**
 * Foreach callback for the Interface structure.
 *
 * @param p     The Interface instance that requested this foreach action
 * @param inode Currently selected node element
 * @param udata User data supplied from ::interface_foreach.
 */
typedef void (*InterfaceForeach) (Interface *p, InterfaceNode *inode,
                                  void *udata);

/*****************************************************************************/

/**
 * Create a new instance intended for manual construction (generally this is
 * used for data delivery, instead of parsing or handling).
 *
 * @param command
 * @param value
 */
LIBGIFT_EXPORT
  Interface *interface_new (char *command, char *value);

/**
 * Destroy a previously created instance.
 */
LIBGIFT_EXPORT
  void interface_free (Interface *p);

/**
 * Basic write accessor for the interface command.
 */
LIBGIFT_EXPORT
  void interface_set_command (Interface *p, char *command);

/**
 * Basic write accessor for the interface command's value.
 */
LIBGIFT_EXPORT
  void interface_set_value (Interface *p, char *value);

/**
 * Retrieve an interface element's value given a fully qualified key path.
 * For example, if you wanted to get the Artist meta data for a given ITEM
 * result (once parsed), you would use:
 *
 * \code
 * printf ("ARTIST=%s\n", interface_get (p, "META/artist"));
 * \endcode
 *
 * @param p
 * @param key  Key path that references the desired interface element.  Not
 *             case sensitive.
 *
 * @return The value as a string if found, otherwise NULL.  Note that any
 *         key which has an empty value set will actually return a blank
 *         string (string literal "") instead of NULL.
 */
LIBGIFT_EXPORT
  char *interface_get (Interface *p, char *key);
#define INTERFACE_GETLU(p,k) ((unsigned long)ATOUL(interface_get(p,k)))
#define INTERFACE_GETLI(p,k) ((signed long)ATOUL(interface_get(p,k)))
#define INTERFACE_GETL(p,k) INTERFACE_GETLI(p,k)
#define INTERFACE_GETI(p,k) ((int)(ATOI(interface_get(p,k))))
#define INTERFACE_GETU(p,k) ((unsigned int)(ATOI(interface_get(p,k))))

/**
 * Add an interface element (key and value set) at the specified key path.
 * See ::interface_get for more information on key paths.
 *
 * @param p
 * @param key    Key path.
 * @param value  Key data.
 *
 * @return Boolean success or failure.  This function will return TRUE even
 *         if the inserted data already exists.
 */
LIBGIFT_EXPORT
  BOOL interface_put (Interface *p, char *key, char *value);
#define INTERFACE_PUTLU(p,k,v) interface_put(p, k, stringf ("%lu", (unsigned long)v))
#define INTERFACE_PUTLI(p,k,v) interface_put(p, k, stringf ("%li", (long)v))
#define INTERFACE_PUTL(p,k,v) INTERFACE_PUTLI(p,k,v)
#define INTERFACE_PUTI(p,k,v) interface_put(p, k, stringf ("%i", (int)v))
#define INTERFACE_PUTU(p,k,v) interface_put(p, k, stringf ("%u", (unsigned int)v))

/**
 * Loop through all interface elements that exists non-recursively at the key
 * path specified by \em key.
 *
 * @param p
 * @param key    Key path to begin searching from.  A NULL value here will
 *               effectively loop through all base elements.
 * @param func   Callback routine to be used for each element.
 * @param udata  Arbitrary data to be given to each call of \em func.
 */
LIBGIFT_EXPORT
  void interface_foreach (Interface *p, char *key, InterfaceForeach func,
                          void *udata);

/**
 * Loop through all interface elements given an interface node as a starting
 * point.  This is useful when it is necessary to manually implement loop
 * recursion with interface_foreach, ie, you are trying to parse all SOURCE
 * elements for a given transfer.  See ::interface_foreach for more details.
 *
 * @param p
 * @param node   Node given by a previous InterfaceForeach callback.
 * @param func
 * @param udata
 */
LIBGIFT_EXPORT
  void interface_foreach_ex (Interface *p, InterfaceNode *node,
                             InterfaceForeach func, void *udata);

/**
 * Flatten a fully constructed Interface instance into a simple String
 * object.  This is used prior to network delivery of the data represented by
 * \em p.
 *
 * @param p
 *
 * @return String object containing the serialized data.
 */
LIBGIFT_EXPORT
  String *interface_serialize (Interface *p);

/**
 * Parse a serialized interface object back into the high-level structure
 * required for the API defined here to work.  This is a required step for
 * actually handling the data received over a socket.
 *
 * @param data  Optionally NUL-terminated string similar to the return value
 *              of interface_serialize.  Likely data received from some
 *              lower level network interface (eg, a socket).
 * @param len   Length of data.
 *
 * @return Interface object pointer if successful, otherwise NULL.
 */
LIBGIFT_EXPORT
  Interface *interface_unserialize (char *data, size_t len);

/**
 * Simple abstract wrapper for delivering all the contents represented by an
 * Interface instance to an established socket connection.  Basically just
 * serializes the data and schedules for delivery.
 *
 * @param p
 * @param c
 *
 * @return Total number of bytes delivered (or scheduled for delivery) on
 *         success, -1 on failure.
 */
LIBGIFT_EXPORT
  int interface_send (Interface *p, TCPC *c);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __INTERFACE_H */
