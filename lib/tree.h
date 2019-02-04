/*
 * $Id: tree.h,v 1.8 2003/10/16 18:50:55 jasta Exp $
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

#ifndef __TREE_H
#define __TREE_H

/*****************************************************************************/

/**
 * @file tree.h
 *
 * @brief Recursive tree data structure.
 */

/*****************************************************************************/

/**
 * Tree node structure.
 */
typedef struct _tree_node
{
	struct _tree_node *parent;         /**< Parent reference */
	struct _tree_node *child;          /**< First child reference */

	struct _tree_node *prev;           /**< Previous sibling */
	struct _tree_node *next;           /**< Next sibling */

	void *data;                        /**< User data */
} TreeNode;

/**
 * Tree structure.
 */
typedef struct
{
	TreeNode *root;                    /**< Root node reference */
} Tree;

/*****************************************************************************/

/**
 * Tree node comparison.
 */
typedef int (*TreeNodeCompare) (void *a, void *b);

/**
 * Tree iterator callback.
 *
 * @param node
 * @param udata User-supplied data
 * @param depth Depth in tree
 */
typedef void (*TreeForeach) (TreeNode *node, void *udata,
                             int depth);

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Insert a new tree node.
 *
 * @param tree
 * @param parent  Insert beneath the supplied parent.
 * @param sibling Insert after the subling sibling.
 * @param data    User-supplied data for insertion.
 *
 * @retval Reference to the inserted node.
 */
LIBGIFT_EXPORT
  TreeNode *tree_insert (Tree **tree, TreeNode *parent, TreeNode *sibling,
                         void *data);

/**
 * Remove a tree node.
 */
LIBGIFT_EXPORT
  void tree_remove (Tree **tree, TreeNode *node);

/**
 * Unallocate a tree object.  Sets the storage pointed to by tree to NULL.
 */
LIBGIFT_EXPORT
  void tree_destroy (Tree **tree);

/**
 * Wrapper around tree_destroy which calls free for all user-supplied data
 * values.
 */
LIBGIFT_EXPORT
  void tree_destroy_free (Tree **tree);

/**
 * Locate a node through iteration.
 */
LIBGIFT_EXPORT
  TreeNode *tree_find (Tree **tree, TreeNode *start, int recurse,
                       TreeNodeCompare func, void *data);

/**
 * Iterate a tree.
 */
LIBGIFT_EXPORT
  void tree_foreach (Tree **tree, TreeNode *start, int depth, int recurse,
                     TreeForeach func, void *data);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __TREE_H */
