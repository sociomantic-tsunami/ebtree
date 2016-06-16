/*
 * Elastic Binary Trees - macros and structures for operations on pointer nodes.
 * Version 6.0
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 */

#include "ebpttree.h"
#include "ebtree.h"
#include "eb32tree.h"
#include "eb64tree.h"


/* Return the structure of type <type> whose member <member> points to <ptr> */
#define ebpt_entry(ptr, type, member) container_of(ptr, type, member)

/*
 * Exported functions and macros.
 * Many of them are always inlined because they are extremely small, and
 * are generally called at most once or twice in a program.
 */

/* Return leftmost node in the tree, or NULL if none */
struct ebpt_node *ebpt_first(struct eb_root *root)
{
	return ebpt_entry(eb_first(root), struct ebpt_node, node);
}

/* Return rightmost node in the tree, or NULL if none */
struct ebpt_node *ebpt_last(struct eb_root *root)
{
	return ebpt_entry(eb_last(root), struct ebpt_node, node);
}

/* Return next node in the tree, or NULL if none */
struct ebpt_node *ebpt_next(struct ebpt_node *ebpt)
{
	return ebpt_entry(eb_next(&ebpt->node), struct ebpt_node, node);
}

/* Return previous node in the tree, or NULL if none */
struct ebpt_node *ebpt_prev(struct ebpt_node *ebpt)
{
	return ebpt_entry(eb_prev(&ebpt->node), struct ebpt_node, node);
}

/* Return next node in the tree, skipping duplicates, or NULL if none */
struct ebpt_node *ebpt_next_unique(struct ebpt_node *ebpt)
{
	return ebpt_entry(eb_next_unique(&ebpt->node), struct ebpt_node, node);
}

/* Return previous node in the tree, skipping duplicates, or NULL if none */
struct ebpt_node *ebpt_prev_unique(struct ebpt_node *ebpt)
{
	return ebpt_entry(eb_prev_unique(&ebpt->node), struct ebpt_node, node);
}

/* Delete node from the tree if it was linked in. Mark the node unused. Note
 * that this function relies on a non-inlined generic function: eb_delete.
 */
void ebpt_delete(struct ebpt_node *ebpt)
{
	eb_delete(&ebpt->node);
}

/*
 * The following functions are inlined but derived from the integer versions.
 */
struct ebpt_node *ebpt_lookup(struct eb_root *root, void *x)
{
	if (sizeof(void *) == 4)
		return (struct ebpt_node *)eb32_lookup(root, (uint32_t)(PTR_INT_TYPE)x);
	else
		return (struct ebpt_node *)eb64_lookup(root, (uint64_t)(PTR_INT_TYPE)x);
}

struct ebpt_node *ebpt_lookup_le(struct eb_root *root, void *x)
{
	if (sizeof(void *) == 4)
		return (struct ebpt_node *)eb32_lookup_le(root, (uint32_t)(PTR_INT_TYPE)x);
	else
		return (struct ebpt_node *)eb64_lookup_le(root, (uint64_t)(PTR_INT_TYPE)x);
}

struct ebpt_node *ebpt_lookup_ge(struct eb_root *root, void *x)
{
	if (sizeof(void *) == 4)
		return (struct ebpt_node *)eb32_lookup_ge(root, (uint32_t)(PTR_INT_TYPE)x);
	else
		return (struct ebpt_node *)eb64_lookup_ge(root, (uint64_t)(PTR_INT_TYPE)x);
}

struct ebpt_node *ebpt_insert(struct eb_root *root, struct ebpt_node *new)
{
	if (sizeof(void *) == 4)
		return (struct ebpt_node *)eb32_insert(root, (struct eb32_node *)new);
	else
		return (struct ebpt_node *)eb64_insert(root, (struct eb64_node *)new);
}
