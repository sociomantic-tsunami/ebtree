ebtree
======

Introduction
------------

This is a fork of the `Elastic Binary Tree Library`__ version 6 (by Willy
Terreau), that is part of HAProxy__, but is also distributed as a `standalone
library`__.

__ http://1wt.eu/articles/ebtree/
__ http://www.haproxy.org/
__ http://git.1wt.eu/web?p=ebtree.git;a=summary


Differences with upstream
-------------------------

Firstly, 128bit nodes support was added.

Then, all code in headers files were moved to C files so they get compiled in.
This was done in order to make the library more easily accessible from other
programming languages (in particular D__) by just binding to a simple
C API/ABI.

__ https://dlang.org/

We got in touch with the original author in the hope of some day merging our
changes back to upstream, but the original library is going through a major
rewrite that might make this changes even completely obsolete (at least the
128bit support, and the other changes, is not certain that they are useful for
the general public).


Building
--------

To build the project is enough to type::

    make

And to create a (simplified) Debian package using fpm__::

    make deb

__ https://github.com/jordansissel/fpm


License
-------

This project is released under MIT/X11 license, please see the accompanying
file LICENSE__.

__ https://github.com/sociomantic-tsunami/ebtree/blob/master/LICENSE
