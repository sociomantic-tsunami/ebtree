#!/bin/sh
set -xe

# Travis likes to fetch submodules recursively, but the build system assumes
# shalow submodules fetching, so we need to remove all the recursive submodules
# before proceeding
git submodule foreach --recursive git submodule deinit --force --all

# Build the docker image.
docker build --pull -t ebtree:$DIST -f Dockerfile.$DIST .
