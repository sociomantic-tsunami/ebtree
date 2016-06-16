/*
 * Elastic Binary Trees - macros to manipulate Indirect String data nodes.
 * Version 6.0
 *
 * Copyright (C) 2000-2015 Willy Tarreau <w@1wt.eu>
 * Distributed under MIT/X11 license (See accompanying file LICENSE)
 *
 */

/* These functions and macros rely on Multi-Byte nodes */

#include <string.h>
#include "ebtree.h"
#include "ebpttree.h"

/* These functions and macros rely on Pointer nodes and use the <key> entry as
 * a pointer to an indirect key. Most operations are performed using ebpt_*.
 */

/* The following functions are not inlined by default. They are declared
 * in ebistree.c, which simply relies on their inline version.
 */
extern struct ebpt_node *ebis_lookup(struct eb_root *root, const char *x);
extern struct ebpt_node *ebis_lookup_len(struct eb_root *root, const char *x, unsigned int len);
extern struct ebpt_node *ebis_insert(struct eb_root *root, struct ebpt_node *new);
