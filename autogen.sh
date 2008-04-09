#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PROJECT=Clutter
TEST_TYPE=-d
FILE=clutter

test $TEST_TYPE $FILE || {
        echo "You must run this script in the top-level $PROJECT directory"
        exit 1
}

gtkdocize || exit $?

# back in the stupidity of autoreconf
autoreconf -v --install || exit $?

./configure "$@" ${GTK_DOC_ARGS}

echo "Now type 'make' to compile $PROJECT."
