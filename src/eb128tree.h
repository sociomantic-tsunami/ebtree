/*
 * Elastic Binary Trees - macros and structures for operations on 128bit nodes.
 *
 * Extension to the HAProxy Elastic Binary Trees library.
 *
 * HAProxy Elastic Binary Trees library:
 *
 * Version 6.0
 *
 * 128-bit key extension
 *
 * This extension to the Elastic Binary Trees library uses a 128-bit integer
 * type for the node keys, which is not a part of the standard C language but
 * provided as an extension by GCC 4.6 and later for targets that support it.
 * These targets include x86-64 but not x86.
 *
 * @see http://gcc.gnu.org/onlinedocs/gcc-4.6.2/gcc/_005f_005fint128.html
 * @see http://gcc.gnu.org/gcc-4.6/changes.html
 *
 * If this extension is not enabled, the functions defined below will be missing
 * in the produced binary.
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Copyright (C) 2009-2016 Sociomantic Labs GmbH. All rights reserved
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 */

#include "int128.h"

#ifdef INT128_SUPPORTED

#ifndef _EB128TREE_H
#define _EB128TREE_H

#include "ebtree.h"

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

#define EB128_ROOT	EB_ROOT
#define EB128_TREE_HEAD	EB_TREE_HEAD

/* This structure carries a node, a leaf, and a key. It must start with the
 * eb_node so that it can be cast into an eb_node. We could also have put some
 * sort of transparent union here to reduce the indirection level, but the fact
 * is, the end user is not meant to manipulate internals, so this is pointless.
 */
struct eb128_node {
	struct eb_node node; /* the tree node, must be at the beginning */
	uint128_t key;
};

/*
 * Exported functions and macros.
 * Many of them are always inlined because they are extremely small, and
 * are generally called at most once or twice in a program.
 */

/* Return leftmost node in the tree, or NULL if none */
extern struct eb128_node *eb128_first(struct eb_root *root);

/* Return rightmost node in the tree, or NULL if none */
extern struct eb128_node *eb128_last(struct eb_root *root);

/* Return next node in the tree, or NULL if none */
extern struct eb128_node *eb128_next(struct eb128_node *eb128);

/* Return previous node in the tree, or NULL if none */
extern struct eb128_node *eb128_prev(struct eb128_node *eb128);

/* Return next node in the tree, skipping duplicates, or NULL if none */
extern struct eb128_node *eb128_next_unique(struct eb128_node *eb128);

/* Return previous node in the tree, skipping duplicates, or NULL if none */
extern struct eb128_node *eb128_prev_unique(struct eb128_node *eb128);

/* Delete node from the tree if it was linked in. Mark the node unused. Note
/*
 * The following functions are less likely to be used directly, because their
 * code is larger. The non-inlined version is preferred.
 */

/* Delete node from the tree if it was linked in. Mark the node unused. */
extern void eb128_delete(struct eb128_node *eb128);

/*
 * Find the first occurence of a key in the tree <root>. If none can be
 * found, return NULL.
 */
extern struct eb128_node *eb128_lookup(struct eb_root *root, uint128_t x);
extern struct eb128_node *eb128i_lookup(struct eb_root *root, int128_t x);
extern struct eb128_node *eb128_lookup_264(struct eb_root *root, uint64_t lo, uint64_t hi);
extern struct eb128_node *eb128i_lookup_264(struct eb_root *root, uint64_t lo, int64_t hi);

extern struct eb128_node *eb128_lookup_le(struct eb_root *root, uint128_t x);
extern struct eb128_node *eb128_lookup_le_264(struct eb_root *root, uint64_t lo, uint64_t hi);

extern struct eb128_node *eb128_lookup_ge(struct eb_root *root, uint128_t x);
extern struct eb128_node *eb128_lookup_ge_264(struct eb_root *root, uint64_t lo, uint64_t hi);

/* Insert eb128_node <new> into subtree starting at node root <root>.
 * Only new->key needs be set with the key. The eb128_node is returned.
 * If root->b[EB_RGHT]==1, the tree may only contain unique keys.
 */
extern struct eb128_node *eb128_insert(struct eb_root *root, struct eb128_node *new);

/* Insert eb128_node <new> into subtree starting at node root <root>, using
 * signed keys. Only new->key needs be set with the key. The eb128_node
 * is returned. If root->b[EB_RGHT]==1, the tree may only contain unique keys.
 */
extern struct eb128_node *eb128i_insert(struct eb_root *root, struct eb128_node *new);

/******************************************************************************

	Tells whether a is less than b. a and b are uint128_t values composed from
	alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a < b or false otherwise.

 ******************************************************************************/

extern bool eb128_less_264(uint64_t alo, uint64_t ahi, uint64_t blo, uint64_t bhi);

/******************************************************************************

	Tells whether a is less than or equal to b. a and b are uint128_t values
	composed from alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a <= b or false otherwise.

 ******************************************************************************/

extern bool eb128_less_or_equal_264(uint64_t alo, uint64_t ahi, uint64_t blo, uint64_t bhi);

/******************************************************************************

	Tells whether a is equal to b. a and b are uint128_t values
	composed from alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a == b or false otherwise.

 ******************************************************************************/

extern bool eb128_equal_264(uint64_t alo, uint64_t ahi, uint64_t blo, uint64_t bhi);

/******************************************************************************

	Tells whether a is greater than or equal to b. a and b are uint128_t values
	composed from alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a >= b or false otherwise.

 ******************************************************************************/

extern bool eb128_greater_or_equal_264(uint64_t alo, uint64_t ahi, uint64_t blo, const uint64_t bhi);

/******************************************************************************

	Tells whether a is greater than b. a and b are uint128_t values
	composed from alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a <= b or false otherwise.

 ******************************************************************************/

extern bool eb128_greater_264(uint64_t alo, uint64_t ahi, uint64_t blo, uint64_t bhi);

/******************************************************************************

	Compares a and b in a qsort callback/D opCmp fashion. a and b are uint128_t
	values composed from alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		a value less than 0 if a < b,
		a value greater than 0 if a > b
		or 0 if a == b.

 ******************************************************************************/

extern int eb128_cmp_264(uint64_t alo, uint64_t ahi, uint64_t blo, uint64_t bhi);

/******************************************************************************

	Tells whether a is less than b. a and b are int128_t values composed from
	alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a < b or false otherwise.

 ******************************************************************************/

extern bool eb128i_less_264(uint64_t alo, int64_t ahi, uint64_t blo, int64_t bhi);

/******************************************************************************

	Tells whether a is less than or equal to b. a and b are int128_t values
	composed from alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a <= b or false otherwise.

 ******************************************************************************/

extern bool eb128i_less_or_equal_264(uint64_t alo, int64_t ahi,	uint64_t blo, int64_t bhi);

/******************************************************************************

	Tells whether a is equal to b. a and b are int128_t values composed from
	alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a == b or false otherwise.

 ******************************************************************************/

extern bool eb128i_equal_264(uint64_t alo, int64_t ahi, uint64_t blo, int64_t bhi);

/******************************************************************************

	Tells whether a is greater or equal to than b. a and b are int128_t values
	composed from alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a >= b or false otherwise.

 ******************************************************************************/

extern bool eb128i_greater_or_equal_264(uint64_t alo, int64_t ahi, uint64_t blo, int64_t bhi);

/******************************************************************************

	Tells whether a is greater than b. a and b are int128_t values composed from
	alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		true if a > b or false otherwise.

 ******************************************************************************/

extern bool eb128i_greater_264(uint64_t alo, int64_t ahi, uint64_t blo, int64_t bhi);

/******************************************************************************

	Compares a and b in a qsort callback/D opCmp fashion. a and b are int128_t
	values composed from alo and ahi or blo and bhi, respectively.

	Params:
		alo = value of the lower 64 bits of a
		ahi = value of the higher 64 bits of a
		blo = value of the lower 64 bits of b
		ahi = value of the higher 64 bits of b

	Returns:
		a value less than 0 if a < b,
		a value greater than 0 if a > b
		or 0 if a == b.

 ******************************************************************************/

extern int eb128i_cmp_264(uint64_t alo, int64_t ahi, uint64_t blo, int64_t bhi);

/******************************************************************************

	Sets node->key to an uint128_t value composed from lo and hi.

	Params:
		node = node to set the key
		lo   = value of the lower 64 value bits of node->key
		hi   = value of the higher 64 value bits of node->key

	Returns:
		node

 ******************************************************************************/

extern struct eb128_node *eb128_node_setkey_264(struct eb128_node *node, uint64_t lo, uint64_t hi);

/******************************************************************************

	Sets node->key to an int128_t value composed from lo and hi.

	Params:
		node = node to set the key
		lo   = value of the lower 64 value bits of node->key
		hi   = value of the higher 64 value bits of node->key

	Returns:
		node

 ******************************************************************************/

extern struct eb128_node *eb128i_node_setkey_264(struct eb128_node *node, uint64_t lo, int64_t hi);

/******************************************************************************

	Obtains node->key,and decomposes it into two uint64_t values. This assumes
	that the key was originally unsigned, e.g. set by eb128_node_setkey_264().

	Params:
		node = node to obtain the key
		lo   = output of the value of the lower 64 value bits of node->key
		hi   = output of the value of the higher 64 value bits of node->key

 ******************************************************************************/

extern void eb128_node_getkey_264(const struct eb128_node *node, uint64_t *restrict lo, uint64_t *restrict hi);

/******************************************************************************

	Obtains node->key,and decomposes it into an int64_t and an uint64_t value.
	This assumes that the key was originally signed, e.g. set by
	eb128i_node_setkey_264().

	Params:
		node = node to obtain the key
		lo   = output of the value of the lower 64 value bits of node->key
		hi   = output of the value of the higher 64 value bits of node->key

 ******************************************************************************/

extern void eb128i_node_getkey_264(const struct eb128_node *node, uint64_t *lo, int64_t *hi);

#endif /* _EB128_TREE_H */
#endif /* INT128_SUPPORTED */

