#!/bin/sh
set -xe

# Build the docker image.
docker build --pull -t ebtree:$DIST -f Dockerfile.$DIST .
