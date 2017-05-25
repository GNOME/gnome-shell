#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

olddir="$(pwd)"

cd "${srcdir}"

(test -f configure.ac \
  && test -d src) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome-shell directory"
    exit 1
}

# Fetch submodules if needed
if test ! -f subprojects/gvc/Makefile.am || test ! -f data/theme/gnome-shell-sass/COPYING;
then
  echo "+ Setting up submodules"
  git submodule init
fi
git submodule update

aclocal --install || exit 1
gtkdocize --copy || exit 1
intltoolize --force --copy --automake || exit 1
autoreconf --verbose --force --install || exit 1

cd "${olddir}"

if [ "$NOCONFIGURE" = "" ]; then
    "${srcdir}/configure" "$@" || exit 1
fi
