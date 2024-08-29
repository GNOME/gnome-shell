#!/bin/bash
#
# This script ensures that all dependencies required to build and run
# a GNOME Shell system extension are present in:
#
# The building environment, by installing these dependencies to the container.
# The running environment, by bundling these dependencies with the extension.
#

set -e

# Install dependencies to $SYSEXT_DEST_DIR to bundle these with the extension.

SYSEXT_DEST_DIR="$(realpath $1)"

# Ensure that we're building against (and bundling) the right mutter branch
# and its dependencies:

SCRIPT_DIR="$(dirname $0)"

$SCRIPT_DIR/checkout-mutter.sh
./mutter/.gitlab-ci/install-gnomeos-sysext-dependencies.sh $SYSEXT_DEST_DIR

meson setup mutter/build mutter --prefix=/usr --libdir="lib/$(gcc -print-multiarch)"
meson compile -C mutter/build
meson install -C mutter/build --destdir $SYSEXT_DEST_DIR

sudo meson install -C mutter/build

# Ensure that any other dependency missing in GNOME OS is installed and bundled
# here as it was done with mutter:
