/*
 * $Id: list_lock.c,v 1.7 2003/05/25 23:03:23 jasta Exp $
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

#include "libgift.h"
#include "list.h"

#include "list_lock.h"

/*****************************************************************************/

ListLock *list_lock_new ()
{
	ListLock *lock;

	if (!(lock = malloc (sizeof (ListLock))))
		return NULL;

	memset (lock, 0, sizeof (ListLock));

	return lock;
}

void list_lock_free (ListLock *lock)
{
	if (!lock)
		return;

	list_free (lock->list);
	free (lock);
}

/*****************************************************************************/

void list_lock (ListLock *lock)
{
	if (!lock)
		return;

	lock->locked++;
}

static int locking_append (void *data, ListLock *lock)
{
	lock->list = list_append (lock->list, data);
	return TRUE;
}

static int locking_prepend (void *data, ListLock *lock)
{
	lock->list = list_prepend (lock->list, data);
	return TRUE;
}

static int locking_remove (void *data, ListLock *lock)
{
	lock->list = list_remove (lock->list, data);
	return TRUE;
}

static int locking_insert_sorted (void *data, ListLock *lock)
{
	lock->list = list_insert_sorted (lock->list, NULL, data);
	return TRUE;
}

static void lock_apply (ListLock *lock, List **list, void *func)
{
	*list = list_foreach_remove (*list, (ListForeachFunc) func, lock);
}

void list_unlock (ListLock *lock)
{
	if (!lock)
		return;

	if (lock->locked > 0)
		lock->locked--;

	/* still locked */
	if (lock->locked > 0)
		return;

	lock_apply (lock, &lock->lock_append,        locking_append);
	lock_apply (lock, &lock->lock_prepend,       locking_prepend);
	lock_apply (lock, &lock->lock_remove,        locking_remove);
	lock_apply (lock, &lock->lock_insert_sorted, locking_insert_sorted);
}

/*****************************************************************************/

void list_lock_append (ListLock *lock, void *data)
{
	assert (lock != NULL);

	if (lock->locked)
	{
		lock->lock_append = list_append (lock->lock_append, data);
		return;
	}

	lock->list = list_append (lock->list, data);
}

void list_lock_prepend (ListLock *lock, void *data)
{
	assert (lock != NULL);

	if (lock->locked)
	{
		lock->lock_prepend = list_prepend (lock->lock_prepend, data);
		return;
	}

	lock->list = list_prepend (lock->list, data);
}

/*****************************************************************************/

void list_lock_remove (ListLock *lock, void *data)
{
	if (lock->locked)
	{
		lock->lock_remove = list_prepend (lock->lock_remove, data);
		return;
	}

	lock->list = list_remove (lock->list, data);
}

/*****************************************************************************/

void list_lock_insert_sorted (ListLock *lock, CompareFunc func, void *data)
{
	if (lock->locked)
	{
		/* TODO: this is obviously not right ... this whole fucking module
		 * sucks anyway */
		list_lock_prepend (lock, data);
		return;
	}

	lock->list = list_insert_sorted (lock->list, func, data);
}

/*****************************************************************************/

#if 0
static ListLock *x1 = NULL;
static ListLock *x2 = NULL;

static void my_foreach (ListLock *lock, ListForeachFunc func)
{
	List *ptr;

	printf ("my_foreach (%p)\n{\n", lock);

	list_lock (lock);

	for (ptr = lock->list; ptr; ptr = list_next (ptr))
	{
		printf ("\tmy_foreach: %p: %p: '%s'\n", lock, ptr, (char *) ptr->data);
		(*func) (lock, ptr);
	}

	list_unlock (lock);

	printf ("}\n");
}


static int x1_print (ListLock *lock, List *node)
{
	printf ("x1_print: %p: '%s'\n", lock, (char *) node->data);
	return 0;
}

static int x1_move (ListLock *lock, List *node)
{
	if (node->data == "x1insert3")
	{
		printf ("moving '%s'\n", (char *) node->data);
		list_lock_remove (lock, node->data);
		list_lock_append (x2, node->data);
	}

	return 0;
}

static int x1_fuckup (ListLock *lock, List *node)
{
	printf ("removing '%s'\n", (char *) node->data);
	list_lock_remove (lock, node->data);

	printf ("attempting print...\n");
	my_foreach (lock, (ListForeachFunc) x1_print);

	printf ("attempting move...\n");
	my_foreach (lock, (ListForeachFunc) x1_move);

	return 0;
}

int main ()
{
	x1 = list_lock_new ();
	x2 = list_lock_new ();

	list_lock_append (x1, "x1insert1");
	list_lock_append (x1, "x1insert2");
	list_lock_append (x1, "x1insert3");

	list_lock_append (x2, "x2insert1");
	list_lock_append (x2, "x2insert2");
	list_lock_append (x2, "x2insert3");

	my_foreach (x1, (ListForeachFunc) x1_fuckup);
	my_foreach (x1, (ListForeachFunc) x1_print);

	list_lock_free (x1);
	list_lock_free (x2);

	return 0;
}
#endif
