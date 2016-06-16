# Makefile for ebtree
#
# Copyright (C) 2000-2016 Willy Tarreau <w@1wt.eu>
# Copyright (C) 2009-2016 Sociomantic Labs GmbH.
# Distributed under MIT/X11 license (See accompanying file LICENSE)

o ?= obj

DEBUG ?= 1

CFLAGS := -O3 -Wall -std=c99 $(if $(DEBUG),-g)

VERSION := 6
OBJS := ebtree.o eb32tree.o eb64tree.o eb128tree.o ebmbtree.o ebsttree.o ebimtree.o ebistree.o

$(shell mkdir -p $o)

.PHONY: all
all: libebtree.a libebtree.so

libebtree.a: $(addprefix $o/static-,$(OBJS))
	$(AR) rv $@ $^

libebtree.so: CFLAGS += -fPIC
libebtree.so: $(addprefix $o/dynamic-,$(OBJS))
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) $(OUTPUT_OPTION) -shared -Wl,-soname,$@.$(VERSION)

$o/static-%.o: src/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$o/dynamic-%.o: src/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

.PHONY: deb
deb: all
	deb/build

.PHONY: clean
clean:
	$(RM) -r libebtree.a libebtree.so $o deb/libebtree* deb/ebtree/

