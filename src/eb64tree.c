/*
 * Elastic Binary Trees - macros and structures for operations on 64bit nodes.
 * Version 6.0
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 */

#include "eb64tree.h"
#include "ebtree.h"
#include <stdint.h>
#include <limits.h>

/* Return the structure of type <type> whose member <member> points to <ptr> */
#define eb64_entry(ptr, type, member) container_of(ptr, type, member)

/*
 * Exported functions and macros.
 * Many of them are always inlined because they are extremely small, and
 * are generally called at most once or twice in a program.
 */

/* Return leftmost node in the tree, or NULL if none */
inline struct eb64_node *eb64_first(struct eb_root *root)
{
	return eb64_entry(eb_first(root), struct eb64_node, node);
}

/* Return rightmost node in the tree, or NULL if none */
inline struct eb64_node *eb64_last(struct eb_root *root)
{
	return eb64_entry(eb_last(root), struct eb64_node, node);
}

/* Return next node in the tree, or NULL if none */
inline struct eb64_node *eb64_next(struct eb64_node *eb64)
{
	return eb64_entry(eb_next(&eb64->node), struct eb64_node, node);
}

/* Return previous node in the tree, or NULL if none */
inline struct eb64_node *eb64_prev(struct eb64_node *eb64)
{
	return eb64_entry(eb_prev(&eb64->node), struct eb64_node, node);
}

/* Return next node in the tree, skipping duplicates, or NULL if none */
inline struct eb64_node *eb64_next_unique(struct eb64_node *eb64)
{
	return eb64_entry(eb_next_unique(&eb64->node), struct eb64_node, node);
}

/* Return previous node in the tree, skipping duplicates, or NULL if none */
inline struct eb64_node *eb64_prev_unique(struct eb64_node *eb64)
{
	return eb64_entry(eb_prev_unique(&eb64->node), struct eb64_node, node);
}

/* Delete node from the tree if it was linked in. Mark the node unused. Note
 * that this function relies on a non-inlined generic function: eb_delete.
 */
inline void eb64_delete(struct eb64_node *eb64)
{
	eb_delete(&eb64->node);
}

/*
 * Find the first occurence of a key in the tree <root>. If none can be
 * found, return NULL.
 */
struct eb64_node *eb64_lookup(struct eb_root *root, uint64_t x)
{
	struct eb64_node *node;
	eb_troot_t *troot;
	uint64_t y;
	int node_bit;

	troot = root->b[EB_LEFT];
	if (unlikely(troot == NULL))
		return NULL;

	while (1) {
		if ((eb_gettag(troot) == EB_LEAF)) {
			node = container_of(eb_untag(troot, EB_LEAF),
					    struct eb64_node, node.branches);
			if (node->key == x)
				return node;
			else
				return NULL;
		}
		node = container_of(eb_untag(troot, EB_NODE),
				    struct eb64_node, node.branches);
		node_bit = node->node.bit;

		y = node->key ^ x;
		if (!y) {
			/* Either we found the node which holds the key, or
			 * we have a dup tree. In the later case, we have to
			 * walk it down left to get the first entry.
			 */
			if (node->node.bit < 0) {
				troot = node->node.branches.b[EB_LEFT];
				while (eb_gettag(troot) != EB_LEAF)
					troot = (eb_untag(troot, EB_NODE))->b[EB_LEFT];
				node = container_of(eb_untag(troot, EB_LEAF),
						    struct eb64_node, node.branches);
			}
			return node;
		}

		if ((y >> node->node.bit) >= EB_NODE_BRANCHES)
			return NULL; /* no more common bits */

		troot = node->node.branches.b[(x >> node->node.bit) & EB_NODE_BRANCH_MASK];
	}
}

/*
 * Find the first occurence of a signed key in the tree <root>. If none can
 * be found, return NULL.
 */
struct eb64_node *eb64i_lookup(struct eb_root *root, int64_t x)
{
	struct eb64_node *node;
	eb_troot_t *troot;
	uint64_t key = x ^ (1ULL << 63);
	uint64_t y;
	int node_bit;

	troot = root->b[EB_LEFT];
	if (unlikely(troot == NULL))
		return NULL;

	while (1) {
		if ((eb_gettag(troot) == EB_LEAF)) {
			node = container_of(eb_untag(troot, EB_LEAF),
					    struct eb64_node, node.branches);
			if (node->key == (uint64_t)x)
				return node;
			else
				return NULL;
		}
		node = container_of(eb_untag(troot, EB_NODE),
				    struct eb64_node, node.branches);
		node_bit = node->node.bit;

		y = node->key ^ x;
		if (!y) {
			/* Either we found the node which holds the key, or
			 * we have a dup tree. In the later case, we have to
			 * walk it down left to get the first entry.
			 */
			if (node->node.bit < 0) {
				troot = node->node.branches.b[EB_LEFT];
				while (eb_gettag(troot) != EB_LEAF)
					troot = (eb_untag(troot, EB_NODE))->b[EB_LEFT];
				node = container_of(eb_untag(troot, EB_LEAF),
						    struct eb64_node, node.branches);
			}
			return node;
		}

		if ((y >> node->node.bit) >= EB_NODE_BRANCHES)
			return NULL; /* no more common bits */

		troot = node->node.branches.b[(key >> node->node.bit) & EB_NODE_BRANCH_MASK];
	}
}

/* Insert eb64_node <new> into subtree starting at node root <root>.
 * Only new->key needs be set with the key. The eb64_node is returned.
 * If root->b[EB_RGHT]==1, the tree may only contain unique keys.
 */
struct eb64_node *eb64_insert(struct eb_root *root, struct eb64_node *new) {
	struct eb64_node *old;
	unsigned int side;
	eb_troot_t *troot;
	uint64_t newkey; /* caching the key saves approximately one cycle */
	eb_troot_t *root_right = root;
	int old_node_bit;

	side = EB_LEFT;
	troot = root->b[EB_LEFT];
	root_right = root->b[EB_RGHT];
	if (unlikely(troot == NULL)) {
		/* Tree is empty, insert the leaf part below the left branch */
		root->b[EB_LEFT] = eb_dotag(&new->node.branches, EB_LEAF);
		new->node.leaf_p = eb_dotag(root, EB_LEFT);
		new->node.node_p = NULL; /* node part unused */
		return new;
	}

	/* The tree descent is fairly easy :
	 *  - first, check if we have reached a leaf node
	 *  - second, check if we have gone too far
	 *  - third, reiterate
	 * Everywhere, we use <new> for the node node we are inserting, <root>
	 * for the node we attach it to, and <old> for the node we are
	 * displacing below <new>. <troot> will always point to the future node
	 * (tagged with its type). <side> carries the side the node <new> is
	 * attached to below its parent, which is also where previous node
	 * was attached. <newkey> carries the key being inserted.
	 */
	newkey = new->key;

	while (1) {
		if (unlikely(eb_gettag(troot) == EB_LEAF)) {
			eb_troot_t *new_left, *new_rght;
			eb_troot_t *new_leaf, *old_leaf;

			old = container_of(eb_untag(troot, EB_LEAF),
					    struct eb64_node, node.branches);

			new_left = eb_dotag(&new->node.branches, EB_LEFT);
			new_rght = eb_dotag(&new->node.branches, EB_RGHT);
			new_leaf = eb_dotag(&new->node.branches, EB_LEAF);
			old_leaf = eb_dotag(&old->node.branches, EB_LEAF);

			new->node.node_p = old->node.leaf_p;

			/* Right here, we have 3 possibilities :
			   - the tree does not contain the key, and we have
			     new->key < old->key. We insert new above old, on
			     the left ;

			   - the tree does not contain the key, and we have
			     new->key > old->key. We insert new above old, on
			     the right ;

			   - the tree does contain the key, which implies it
			     is alone. We add the new key next to it as a
			     first duplicate.

			   The last two cases can easily be partially merged.
			*/
			 
			if (new->key < old->key) {
				new->node.leaf_p = new_left;
				old->node.leaf_p = new_rght;
				new->node.branches.b[EB_LEFT] = new_leaf;
				new->node.branches.b[EB_RGHT] = old_leaf;
			} else {
				/* we may refuse to duplicate this key if the tree is
				 * tagged as containing only unique keys.
				 */
				if ((new->key == old->key) && eb_gettag(root_right))
					return old;

				/* new->key >= old->key, new goes the right */
				old->node.leaf_p = new_left;
				new->node.leaf_p = new_rght;
				new->node.branches.b[EB_LEFT] = old_leaf;
				new->node.branches.b[EB_RGHT] = new_leaf;

				if (new->key == old->key) {
					new->node.bit = -1;
					root->b[side] = eb_dotag(&new->node.branches, EB_NODE);
					return new;
				}
			}
			break;
		}

		/* OK we're walking down this link */
		old = container_of(eb_untag(troot, EB_NODE),
				    struct eb64_node, node.branches);
		old_node_bit = old->node.bit;

		/* Stop going down when we don't have common bits anymore. We
		 * also stop in front of a duplicates tree because it means we
		 * have to insert above.
		 */

		if ((old_node_bit < 0) || /* we're above a duplicate tree, stop here */
		    (((new->key ^ old->key) >> old_node_bit) >= EB_NODE_BRANCHES)) {
			/* The tree did not contain the key, so we insert <new> before the node
			 * <old>, and set ->bit to designate the lowest bit position in <new>
			 * which applies to ->branches.b[].
			 */
			eb_troot_t *new_left, *new_rght;
			eb_troot_t *new_leaf, *old_node;

			new_left = eb_dotag(&new->node.branches, EB_LEFT);
			new_rght = eb_dotag(&new->node.branches, EB_RGHT);
			new_leaf = eb_dotag(&new->node.branches, EB_LEAF);
			old_node = eb_dotag(&old->node.branches, EB_NODE);

			new->node.node_p = old->node.node_p;

			if (new->key < old->key) {
				new->node.leaf_p = new_left;
				old->node.node_p = new_rght;
				new->node.branches.b[EB_LEFT] = new_leaf;
				new->node.branches.b[EB_RGHT] = old_node;
			}
			else if (new->key > old->key) {
				old->node.node_p = new_left;
				new->node.leaf_p = new_rght;
				new->node.branches.b[EB_LEFT] = old_node;
				new->node.branches.b[EB_RGHT] = new_leaf;
			}
			else {
				struct eb_node *ret;
				ret = eb_insert_dup(&old->node, &new->node);
				return container_of(ret, struct eb64_node, node);
			}
			break;
		}

		/* walk down */
		root = &old->node.branches;
#if ULONG_MAX >= UINT64_T_MAX
		side = (newkey >> old_node_bit) & EB_NODE_BRANCH_MASK;
#else
		side = newkey;
		side >>= old_node_bit;
		if (old_node_bit >= 0x20) {
			side = newkey >> 0x20;
			side >>= old_node_bit & 0x1F;
		}
		side &= EB_NODE_BRANCH_MASK;
#endif
		troot = root->b[side];
	}

	/* Ok, now we are inserting <new> between <root> and <old>. <old>'s
	 * parent is already set to <new>, and the <root>'s branch is still in
	 * <side>. Update the root's leaf till we have it. Note that we can also
	 * find the side by checking the side of new->node.node_p.
	 */

	/* We need the common higher bits between new->key and old->key.
	 * What differences are there between new->key and the node here ?
	 * NOTE that bit(new) is always < bit(root) because highest
	 * bit of new->key and old->key are identical here (otherwise they
	 * would sit on different branches).
	 */
	// note that if EB_NODE_BITS > 1, we should check that it's still >= 0
	new->node.bit = fls64(new->key ^ old->key) - EB_NODE_BITS;
	root->b[side] = eb_dotag(&new->node.branches, EB_NODE);

	return new;
}

/* Insert eb64_node <new> into subtree starting at node root <root>, using
 * signed keys. Only new->key needs be set with the key. The eb64_node
 * is returned. If root->b[EB_RGHT]==1, the tree may only contain unique keys.
 */
struct eb64_node *eb64i_insert(struct eb_root *root, struct eb64_node *new) {
	struct eb64_node *old;
	unsigned int side;
	eb_troot_t *troot;
	uint64_t newkey; /* caching the key saves approximately one cycle */
	eb_troot_t *root_right = root;
	int old_node_bit;

	side = EB_LEFT;
	troot = root->b[EB_LEFT];
	root_right = root->b[EB_RGHT];
	if (unlikely(troot == NULL)) {
		/* Tree is empty, insert the leaf part below the left branch */
		root->b[EB_LEFT] = eb_dotag(&new->node.branches, EB_LEAF);
		new->node.leaf_p = eb_dotag(root, EB_LEFT);
		new->node.node_p = NULL; /* node part unused */
		return new;
	}

	/* The tree descent is fairly easy :
	 *  - first, check if we have reached a leaf node
	 *  - second, check if we have gone too far
	 *  - third, reiterate
	 * Everywhere, we use <new> for the node node we are inserting, <root>
	 * for the node we attach it to, and <old> for the node we are
	 * displacing below <new>. <troot> will always point to the future node
	 * (tagged with its type). <side> carries the side the node <new> is
	 * attached to below its parent, which is also where previous node
	 * was attached. <newkey> carries a high bit shift of the key being
	 * inserted in order to have negative keys stored before positive
	 * ones.
	 */
	newkey = new->key ^ (1ULL << 63);

	while (1) {
		if (unlikely(eb_gettag(troot) == EB_LEAF)) {
			eb_troot_t *new_left, *new_rght;
			eb_troot_t *new_leaf, *old_leaf;

			old = container_of(eb_untag(troot, EB_LEAF),
					    struct eb64_node, node.branches);

			new_left = eb_dotag(&new->node.branches, EB_LEFT);
			new_rght = eb_dotag(&new->node.branches, EB_RGHT);
			new_leaf = eb_dotag(&new->node.branches, EB_LEAF);
			old_leaf = eb_dotag(&old->node.branches, EB_LEAF);

			new->node.node_p = old->node.leaf_p;

			/* Right here, we have 3 possibilities :
			   - the tree does not contain the key, and we have
			     new->key < old->key. We insert new above old, on
			     the left ;

			   - the tree does not contain the key, and we have
			     new->key > old->key. We insert new above old, on
			     the right ;

			   - the tree does contain the key, which implies it
			     is alone. We add the new key next to it as a
			     first duplicate.

			   The last two cases can easily be partially merged.
			*/
			 
			if ((int64_t)new->key < (int64_t)old->key) {
				new->node.leaf_p = new_left;
				old->node.leaf_p = new_rght;
				new->node.branches.b[EB_LEFT] = new_leaf;
				new->node.branches.b[EB_RGHT] = old_leaf;
			} else {
				/* we may refuse to duplicate this key if the tree is
				 * tagged as containing only unique keys.
				 */
				if ((new->key == old->key) && eb_gettag(root_right))
					return old;

				/* new->key >= old->key, new goes the right */
				old->node.leaf_p = new_left;
				new->node.leaf_p = new_rght;
				new->node.branches.b[EB_LEFT] = old_leaf;
				new->node.branches.b[EB_RGHT] = new_leaf;

				if (new->key == old->key) {
					new->node.bit = -1;
					root->b[side] = eb_dotag(&new->node.branches, EB_NODE);
					return new;
				}
			}
			break;
		}

		/* OK we're walking down this link */
		old = container_of(eb_untag(troot, EB_NODE),
				    struct eb64_node, node.branches);
		old_node_bit = old->node.bit;

		/* Stop going down when we don't have common bits anymore. We
		 * also stop in front of a duplicates tree because it means we
		 * have to insert above.
		 */

		if ((old_node_bit < 0) || /* we're above a duplicate tree, stop here */
		    (((new->key ^ old->key) >> old_node_bit) >= EB_NODE_BRANCHES)) {
			/* The tree did not contain the key, so we insert <new> before the node
			 * <old>, and set ->bit to designate the lowest bit position in <new>
			 * which applies to ->branches.b[].
			 */
			eb_troot_t *new_left, *new_rght;
			eb_troot_t *new_leaf, *old_node;

			new_left = eb_dotag(&new->node.branches, EB_LEFT);
			new_rght = eb_dotag(&new->node.branches, EB_RGHT);
			new_leaf = eb_dotag(&new->node.branches, EB_LEAF);
			old_node = eb_dotag(&old->node.branches, EB_NODE);

			new->node.node_p = old->node.node_p;

			if ((int64_t)new->key < (int64_t)old->key) {
				new->node.leaf_p = new_left;
				old->node.node_p = new_rght;
				new->node.branches.b[EB_LEFT] = new_leaf;
				new->node.branches.b[EB_RGHT] = old_node;
			}
			else if ((int64_t)new->key > (int64_t)old->key) {
				old->node.node_p = new_left;
				new->node.leaf_p = new_rght;
				new->node.branches.b[EB_LEFT] = old_node;
				new->node.branches.b[EB_RGHT] = new_leaf;
			}
			else {
				struct eb_node *ret;
				ret = eb_insert_dup(&old->node, &new->node);
				return container_of(ret, struct eb64_node, node);
			}
			break;
		}

		/* walk down */
		root = &old->node.branches;
#if ULONG_MAX >= UINT64_T_MAX
		side = (newkey >> old_node_bit) & EB_NODE_BRANCH_MASK;
#else
		side = newkey;
		side >>= old_node_bit;
		if (old_node_bit >= 0x20) {
			side = newkey >> 0x20;
			side >>= old_node_bit & 0x1F;
		}
		side &= EB_NODE_BRANCH_MASK;
#endif
		troot = root->b[side];
	}

	/* Ok, now we are inserting <new> between <root> and <old>. <old>'s
	 * parent is already set to <new>, and the <root>'s branch is still in
	 * <side>. Update the root's leaf till we have it. Note that we can also
	 * find the side by checking the side of new->node.node_p.
	 */

	/* We need the common higher bits between new->key and old->key.
	 * What differences are there between new->key and the node here ?
	 * NOTE that bit(new) is always < bit(root) because highest
	 * bit of new->key and old->key are identical here (otherwise they
	 * would sit on different branches).
	 */
	// note that if EB_NODE_BITS > 1, we should check that it's still >= 0
	new->node.bit = fls64(new->key ^ old->key) - EB_NODE_BITS;
	root->b[side] = eb_dotag(&new->node.branches, EB_NODE);

	return new;
}

/*
 * Find the last occurrence of the highest key in the tree <root>, which is
 * equal to or less than <x>. NULL is returned is no key matches.
 */
struct eb64_node *eb64_lookup_le(struct eb_root *root, uint64_t x)
{
	struct eb64_node *node;
	eb_troot_t *troot;

	troot = root->b[EB_LEFT];
	if (unlikely(troot == NULL))
		return NULL;

	while (1) {
		if ((eb_gettag(troot) == EB_LEAF)) {
			/* We reached a leaf, which means that the whole upper
			 * parts were common. We will return either the current
			 * node or its next one if the former is too small.
			 */
			node = container_of(eb_untag(troot, EB_LEAF),
					    struct eb64_node, node.branches);
			if (node->key <= x)
				return node;
			/* return prev */
			troot = node->node.leaf_p;
			break;
		}
		node = container_of(eb_untag(troot, EB_NODE),
				    struct eb64_node, node.branches);

		if (node->node.bit < 0) {
			/* We're at the top of a dup tree. Either we got a
			 * matching value and we return the rightmost node, or
			 * we don't and we skip the whole subtree to return the
			 * prev node before the subtree. Note that since we're
			 * at the top of the dup tree, we can simply return the
			 * prev node without first trying to escape from the
			 * tree.
			 */
			if (node->key <= x) {
				troot = node->node.branches.b[EB_RGHT];
				while (eb_gettag(troot) != EB_LEAF)
					troot = (eb_untag(troot, EB_NODE))->b[EB_RGHT];
				return container_of(eb_untag(troot, EB_LEAF),
						    struct eb64_node, node.branches);
			}
			/* return prev */
			troot = node->node.node_p;
			break;
		}

		if (((x ^ node->key) >> node->node.bit) >= EB_NODE_BRANCHES) {
			/* No more common bits at all. Either this node is too
			 * small and we need to get its highest value, or it is
			 * too large, and we need to get the prev value.
			 */
			if ((node->key >> node->node.bit) < (x >> node->node.bit)) {
				troot = node->node.branches.b[EB_RGHT];
				return eb64_entry(eb_walk_down(troot, EB_RGHT), struct eb64_node, node);
			}

			/* Further values will be too high here, so return the prev
			 * unique node (if it exists).
			 */
			troot = node->node.node_p;
			break;
		}
		troot = node->node.branches.b[(x >> node->node.bit) & EB_NODE_BRANCH_MASK];
	}

	/* If we get here, it means we want to report previous node before the
	 * current one which is not above. <troot> is already initialised to
	 * the parent's branches.
	 */
	while (eb_gettag(troot) == EB_LEFT) {
		/* Walking up from left branch. We must ensure that we never
		 * walk beyond root.
		 */
		if (unlikely(eb_clrtag((eb_untag(troot, EB_LEFT))->b[EB_RGHT]) == NULL))
			return NULL;
		troot = (eb_root_to_node(eb_untag(troot, EB_LEFT)))->node_p;
	}
	/* Note that <troot> cannot be NULL at this stage */
	troot = (eb_untag(troot, EB_RGHT))->b[EB_LEFT];
	node = eb64_entry(eb_walk_down(troot, EB_RGHT), struct eb64_node, node);
	return node;
}

/*
 * Find the first occurrence of the lowest key in the tree <root>, which is
 * equal to or greater than <x>. NULL is returned is no key matches.
 */
struct eb64_node *eb64_lookup_ge(struct eb_root *root, uint64_t x)
{
	struct eb64_node *node;
	eb_troot_t *troot;

	troot = root->b[EB_LEFT];
	if (unlikely(troot == NULL))
		return NULL;

	while (1) {
		if ((eb_gettag(troot) == EB_LEAF)) {
			/* We reached a leaf, which means that the whole upper
			 * parts were common. We will return either the current
			 * node or its next one if the former is too small.
			 */
			node = container_of(eb_untag(troot, EB_LEAF),
					    struct eb64_node, node.branches);
			if (node->key >= x)
				return node;
			/* return next */
			troot = node->node.leaf_p;
			break;
		}
		node = container_of(eb_untag(troot, EB_NODE),
				    struct eb64_node, node.branches);

		if (node->node.bit < 0) {
			/* We're at the top of a dup tree. Either we got a
			 * matching value and we return the leftmost node, or
			 * we don't and we skip the whole subtree to return the
			 * next node after the subtree. Note that since we're
			 * at the top of the dup tree, we can simply return the
			 * next node without first trying to escape from the
			 * tree.
			 */
			if (node->key >= x) {
				troot = node->node.branches.b[EB_LEFT];
				while (eb_gettag(troot) != EB_LEAF)
					troot = (eb_untag(troot, EB_NODE))->b[EB_LEFT];
				return container_of(eb_untag(troot, EB_LEAF),
						    struct eb64_node, node.branches);
			}
			/* return next */
			troot = node->node.node_p;
			break;
		}

		if (((x ^ node->key) >> node->node.bit) >= EB_NODE_BRANCHES) {
			/* No more common bits at all. Either this node is too
			 * large and we need to get its lowest value, or it is too
			 * small, and we need to get the next value.
			 */
			if ((node->key >> node->node.bit) > (x >> node->node.bit)) {
				troot = node->node.branches.b[EB_LEFT];
				return eb64_entry(eb_walk_down(troot, EB_LEFT), struct eb64_node, node);
			}

			/* Further values will be too low here, so return the next
			 * unique node (if it exists).
			 */
			troot = node->node.node_p;
			break;
		}
		troot = node->node.branches.b[(x >> node->node.bit) & EB_NODE_BRANCH_MASK];
	}

	/* If we get here, it means we want to report next node after the
	 * current one which is not below. <troot> is already initialised
	 * to the parent's branches.
	 */
	while (eb_gettag(troot) != EB_LEFT)
		/* Walking up from right branch, so we cannot be below root */
		troot = (eb_root_to_node(eb_untag(troot, EB_RGHT)))->node_p;

	/* Note that <troot> cannot be NULL at this stage */
	troot = (eb_untag(troot, EB_LEFT))->b[EB_RGHT];
	if (eb_clrtag(troot) == NULL)
		return NULL;

	node = eb64_entry(eb_walk_down(troot, EB_LEFT), struct eb64_node, node);
	return node;
}
