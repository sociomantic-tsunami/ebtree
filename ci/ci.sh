#!/bin/sh
set -xe

# Defaults (in case they are not set by the CI)
DIST=${DIST:-xenial}

make -r deb
