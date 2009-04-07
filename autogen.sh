#!/bin/sh

(cd `dirname $0`;
    touch ChangeLog NEWS &&
    autoreconf --install --symlink &&
    autoreconf &&
    ./configure --enable-maintainer-mode $@
)
