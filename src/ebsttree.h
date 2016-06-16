/*
 * Elastic Binary Trees - macros to manipulate String data nodes.
 * Version 6.0
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 */

/* These functions and macros rely on Multi-Byte nodes */

#ifndef _EBSTTREE_H
#define _EBSTTREE_H

#include "ebtree.h"
#include "ebmbtree.h"

/* The following functions are not inlined by default. They are declared
 * in ebsttree.c, which simply relies on their inline version.
 */
extern struct ebmb_node *ebst_lookup(struct eb_root *root, const char *x);
extern struct ebmb_node *ebst_lookup_len(struct eb_root *root, const char *x, unsigned int len);
extern struct ebmb_node *ebst_insert(struct eb_root *root, struct ebmb_node *new);

#endif /* _EBSTTREE_H */

