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

#ifndef _EBTREE_H
#define _EBTREE_H

#include <stdlib.h>
#include <stdint.h>
#include "compiler.h"
#include "int128.h"

/* Number of bits per node, and number of leaves per node */
#define EB_NODE_BITS          1
#define EB_NODE_BRANCHES      (1 << EB_NODE_BITS)
#define EB_NODE_BRANCH_MASK   (EB_NODE_BRANCHES - 1)

/* Be careful not to tweak those values. The walking code is optimized for NULL
 * detection on the assumption that the following values are intact.
 */
#define EB_LEFT     0
#define EB_RGHT     1
#define EB_LEAF     0
#define EB_NODE     1

/* Tags to set in root->b[EB_RGHT] :
 * - EB_NORMAL is a normal tree which stores duplicate keys.
 * - EB_UNIQUE is a tree which stores unique keys.
 */
#define EB_NORMAL   0
#define EB_UNIQUE   1


/* Linux-like "container_of". It returns a pointer to the structure of type
 * <type> which has its member <name> stored at address <ptr>.
 */
#ifndef container_of
#define container_of(ptr, type, name) ((type *)(((void *)(ptr)) - ((long)&((type *)0)->name)))
#endif


/* Return the structure of type <type> whose member <member> points to <ptr> */
#define eb_entry(ptr, type, member) container_of(ptr, type, member)

/* The root of a tree is an eb_root initialized with both pointers NULL.
 * During its life, only the left pointer will change. The right one will
 * always remain NULL, which is the way we detect it.
 */
#define EB_ROOT						\
	(struct eb_root) {				\
		.b = {[0] = NULL, [1] = NULL },		\
	}

#define EB_ROOT_UNIQUE					\
	(struct eb_root) {				\
		.b = {[0] = NULL, [1] = (void *)1 },	\
	}

#define EB_TREE_HEAD(name)				\
	struct eb_root name = EB_ROOT


/* This is the same as an eb_node pointer, except that the lower bit embeds
 * a tag. See eb_dotag()/eb_untag()/eb_gettag(). This tag has two meanings :
 *  - 0=left, 1=right to designate the parent's branch for leaf_p/node_p
 *  - 0=link, 1=leaf  to designate the branch's type for branch[]
 */
typedef void eb_troot_t;

/* The eb_root connects the node which contains it, to two nodes below it, one
 * of which may be the same node. At the top of the tree, we use an eb_root
 * too, which always has its right branch NULL (+/1 low-order bits).
 */
struct eb_root {
	eb_troot_t    *b[EB_NODE_BRANCHES]; /* left and right branches */
};

/* The eb_node contains the two parts, one for the leaf, which always exists,
 * and one for the node, which remains unused in the very first node inserted
 * into the tree. This structure is 20 bytes per node on 32-bit machines. Do
 * not change the order, benchmarks have shown that it's optimal this way.
 */
struct eb_node {
	struct eb_root branches; /* branches, must be at the beginning */
	eb_troot_t    *node_p;  /* link node's parent */
	eb_troot_t    *leaf_p;  /* leaf node's parent */
	short int      bit;     /* link's bit position. */
	short int      pfx;     /* data prefix length, always related to leaf */
};

/**************************************\
 * Public functions, for the end-user *
\**************************************/

/* Return non-zero if the tree is empty, otherwise zero */
extern int eb_is_empty(struct eb_root *root);

/* Return the first leaf in the tree starting at <root>, or NULL if none */
extern struct eb_node *eb_first(struct eb_root *root);

/* Return the last leaf in the tree starting at <root>, or NULL if none */
extern struct eb_node *eb_last(struct eb_root *root);

/* Return previous leaf node before an existing leaf node, or NULL if none. */
extern struct eb_node *eb_prev(struct eb_node *node);
/* Return next leaf node after an existing leaf node, or NULL if none. */
extern struct eb_node *eb_next(struct eb_node *node);
/* Return previous leaf node before an existing leaf node, skipping duplicates,
 * or NULL if none. */
extern struct eb_node *eb_prev_unique(struct eb_node *node);
/* Return next leaf node after an existing leaf node, skipping duplicates, or
 * NULL if none.
 */
extern struct eb_node *eb_next_unique(struct eb_node *node);
/* Removes a leaf node from the tree if it was still in it. Marks the node
 * as unlinked.
 */
extern void eb_delete(struct eb_node *node);

/* Compare blocks <a> and <b> byte-to-byte, from bit <ignore> to bit <len-1>.
 * Return the number of equal bits between strings, assuming that the first
 * <ignore> bits are already identical. It is possible to return slightly more
 * than <len> bits if <len> does not stop on a byte boundary and we find exact
 * bytes. Note that parts or all of <ignore> bits may be rechecked. It is only
 * passed here as a hint to speed up the check.
 */
extern int equal_bits(const unsigned char *a,
				  const unsigned char *b,
				  int ignore, int len);

/* check that the two blocks <a> and <b> are equal on <len> bits. If it is known
 * they already are on some bytes, this number of equal bytes to be skipped may
 * be passed in <skip>. It returns 0 if they match, otherwise non-zero.
 */
extern int check_bits(const unsigned char *a,
				  const unsigned char *b,
				  int skip,
				  int len);

/* Compare strings <a> and <b> byte-to-byte, from bit <ignore> to the last 0.
 * Return the number of equal bits between strings, assuming that the first
 * <ignore> bits are already identical. Note that parts or all of <ignore> bits
 * may be rechecked. It is only passed here as a hint to speed up the check.
 * The caller is responsible for not passing an <ignore> value larger than any
 * of the two strings. However, referencing any bit from the trailing zero is
 * permitted. Equal strings are reported as a negative number of bits, which
 * indicates the end was reached.
 */
extern int string_equal_bits(const unsigned char *a,
					 const unsigned char *b,
					 int ignore);

extern int cmp_bits(const unsigned char *a, const unsigned char *b, unsigned int pos);

extern int get_bit(const unsigned char *a, unsigned int pos);

/***************************************\
 * Private functions. Not for end-user *
\***************************************/

static struct eb_troot_t *eb_dotag(const struct eb_root *root, int tag);
static struct eb_root *eb_untag(const eb_troot_t *troot, int tag);
static struct eb_root *eb_untag(const eb_troot_t *troot, int tag);
static struct eb_root *eb_clrtag(const eb_troot_t *troot);
static struct eb_node *eb_root_to_node(struct eb_root *root);
extern struct eb_node *eb_walk_down(eb_troot_t *start, unsigned int side);
extern struct eb_node *eb_insert_dup(struct eb_node *sub, struct eb_node *new);
extern void eb_delete(struct eb_node *node);

/* Converts a root pointer to its equivalent eb_troot_t pointer,
 * ready to be stored in ->branch[], leaf_p or node_p. NULL is not
 * conserved. To be used with EB_LEAF, EB_NODE, EB_LEFT or EB_RGHT in <tag>.
 */
static inline struct eb_troot_t *eb_dotag(const struct eb_root *root, const int tag)
{
	return (eb_troot_t *)((void *)root + tag);
}

/* Converts an eb_troot_t pointer pointer to its equivalent eb_root pointer,
 * for use with pointers from ->branch[], leaf_p or node_p. NULL is conserved
 * as long as the tree is not corrupted. To be used with EB_LEAF, EB_NODE,
 * EB_LEFT or EB_RGHT in <tag>.
 */
static inline struct eb_root *eb_untag(const eb_troot_t *troot, const int tag)
{
	return (struct eb_root *)((void *)troot - tag);
}

/* returns the tag associated with an eb_troot_t pointer */
static inline int eb_gettag(eb_troot_t *troot)
{
	return (unsigned long)troot & 1;
}

/* Converts a root pointer to its equivalent eb_troot_t pointer and clears the
 * tag, no matter what its value was.
 */
static inline struct eb_root *eb_clrtag(const eb_troot_t *troot)
{
	return (struct eb_root *)((unsigned long)troot & ~1UL);
}

/* Returns a pointer to the eb_node holding <root> */
static inline struct eb_node *eb_root_to_node(struct eb_root *root)
{
	return container_of(root, struct eb_node, branches);
}

static inline int flsnz8_generic(unsigned int x)
{
	int ret = 0;
	if (x >> 4) { x >>= 4; ret += 4; }
	return ret + ((0xFFFFAA50U >> (x << 1)) & 3) + 1;
}

/* Note: we never need to run fls on null keys, so we can optimize the fls
 * function by removing a conditional jump.
 */
static inline int flsnz(const int x)
{
	int r;
#if defined(__i386__) || defined(__x86_64__)
/* this code is similar on 32 and 64 bit */
	__asm__("bsrl %1,%0\n" : "=r" (r) : "rm" (x));
#else
	r = 0;
	if (x & 0xffff0000) { x &= 0xffff0000; r += 0x10;}
	if (x & 0xff00ff00) { x &= 0xff00ff00; r += 0x08;}
	if (x & 0xf0f0f0f0) { x &= 0xf0f0f0f0; r += 0x04;}
	if (x & 0xcccccccc) { x &= 0xcccccccc; r += 0x02;}
	if (x & 0xaaaaaaaa) { x &= 0xaaaaaaaa; r += 0x01;}

#endif
	return r + 1;
}

static inline int flsnz8(const uint8_t x)
{
#if defined(__i386__) || defined(__x86_64__)
	int r;
	__asm__("movzbl %%al, %%eax\n"
		"bsrl %%eax,%0\n"
	        : "=r" (r) : "a" (x));
	return r+1;
#else
	return flsnz8_generic(x);
#endif
}

static inline int fls64(const uint64_t x)
{
#ifdef __x86_64__
	long r = -1;
	__asm__("bsrq %1,%0" : "+r" (r) : "rm" (x));
	return r + 1;
#else
	const uint32_t h = x >> 0x20;
	return h? flsnz(h) + 0x20 : flsnz(x);
#endif
}

#ifdef INT128_SUPPORTED

static inline int fls128(const uint128_t x)
{
	const uint64_t h = x >> 0x40;
	return h? fls64(h) + 0x40 : fls64(x);
}

#define fls_auto(x) (sizeof(x) > 8) ? fls128(x) : (sizeof(x) > 4) ? fls64(x) : flsnz(x))

#else   /* __SIZEOF_INT128__ */
#define fls_auto(x) (sizeof(x) > 4) ? fls64(x) : flsnz(x))
#endif  /* __SIZEOF_INT128__ */

#endif /* _EB_TREE_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 */
