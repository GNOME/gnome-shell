#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`

cd $srcdir
PROJECT=Clutter
TEST_TYPE=-f
FILE=clutter/clutter.h

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

(gtkdocize --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have gtk-doc installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnome.org/pub/GNOME/sources/gtk-doc/"
        exit 1
}

# NOCONFIGURE is used by gnome-common
if test -z "$NOCONFIGURE"; then
        if test -z "$*"; then
                echo "I am going to run ./configure with no arguments - if you wish "
                echo "to pass any to it, please specify them on the $0 command line."
        fi
fi

rm -rf autom4te.cache

gtkdocize || exit $?
autoreconf -vfi || exit $?
cd $ORIGDIR || exit $?

if test -z "$NOCONFIGURE"; then
        $srcdir/configure $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?
        echo "Now type 'make' to compile $PROJECT."
fi
