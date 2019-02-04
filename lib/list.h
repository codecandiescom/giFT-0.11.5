/*
 * $Id: list.h,v 1.15 2003/12/23 03:51:31 jasta Exp $
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

#ifndef __LIST_H
#define __LIST_H

/*****************************************************************************/

/**
 * @file list.h
 *
 * @brief Singly linked list routines
 *
 * Implements a standard doubly linked list.
 */

/*****************************************************************************/

#define list_next(n) ((n) ? (n)->next : NULL)
#define list_prev(n) ((n) ? (n)->prev : NULL)

/*****************************************************************************/

/**
 * Node comparison.
 *
 * @retval >0 \a a is greater than \a b
 * @retval 0  \a a is equal to \a b
 * @retval <0 \a a is less than \a b
 */
typedef int (*CompareFunc) (void *a, void *b);

/**
 * Linked list structure.  This represents all chains in the list.
 */
typedef struct listlink
{
	void         *data;                /**< Data inserted */

	struct listlink *prev;             /**< Previous chain */
	struct listlink *next;             /**< Next chain */
} List;

/**************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Foreach iteration function.  Return value depends on the usage.
 *
 * @param data  List data at this chain
 * @param udata Arbitrary data passed along
 */
typedef int (*ListForeachFunc) (void *data, void *udata);
#define LIST_FOREACH(func) ((ListForeachFunc)func)

/*****************************************************************************/

/**
 * Add a new link to the end of the linked list pointed to by `head'.
 */
LIBGIFT_EXPORT
  List *list_append (List *head, void *udata);

#if 0
/**
 * Inserts the previously allocated link chain at the end of the list pointed
 * to by `head'.  Be sure to use list_unlink_link first to make sure that this
 * link doesn't already belong to another list.
 */
LIBGIFT_EXPORT
  List *list_append_link (List *head, List *link);
#endif

/**
 * Similar to ::list_append, but uses the prepend operation.
 */
LIBGIFT_EXPORT
  List *list_prepend (List *head, void *udata);

#if 0
/**
 * Similar to ::list_append_link, but uses the prepend operation.
 */
LIBGIFT_EXPORT
  List *list_prepend_link (List *head, List *link);
#endif

/*****************************************************************************/

/**
 * Insert a new link immediately preceding the link that currently exists at
 * the `nth' position.
 */
LIBGIFT_EXPORT
  List *list_insert (List *head, int nth, void *udata);

/**
 * Similar to ::list_insert, but determines `nth' based on the result of the
 * comparison function `func'.
 */
LIBGIFT_EXPORT
  List *list_insert_sorted (List *head, CompareFunc func, void *udata);

/*****************************************************************************/

/**
 * Search for and remove the element inserted with the data described by
 * `udata'.
 */
LIBGIFT_EXPORT
  List *list_remove (List *head, void *udata);

/**
 * Similar to ::list_remove, but will not search for the links position.  The
 * memory pointed to by `link' will be freed by this call.
 */
LIBGIFT_EXPORT
  List *list_remove_link (List *head, List *link);

#if 0
/**
 * Similar to ::list_remove_link, but the memory pointed to by `link' will not
 * be freed.
 */
LIBGIFT_EXPORT
  List *list_unlink_link (List *head, List *link);
#endif

/*****************************************************************************/

/**
 * Walk through the list pointed to by `head' freeing each individual link
 * pointer.  This will not affect the data added by the user, which you will
 * need to manage yourself.  Consider ::list_foreach_remove for an alternate
 * interface.
 */
LIBGIFT_EXPORT
  List *list_free (List *head);

/*****************************************************************************/

/**
 * Search for the first list link found with the user data element matching
 * `udata'.
 */
LIBGIFT_EXPORT
  List *list_find (List *head, void *udata);

/**
 * Similar to ::list_find, except that comparison of your search data against
 * the data in the list will be subject to the comparison function `func',
 * instead of the default pointer comparison.
 */
LIBGIFT_EXPORT
  List *list_find_custom (List *head, void *udata, CompareFunc func);

/*****************************************************************************/

/**
 * Iterate through each node in the list described by `head' calling the
 * iterator function `func' for each link chain encountered.
 */
LIBGIFT_EXPORT
  void list_foreach (List *head, ListForeachFunc func, void *udata);

/**
 * Combination of ::list_foreach and ::list_free, allowing the user the
 * chance to manage the memory of each individual link chain and the
 * associated user data.  See ::ListForeachFunc for more information on the
 * various options you have.
 */
LIBGIFT_EXPORT
  List *list_foreach_remove (List *head, ListForeachFunc func, void *udata);

/*****************************************************************************/

/**
 * Access the link chain at the nth position in the list described by `head'.
 */
LIBGIFT_EXPORT
  List *list_nth (List *head, int nth);

/**
 * Similar to ::list_nth, except that the link chains user data element is
 * returned instead of the link chain itself.
 */
LIBGIFT_EXPORT
  void *list_nth_data (List *head, int nth);

/**
 * Iterate through the list described by `head' until the tail is reached, in
 * which case it will be returned to the caller.
 */
LIBGIFT_EXPORT
  List *list_last (List *head);

/*****************************************************************************/

/**
 * Determine the length of the list described by `head' through iteration.
 */
LIBGIFT_EXPORT
  int list_length (List *head);

/**
 * Sort the list described by `head' according to the comparison function
 * `func'.  If NULL is used for the comparison function, the default string
 * comparison will be used via strcmp.
 */
LIBGIFT_EXPORT
  List *list_sort (List *head, CompareFunc func);

/**
 * Allocate a copy of each individual node in the list decsribed by `head'.
 * This will not affect the data added by the user.
 */
LIBGIFT_EXPORT
  List *list_copy (List *head);

/**************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __LIST_H */
