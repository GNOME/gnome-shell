#! /bin/sh
gtkdocize || exit 1

# back in the stupidity of autoreconf
touch README
autoreconf -v --install || exit 1
rm -f README

./configure "$@"
