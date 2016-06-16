/*
 * Elastic Binary Trees - generic macros and structures.
 * Version 6.0.1
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 * Short history :
 *
 * 2007/09/28: full support for the duplicates tree => v3
 * 2007/07/08: merge back cleanups from kernel version.
 * 2007/07/01: merge into Linux Kernel (try 1).
 * 2007/05/27: version 2: compact everything into one single struct
 * 2007/05/18: adapted the structure to support embedded nodes
 * 2007/05/13: adapted to mempools v2.
 */



/*
  General idea:
  -------------
  In a radix binary tree, we may have up to 2N-1 nodes for N keys if all of
  them are leaves. If we find a way to differentiate intermediate nodes (later
  called "nodes") and final nodes (later called "leaves"), and we associate
  them by two, it is possible to build sort of a self-contained radix tree with
  intermediate nodes always present. It will not be as cheap as the ultree for
  optimal cases as shown below, but the optimal case almost never happens :

  Eg, to store 8, 10, 12, 13, 14 :

             ultree          this theorical tree

               8                   8
              / \                 / \
             10 12               10 12
               /  \                /  \
              13  14              12  14
                                 / \
                                12 13

   Note that on real-world tests (with a scheduler), is was verified that the
   case with data on an intermediate node never happens. This is because the
   data spectrum is too large for such coincidences to happen. It would require
   for instance that a task has its expiration time at an exact second, with
   other tasks sharing that second. This is too rare to try to optimize for it.

   What is interesting is that the node will only be added above the leaf when
   necessary, which implies that it will always remain somewhere above it. So
   both the leaf and the node can share the exact value of the leaf, because
   when going down the node, the bit mask will be applied to comparisons. So we
   are tempted to have one single key shared between the node and the leaf.

   The bit only serves the nodes, and the dups only serve the leaves. So we can
   put a lot of information in common. This results in one single entity with
   two branch pointers and two parent pointers, one for the node part, and one
   for the leaf part :

              node's         leaf's
              parent         parent
                |              |
              [node]         [leaf]
               / \
           left   right
         branch   branch

   The node may very well refer to its leaf counterpart in one of its branches,
   indicating that its own leaf is just below it :

              node's
              parent
                |
              [node]
               / \
           left  [leaf]
         branch

   Adding keys in such a tree simply consists in inserting nodes between
   other nodes and/or leaves :

                [root]
                  |
               [node2]
                 / \
          [leaf1]   [node3]
                      / \
               [leaf2]   [leaf3]

   On this diagram, we notice that [node2] and [leaf2] have been pulled away
   from each other due to the insertion of [node3], just as if there would be
   an elastic between both parts. This elastic-like behaviour gave its name to
   the tree : "Elastic Binary Tree", or "EBtree". The entity which associates a
   node part and a leaf part will be called an "EB node".

   We also notice on the diagram that there is a root entity required to attach
   the tree. It only contains two branches and there is nothing above it. This
   is an "EB root". Some will note that [leaf1] has no [node1]. One property of
   the EBtree is that all nodes have their branches filled, and that if a node
   has only one branch, it does not need to exist. Here, [leaf1] was added
   below [root] and did not need any node.

   An EB node contains :
     - a pointer to the node's parent (node_p)
     - a pointer to the leaf's parent (leaf_p)
     - two branches pointing to lower nodes or leaves (branches)
     - a bit position (bit)
     - an optional key.

   The key here is optional because it's used only during insertion, in order
   to classify the nodes. Nothing else in the tree structure requires knowledge
   of the key. This makes it possible to write type-agnostic primitives for
   everything, and type-specific insertion primitives. This has led to consider
   two types of EB nodes. The type-agnostic ones will serve as a header for the
   other ones, and will simply be called "struct eb_node". The other ones will
   have their type indicated in the structure name. Eg: "struct eb32_node" for
   nodes carrying 32 bit keys.

   We will also node that the two branches in a node serve exactly the same
   purpose as an EB root. For this reason, a "struct eb_root" will be used as
   well inside the struct eb_node. In order to ease pointer manipulation and
   ROOT detection when walking upwards, all the pointers inside an eb_node will
   point to the eb_root part of the referenced EB nodes, relying on the same
   principle as the linked lists in Linux.

   Another important point to note, is that when walking inside a tree, it is
   very convenient to know where a node is attached in its parent, and what
   type of branch it has below it (leaf or node). In order to simplify the
   operations and to speed up the processing, it was decided in this specific
   implementation to use the lowest bit from the pointer to designate the side
   of the upper pointers (left/right) and the type of a branch (leaf/node).
   This practise is not mandatory by design, but an implementation-specific
   optimisation permitted on all platforms on which data must be aligned. All
   known 32 bit platforms align their integers and pointers to 32 bits, leaving
   the two lower bits unused. So, we say that the pointers are "tagged". And
   since they designate pointers to root parts, we simply call them
   "tagged root pointers", or "eb_troot" in the code.

   Duplicate keys are stored in a special manner. When inserting a key, if
   the same one is found, then an incremental binary tree is built at this
   place from these keys. This ensures that no special case has to be written
   to handle duplicates when walking through the tree or when deleting entries.
   It also guarantees that duplicates will be walked in the exact same order
   they were inserted. This is very important when trying to achieve fair
   processing distribution for instance.

   Algorithmic complexity can be derived from 3 variables :
     - the number of possible different keys in the tree : P
     - the number of entries in the tree : N
     - the number of duplicates for one key : D

   Note that this tree is deliberately NOT balanced. For this reason, the worst
   case may happen with a small tree (eg: 32 distinct keys of one bit). BUT,
   the operations required to manage such data are so much cheap that they make
   it worth using it even under such conditions. For instance, a balanced tree
   may require only 6 levels to store those 32 keys when this tree will
   require 32. But if per-level operations are 5 times cheaper, it wins.

   Minimal, Maximal and Average times are specified in number of operations.
   Minimal is given for best condition, Maximal for worst condition, and the
   average is reported for a tree containing random keys. An operation
   generally consists in jumping from one node to the other.

   Complexity :
     - lookup              : min=1, max=log(P), avg=log(N)
     - insertion from root : min=1, max=log(P), avg=log(N)
     - insertion of dups   : min=1, max=log(D), avg=log(D)/2 after lookup
     - deletion            : min=1, max=1,      avg=1
     - prev/next           : min=1, max=log(P), avg=2 :
       N/2 nodes need 1 hop  => 1*N/2
       N/4 nodes need 2 hops => 2*N/4
       N/8 nodes need 3 hops => 3*N/8
       ...
       N/x nodes need log(x) hops => log2(x)*N/x
       Total cost for all N nodes : sum[i=1..N](log2(i)*N/i) = N*sum[i=1..N](log2(i)/i)
       Average cost across N nodes = total / N = sum[i=1..N](log2(i)/i) = 2

   This design is currently limited to only two branches per node. Most of the
   tree descent algorithm would be compatible with more branches (eg: 4, to cut
   the height in half), but this would probably require more complex operations
   and the deletion algorithm would be problematic.

   Useful properties :
     - a node is always added above the leaf it is tied to, and never can get
       below nor in another branch. This implies that leaves directly attached
       to the root do not use their node part, which is indicated by a NULL
       value in node_p. This also enhances the cache efficiency when walking
       down the tree, because when the leaf is reached, its node part will
       already have been visited (unless it's the first leaf in the tree).

     - pointers to lower nodes or leaves are stored in "branch" pointers. Only
       the root node may have a NULL in either branch, it is not possible for
       other branches. Since the nodes are attached to the left branch of the
       root, it is not possible to see a NULL left branch when walking up a
       tree. Thus, an empty tree is immediately identified by a NULL left
       branch at the root. Conversely, the one and only way to identify the
       root node is to check that it right branch is NULL. Note that the
       NULL pointer may have a few low-order bits set.

     - a node connected to its own leaf will have branch[0|1] pointing to
       itself, and leaf_p pointing to itself.

     - a node can never have node_p pointing to itself.

     - a node is linked in a tree if and only if it has a non-null leaf_p.

     - a node can never have both branches equal, except for the root which can
       have them both NULL.

     - deletion only applies to leaves. When a leaf is deleted, its parent must
       be released too (unless it's the root), and its sibling must attach to
       the grand-parent, replacing the parent. Also, when a leaf is deleted,
       the node tied to this leaf will be removed and must be released too. If
       this node is different from the leaf's parent, the freshly released
       leaf's parent will be used to replace the node which must go. A released
       node will never be used anymore, so there's no point in tracking it.

     - the bit index in a node indicates the bit position in the key which is
       represented by the branches. That means that a node with (bit == 0) is
       just above two leaves. Negative bit values are used to build a duplicate
       tree. The first node above two identical leaves gets (bit == -1). This
       value logarithmically decreases as the duplicate tree grows. During
       duplicate insertion, a node is inserted above the highest bit value (the
       lowest absolute value) in the tree during the right-sided walk. If bit
       -1 is not encountered (highest < -1), we insert above last leaf.
       Otherwise, we insert above the node with the highest value which was not
       equal to the one of its parent + 1.

     - the "eb_next" primitive walks from left to right, which means from lower
       to higher keys. It returns duplicates in the order they were inserted.
       The "eb_first" primitive returns the left-most entry.

     - the "eb_prev" primitive walks from right to left, which means from
       higher to lower keys. It returns duplicates in the opposite order they
       were inserted. The "eb_last" primitive returns the right-most entry.

     - a tree which has 1 in the lower bit of its root's right branch is a
       tree with unique nodes. This means that when a node is inserted with
       a key which already exists will not be inserted, and the previous
       entry will be returned.

 */

#include "ebtree.h"
#include <stdlib.h>
#include "compiler.h"

/**************************************\
 * Public functions, for the end-user *
\**************************************/

/* Return non-zero if the tree is empty, otherwise zero */
int eb_is_empty(struct eb_root *root)
{
	return !root->b[EB_LEFT];
}

/* Return the first leaf in the tree starting at <root>, or NULL if none */
struct eb_node *eb_first(struct eb_root *root)
{
	return eb_walk_down(root->b[0], EB_LEFT);
}

/* Return the last leaf in the tree starting at <root>, or NULL if none */
struct eb_node *eb_last(struct eb_root *root)
{
	return eb_walk_down(root->b[0], EB_RGHT);
}

/* Return previous leaf node before an existing leaf node, or NULL if none. */
struct eb_node *eb_prev(struct eb_node *node)
{
	eb_troot_t *t = node->leaf_p;

	while (eb_gettag(t) == EB_LEFT) {
		/* Walking up from left branch. We must ensure that we never
		 * walk beyond root.
		 */
		if (unlikely(eb_clrtag((eb_untag(t, EB_LEFT))->b[EB_RGHT]) == NULL))
			return NULL;
		t = (eb_root_to_node(eb_untag(t, EB_LEFT)))->node_p;
	}
	/* Note that <t> cannot be NULL at this stage */
	t = (eb_untag(t, EB_RGHT))->b[EB_LEFT];
	return eb_walk_down(t, EB_RGHT);
}

/* Return next leaf node after an existing leaf node, or NULL if none. */
struct eb_node *eb_next(struct eb_node *node)
{
	eb_troot_t *t = node->leaf_p;

	while (eb_gettag(t) != EB_LEFT)
		/* Walking up from right branch, so we cannot be below root */
		t = (eb_root_to_node(eb_untag(t, EB_RGHT)))->node_p;

	/* Note that <t> cannot be NULL at this stage */
	t = (eb_untag(t, EB_LEFT))->b[EB_RGHT];
	if (eb_clrtag(t) == NULL)
		return NULL;
	return eb_walk_down(t, EB_LEFT);
}

/* Return previous leaf node before an existing leaf node, skipping duplicates,
 * or NULL if none. */
struct eb_node *eb_prev_unique(struct eb_node *node)
{
	eb_troot_t *t = node->leaf_p;

	while (1) {
		if (eb_gettag(t) != EB_LEFT) {
			node = eb_root_to_node(eb_untag(t, EB_RGHT));
			/* if we're right and not in duplicates, stop here */
			if (node->bit >= 0)
				break;
			t = node->node_p;
		}
		else {
			/* Walking up from left branch. We must ensure that we never
			 * walk beyond root.
			 */
			if (unlikely(eb_clrtag((eb_untag(t, EB_LEFT))->b[EB_RGHT]) == NULL))
				return NULL;
			t = (eb_root_to_node(eb_untag(t, EB_LEFT)))->node_p;
		}
	}
	/* Note that <t> cannot be NULL at this stage */
	t = (eb_untag(t, EB_RGHT))->b[EB_LEFT];
	return eb_walk_down(t, EB_RGHT);
}

/* Return next leaf node after an existing leaf node, skipping duplicates, or
 * NULL if none.
 */
struct eb_node *eb_next_unique(struct eb_node *node)
{
	eb_troot_t *t = node->leaf_p;

	while (1) {
		if (eb_gettag(t) == EB_LEFT) {
			if (unlikely(eb_clrtag((eb_untag(t, EB_LEFT))->b[EB_RGHT]) == NULL))
				return NULL;	/* we reached root */
			node = eb_root_to_node(eb_untag(t, EB_LEFT));
			/* if we're left and not in duplicates, stop here */
			if (node->bit >= 0)
				break;
			t = node->node_p;
		}
		else {
			/* Walking up from right branch, so we cannot be below root */
			t = (eb_root_to_node(eb_untag(t, EB_RGHT)))->node_p;
		}
	}

	/* Note that <t> cannot be NULL at this stage */
	t = (eb_untag(t, EB_LEFT))->b[EB_RGHT];
	if (eb_clrtag(t) == NULL)
		return NULL;
	return eb_walk_down(t, EB_LEFT);
}


/* Removes a leaf node from the tree if it was still in it. Marks the node
 * as unlinked.
 */
void eb_delete(struct eb_node *node)
{
	__label__ delete_unlink;
	unsigned int pside, gpside, sibtype;
	struct eb_node *parent;
	struct eb_root *gparent;

	if (!node->leaf_p)
		return;

	/* we need the parent, our side, and the grand parent */
	pside = eb_gettag(node->leaf_p);
	parent = eb_root_to_node(eb_untag(node->leaf_p, pside));

	/* We likely have to release the parent link, unless it's the root,
	 * in which case we only set our branch to NULL. Note that we can
	 * only be attached to the root by its left branch.
	 */

	if (eb_clrtag(parent->branches.b[EB_RGHT]) == NULL) {
		/* we're just below the root, it's trivial. */
		parent->branches.b[EB_LEFT] = NULL;
		goto delete_unlink;
	}

	/* To release our parent, we have to identify our sibling, and reparent
	 * it directly to/from the grand parent. Note that the sibling can
	 * either be a link or a leaf.
	 */

	gpside = eb_gettag(parent->node_p);
	gparent = eb_untag(parent->node_p, gpside);

	gparent->b[gpside] = parent->branches.b[!pside];
	sibtype = eb_gettag(gparent->b[gpside]);

	if (sibtype == EB_LEAF) {
		eb_root_to_node(eb_untag(gparent->b[gpside], EB_LEAF))->leaf_p =
			eb_dotag(gparent, gpside);
	} else {
		eb_root_to_node(eb_untag(gparent->b[gpside], EB_NODE))->node_p =
			eb_dotag(gparent, gpside);
	}
	/* Mark the parent unused. Note that we do not check if the parent is
	 * our own node, but that's not a problem because if it is, it will be
	 * marked unused at the same time, which we'll use below to know we can
	 * safely remove it.
	 */
	parent->node_p = NULL;

	/* The parent node has been detached, and is currently unused. It may
	 * belong to another node, so we cannot remove it that way. Also, our
	 * own node part might still be used. so we can use this spare node
	 * to replace ours if needed.
	 */

	/* If our link part is unused, we can safely exit now */
	if (!node->node_p)
		goto delete_unlink;

	/* From now on, <node> and <parent> are necessarily different, and the
	 * <node>'s node part is in use. By definition, <parent> is at least
	 * below <node>, so keeping its key for the bit string is OK.
	 */

	parent->node_p = node->node_p;
	parent->branches = node->branches;
	parent->bit = node->bit;

	/* We must now update the new node's parent... */
	gpside = eb_gettag(parent->node_p);
	gparent = eb_untag(parent->node_p, gpside);
	gparent->b[gpside] = eb_dotag(&parent->branches, EB_NODE);

	/* ... and its branches */
	for (pside = 0; pside <= 1; pside++) {
		if (eb_gettag(parent->branches.b[pside]) == EB_NODE) {
			eb_root_to_node(eb_untag(parent->branches.b[pside], EB_NODE))->node_p =
				eb_dotag(&parent->branches, pside);
		} else {
			eb_root_to_node(eb_untag(parent->branches.b[pside], EB_LEAF))->leaf_p =
				eb_dotag(&parent->branches, pside);
		}
	}
 delete_unlink:
	/* Now the node has been completely unlinked */
	node->leaf_p = NULL;
	return; /* tree is not empty yet */
}

/* Compare blocks <a> and <b> byte-to-byte, from bit <ignore> to bit <len-1>.
 * Return the number of equal bits between strings, assuming that the first
 * <ignore> bits are already identical. It is possible to return slightly more
 * than <len> bits if <len> does not stop on a byte boundary and we find exact
 * bytes. Note that parts or all of <ignore> bits may be rechecked. It is only
 * passed here as a hint to speed up the check.
 */
int equal_bits(const unsigned char *a,
				  const unsigned char *b,
				  int ignore, int len)
{
	for (ignore >>= 3, a += ignore, b += ignore, ignore <<= 3;
	     ignore < len; ) {
		unsigned char c;

		a++; b++;
		ignore += 8;
		c = b[-1] ^ a[-1];

		if (c) {
			/* OK now we know that old and new differ at byte <ptr> and that <c> holds
			 * the bit differences. We have to find what bit is differing and report
			 * it as the number of identical bits. Note that low bit numbers are
			 * assigned to high positions in the byte, as we compare them as strings.
			 */
			ignore -= flsnz8(c);
			break;
		}
	}
	return ignore;
}

/* check that the two blocks <a> and <b> are equal on <len> bits. If it is known
 * they already are on some bytes, this number of equal bytes to be skipped may
 * be passed in <skip>. It returns 0 if they match, otherwise non-zero.
 */
int check_bits(const unsigned char *a,
				  const unsigned char *b,
				  int skip,
				  int len)
{
	int bit, ret;

	/* This uncommon construction gives the best performance on x86 because
	 * it makes heavy use multiple-index addressing and parallel instructions,
	 * and it prevents gcc from reordering the loop since it is already
	 * properly oriented. Tested to be fine with 2.95 to 4.2.
	 */
	bit = ~len + (skip << 3) + 9; // = (skip << 3) + (8 - len)
	ret = a[skip] ^ b[skip];
	if (unlikely(bit >= 0))
		return ret >> bit;
	while (1) {
		skip++;
		if (ret)
			return ret;
		ret = a[skip] ^ b[skip];
		bit += 8;
		if (bit >= 0)
			return ret >> bit;
	}
}


/* Compare strings <a> and <b> byte-to-byte, from bit <ignore> to the last 0.
 * Return the number of equal bits between strings, assuming that the first
 * <ignore> bits are already identical. Note that parts or all of <ignore> bits
 * may be rechecked. It is only passed here as a hint to speed up the check.
 * The caller is responsible for not passing an <ignore> value larger than any
 * of the two strings. However, referencing any bit from the trailing zero is
 * permitted. Equal strings are reported as a negative number of bits, which
 * indicates the end was reached.
 */
int string_equal_bits(const unsigned char *a,
					 const unsigned char *b,
					 int ignore)
{
	int beg;
	unsigned char c;

	beg = ignore >> 3;

	/* skip known and identical bits. We stop at the first different byte
	 * or at the first zero we encounter on either side.
	 */
	while (1) {
		unsigned char d;

		c = a[beg];
		d = b[beg];
		beg++;

		c ^= d;
		if (c)
			break;
		if (!d)
			return -1;
	}
	/* OK now we know that a and b differ at byte <beg>, or that both are zero.
	 * We have to find what bit is differing and report it as the number of
	 * identical bits. Note that low bit numbers are assigned to high positions
	 * in the byte, as we compare them as strings.
	 */
	return (beg << 3) - flsnz8(c);
}

int cmp_bits(const unsigned char *a, const unsigned char *b, unsigned int pos)
{
	unsigned int ofs;
	unsigned char bit_a, bit_b;

	ofs = pos >> 3;
	pos = ~pos & 7;

	bit_a = (a[ofs] >> pos) & 1;
	bit_b = (b[ofs] >> pos) & 1;

	return bit_a - bit_b; /* -1: a<b; 0: a=b; 1: a>b */
}

int get_bit(const unsigned char *a, unsigned int pos)
{
	unsigned int ofs;

	ofs = pos >> 3;
	pos = ~pos & 7;
	return (a[ofs] >> pos) & 1;
}

/* Walks down starting at root pointer <start>, and always walking on side
 * <side>. It either returns the node hosting the first leaf on that side,
 * or NULL if no leaf is found. <start> may either be NULL or a branch pointer.
 * The pointer to the leaf (or NULL) is returned.
 */
struct eb_node *eb_walk_down(eb_troot_t *start, unsigned int side)
{
	/* A NULL pointer on an empty tree root will be returned as-is */
	while (eb_gettag(start) == EB_NODE)
		start = (eb_untag(start, EB_NODE))->b[side];
	/* NULL is left untouched (root==eb_node, EB_LEAF==0) */
	return eb_root_to_node(eb_untag(start, EB_LEAF));
}

/* This function is used to build a tree of duplicates by adding a new node to
 * a subtree of at least 2 entries. It will probably never be needed inlined,
 * and it is not for end-user.
 */
struct eb_node *eb_insert_dup(struct eb_node *sub, struct eb_node *new)
{
	struct eb_node *head = sub;

	/* FIXME: the next three lines provoke "initialization from incompatible
	 * pointer type" compiler warnings because eb_dotag() returns
	 * struct eb_troot_t *. Suggest a cast.
	 */

	struct eb_troot *new_left = (struct eb_troot *)eb_dotag(&new->branches, EB_LEFT);
	struct eb_troot *new_rght = (struct eb_troot *)eb_dotag(&new->branches, EB_RGHT);
	struct eb_troot *new_leaf = (struct eb_troot *)eb_dotag(&new->branches, EB_LEAF);

	/* first, identify the deepest hole on the right branch */
	while (eb_gettag(head->branches.b[EB_RGHT]) != EB_LEAF) {
		struct eb_node *last = head;
		head = container_of(eb_untag(head->branches.b[EB_RGHT], EB_NODE),
				    struct eb_node, branches);
		if (head->bit > last->bit + 1)
			sub = head;     /* there's a hole here */
	}

	/* Here we have a leaf attached to (head)->b[EB_RGHT] */
	if (head->bit < -1) {
		/* A hole exists just before the leaf, we insert there */
		new->bit = -1;
		sub = container_of(eb_untag(head->branches.b[EB_RGHT], EB_LEAF),
				   struct eb_node, branches);
		head->branches.b[EB_RGHT] = eb_dotag(&new->branches, EB_NODE);

		new->node_p = sub->leaf_p;
		new->leaf_p = new_rght;
		sub->leaf_p = new_left;
		new->branches.b[EB_LEFT] = eb_dotag(&sub->branches, EB_LEAF);
		new->branches.b[EB_RGHT] = new_leaf;
		return new;
	} else {
		int side;
		/* No hole was found before a leaf. We have to insert above
		 * <sub>. Note that we cannot be certain that <sub> is attached
		 * to the right of its parent, as this is only true if <sub>
		 * is inside the dup tree, not at the head.
		 */
		new->bit = sub->bit - 1; /* install at the lowest level */
		side = eb_gettag(sub->node_p);
		head = container_of(eb_untag(sub->node_p, side), struct eb_node, branches);
		head->branches.b[side] = eb_dotag(&new->branches, EB_NODE);

		new->node_p = sub->node_p;
		new->leaf_p = new_rght;
		sub->node_p = new_left;
		new->branches.b[EB_LEFT] = eb_dotag(&sub->branches, EB_NODE);
		new->branches.b[EB_RGHT] = new_leaf;
		return new;
	}
}

