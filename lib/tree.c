/*
 * $Id: tree.c,v 1.8 2003/05/25 23:03:24 jasta Exp $
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

#include "tree.h"

/*****************************************************************************/

static Tree *tree_new ()
{
	Tree *tree;

	if (!(tree = malloc (sizeof (Tree))))
		return NULL;

	memset (tree, 0, sizeof (Tree));

	return tree;
}

static void destroy_siblings (TreeNode *node, int free_udata)
{
	TreeNode *next;

	for (; node; node = next)
	{
		next = node->next;

		if (node->child)
			destroy_siblings (node->child, free_udata);

		if (free_udata)
			free (node->data);

		free (node);
	}
}

static void tree_free (Tree **tree, int free_udata)
{
	if (!tree || !(*tree))
		return;

	destroy_siblings ((*tree)->root, free_udata);
	free (*tree);

	*tree = NULL;
}

void tree_destroy (Tree **tree)
{
	tree_free (tree, FALSE);
}

void tree_destroy_free (Tree **tree)
{
	tree_free (tree, TRUE);
}

/*****************************************************************************/

static TreeNode *tree_node_new (void *data)
{
	TreeNode *node;

	if (!(node = malloc (sizeof (TreeNode))))
		return NULL;

	memset (node, 0, sizeof (TreeNode));

	node->data = data;

	return node;
}

static void tree_node_free (TreeNode *node)
{
	free (node);
}

static TreeNode *sibling_last (TreeNode *node)
{
	if (!node)
		return NULL;

	while (node->next)
		node = node->next;

	return node;
}

/*****************************************************************************/

static void insert_before (TreeNode *sibling, TreeNode *node)
{
	TreeNode *prev;

	node->parent = sibling->parent;

	prev = sibling->prev;

	node->next = sibling;
	node->prev = prev;

	if (prev)
		prev->next = node;

	sibling->prev = node;
}

static void insert_after (TreeNode *sibling, TreeNode *node)
{
	TreeNode *next;

	node->parent = sibling->parent;

	next = sibling->next;

	node->next = next;
	node->prev = sibling;

	if (next)
		next->prev = node;

	sibling->next = node;
}

static void insert_under (TreeNode *parent, TreeNode *node)
{
	TreeNode *last;

	node->parent = parent;

	if (!(last = sibling_last (parent->child)))
	{
		parent->child = node;
		return;
	}

	insert_after (last, node);
}

TreeNode *tree_insert (Tree **tree, TreeNode *parent, TreeNode *sibling,
                       void *data)
{
	TreeNode *node;

	if (!tree || !(node = tree_node_new (data)))
		return NULL;

	if (sibling)
		insert_before (sibling, node);
	else if (parent)
		insert_under (parent, node);
	else
	{
		/* sibling add to the root */
		if (!(*tree))
		{
			if (!((*tree) = tree_new ()))
			{
				tree_node_free (node);
				return NULL;
			}
		}

		if ((*tree)->root)
			insert_after (sibling_last ((*tree)->root), node);
		else
			(*tree)->root = node;
	}

	return node;
}

/*****************************************************************************/

/* TODO */
void tree_remove (Tree **tree, TreeNode *node)
{
}

/*****************************************************************************/

static int default_cmp (char *a, char *b)
{
	if (a != b)
		return -1;

	/* a == b */
	return 0;
}

TreeNode *tree_find (Tree **tree, TreeNode *start,
                     int recurse, TreeNodeCompare func, void *data)
{
	TreeNode *ptr;

	if (!tree || !(*tree))
		return NULL;

	if (!start)
		start = (*tree)->root;

	if (!func)
		func = (TreeNodeCompare) default_cmp;

	/* loop siblings */
	for (ptr = start; ptr; ptr = ptr->next)
	{
		if (((*func) (ptr->data, data)) == 0)
			return ptr;

		if (ptr->child && recurse)
		{
			TreeNode *find;

			if ((find = tree_find (tree, ptr->child, recurse, func, data)))
				return find;
		}
	}

	return NULL;
}

/*****************************************************************************/

void tree_foreach (Tree **tree, TreeNode *start, int depth,
                   int recurse, TreeForeach func, void *data)
{
	TreeNode *ptr;

	if (!tree || !(*tree))
		return;

	if (!start)
		start = (*tree)->root;

	if (!func)
		return;

	/* loop siblings */
	for (ptr = start; ptr; ptr = ptr->next)
	{
		(*func) (ptr, data, depth);

		if (ptr->child && recurse)
			tree_foreach (tree, ptr->child, depth + 1, recurse, func, data);
	}
}

/*****************************************************************************/

#if 0
static void foreach (TreeNode *node, void *data, int depth)
{
	while (depth-- > 0)
		printf (" ");

	printf ("%s\n", (char *) node->data);
}

int main ()
{
	Tree     *tree = NULL;
	TreeNode *root, *two;

	root = tree_insert (&tree, NULL, NULL, strdup ("1"));
	tree_insert (&tree, NULL, NULL, strdup ("2"));
	tree_insert (&tree, root, NULL, strdup ("3"));

	two = tree_find (&tree, NULL, TRUE, (TreeNodeCompare) strcmp, "2");

	tree_insert (&tree, two, NULL, strdup ("4"));
	tree_insert (&tree, two, NULL, strdup ("5"));

	tree_foreach (&tree, NULL, 0, TRUE, (TreeForeach) foreach, NULL);

	tree_destroy_free (&tree);

	return 0;
}
#endif
