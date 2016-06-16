/*
 * Elastic Binary Trees - macros for Indirect Multi-Byte data nodes.
 * Version 6.0.5
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 */

#include "ebtree.h"
#include "ebpttree.h"

/* These functions and macros rely on Pointer nodes and use the <key> entry as
 * a pointer to an indirect key. Most operations are performed using ebpt_*.
 */

/* The following functions are not inlined by default. They are declared
 * in ebimtree.c, which simply relies on their inline version.
 */
extern struct ebpt_node *ebim_lookup(struct eb_root *root, const void *x, unsigned int len);
extern struct ebpt_node *ebim_insert(struct eb_root *root, struct ebpt_node *new, unsigned int len);
