#! /bin/sh
gtkdocize || exit 1
autoreconf -v --install || exit 1
./configure --enable-maintainer-mode "$@"
