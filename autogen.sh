#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

REQUIRED_AUTOMAKE_VERSION=1.11

olddir="$(pwd)"

cd "${srcdir}"

(test -f configure.ac \
  && test -d src) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level mutter directory"
    exit 1
}

aclocal --install || exit 1
intltoolize --force --copy --automake || exit 1
autoreconf --verbose --force --install || exit 1

cd "${olddir}"

if [ "$NOCONFIGURE" = "" ]; then
    "${srcdir}/configure" "$@" || exit 1
fi
