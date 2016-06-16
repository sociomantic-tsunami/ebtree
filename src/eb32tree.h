/*
 * Elastic Binary Trees - macros and structures for operations on 32bit nodes.
 * Version 6.0
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 */

#ifndef _EB32TREE_H
#define _EB32TREE_H

#include "ebtree.h"
#include <stdint.h>


/* Return the structure of type <type> whose member <member> points to <ptr> */
#define EB32_ROOT	EB_ROOT
#define EB32_TREE_HEAD	EB_TREE_HEAD

/* This structure carries a node, a leaf, and a key. It must start with the
 * eb_node so that it can be cast into an eb_node. We could also have put some
 * sort of transparent union here to reduce the indirection level, but the fact
 * is, the end user is not meant to manipulate internals, so this is pointless.
 */
struct eb32_node {
	struct eb_node node; /* the tree node, must be at the beginning */
	uint32_t key;
};

/*
 * Exported functions and macros.
 * Many of them are always inlined because they are extremely small, and
 * are generally called at most once or twice in a program.
 */

/* Return leftmost node in the tree, or NULL if none */
extern struct eb32_node *eb32_first(struct eb_root *root);

/* Return rightmost node in the tree, or NULL if none */
extern struct eb32_node *eb32_last(struct eb_root *root);

/* Return next node in the tree, or NULL if none */
extern struct eb32_node *eb32_next(struct eb32_node *eb32);

/* Return previous node in the tree, or NULL if none */
extern struct eb32_node *eb32_prev(struct eb32_node *eb32);

/* Return next node in the tree, skipping duplicates, or NULL if none */
extern struct eb32_node *eb32_next_unique(struct eb32_node *eb32);

/* Return previous node in the tree, skipping duplicates, or NULL if none */
extern struct eb32_node *eb32_prev_unique(struct eb32_node *eb32);

/* Delete node from the tree if it was linked in. Mark the node unused. Note
 * that this function relies on a non-inlined generic function: eb_delete.
 */
extern void eb32_delete(struct eb32_node *eb32);

/*
 * The following functions are not inlined by default. They are declared
 * in eb32tree.c, which simply relies on their inline version.
 */
extern struct eb32_node *eb32_lookup(struct eb_root *root, uint32_t x);
extern struct eb32_node *eb32i_lookup(struct eb_root *root, int32_t x);
extern struct eb32_node *eb32_lookup_le(struct eb_root *root, uint32_t x);
extern struct eb32_node *eb32_lookup_ge(struct eb_root *root, uint32_t x);
extern struct eb32_node *eb32_insert(struct eb_root *root, struct eb32_node *new);
extern struct eb32_node *eb32i_insert(struct eb_root *root, struct eb32_node *new);

/*
 * The following functions are less likely to be used directly, because their
 * code is larger. The non-inlined version is preferred.
 */

/* Delete node from the tree if it was linked in. Mark the node unused. */
extern void eb32_delete(struct eb32_node *eb32);
/*
 * Find the first occurence of a key in the tree <root>. If none can be
 * found, return NULL.
 */
extern struct eb32_node *eb32_lookup(struct eb_root *root, uint32_t x);
/*
 * Find the first occurence of a signed key in the tree <root>. If none can
 * be found, return NULL.
 */
extern struct eb32_node *eb32i_lookup(struct eb_root *root, int32_t x);
/* Insert eb32_node <new> into subtree starting at node root <root>.
 * Only new->key needs be set with the key. The eb32_node is returned.
 * If root->b[EB_RGHT]==1, the tree may only contain unique keys.
 */
extern struct eb32_node *eb32_insert(struct eb_root *root, struct eb32_node *new);

/* Insert eb32_node <new> into subtree starting at node root <root>, using
 * signed keys. Only new->key needs be set with the key. The eb32_node
 * is returned. If root->b[EB_RGHT]==1, the tree may only contain unique keys.
 */
extern struct eb32_node *eb32i_insert(struct eb_root *root, struct eb32_node *new);

#endif /* _EB32_TREE_H */
