#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(test -f $srcdir/configure.ac \
  && test -d $srcdir/src) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome-shell directory"
    exit 1
}

pushd $srcdir

# Fetch submodules if needed
if test ! -f src/gvc/Makefile.am || test ! -f data/theme/gnome-shell-sass/COPYING;
then
  echo "+ Setting up submodules"
  git submodule init
fi
git submodule update

popd

which gnome-autogen.sh || {
    echo "You need to install gnome-common from GNOME Git (or from"
    echo "your OS vendor's package manager)."
    exit 1
}
. gnome-autogen.sh
