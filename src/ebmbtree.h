/*
 * Elastic Binary Trees - macros and structures for Multi-Byte data nodes.
 * Version 6.0.5
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 */

#ifndef _EBMBTREE_H
#define _EBMBTREE_H

#include <string.h>
#include "ebtree.h"

#define EBMB_ROOT	EB_ROOT
#define EBMB_TREE_HEAD	EB_TREE_HEAD

/* This structure carries a node, a leaf, and a key. It must start with the
 * eb_node so that it can be cast into an eb_node. We could also have put some
 * sort of transparent union here to reduce the indirection level, but the fact
 * is, the end user is not meant to manipulate internals, so this is pointless.
 * The 'node.bit' value here works differently from scalar types, as it contains
 * the number of identical bits between the two branches.
 */
struct ebmb_node {
	struct eb_node node; /* the tree node, must be at the beginning */
	unsigned char key[0]; /* the key, its size depends on the application */
};

/*
 * Exported functions and macros.
 * Many of them are always inlined because they are extremely small, and
 * are generally called at most once or twice in a program.
 */

/* Return leftmost node in the tree, or NULL if none */
extern struct ebmb_node *ebmb_first(struct eb_root *root);

/* Return rightmost node in the tree, or NULL if none */
extern struct ebmb_node *ebmb_last(struct eb_root *root);

/* Return next node in the tree, or NULL if none */
extern struct ebmb_node *ebmb_next(struct ebmb_node *ebmb);

/* Return previous node in the tree, or NULL if none */
extern struct ebmb_node *ebmb_prev(struct ebmb_node *ebmb);

/* Return next node in the tree, skipping duplicates, or NULL if none */
extern struct ebmb_node *ebmb_next_unique(struct ebmb_node *ebmb);

/* Return previous node in the tree, skipping duplicates, or NULL if none */
extern struct ebmb_node *ebmb_prev_unique(struct ebmb_node *ebmb);

/* Delete node from the tree if it was linked in. Mark the node unused. Note
 * that this function relies on a non-inlined generic function: eb_delete.
 */
extern void ebmb_delete(struct ebmb_node *ebmb);

/* The following functions are not inlined by default. They are declared
 * in ebmbtree.c, which simply relies on their inline version.
 */
extern struct ebmb_node *ebmb_lookup(struct eb_root *root, const void *x, unsigned int len);
extern struct ebmb_node *ebmb_insert(struct eb_root *root, struct ebmb_node *new, unsigned int len);
extern struct ebmb_node *ebmb_lookup_longest(struct eb_root *root, const void *x);
extern struct ebmb_node *ebmb_lookup_prefix(struct eb_root *root, const void *x, unsigned int pfx);
extern struct ebmb_node *ebmb_insert_prefix(struct eb_root *root, struct ebmb_node *new, unsigned int len);

#endif /* _EBMBTREE_H */

