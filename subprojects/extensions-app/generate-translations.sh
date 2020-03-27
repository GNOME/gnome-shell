#!/usr/bin/bash

cd $(dirname $0)

sed -e '/subprojects\/extensions-app/!d' \
    -e 's:subprojects/extensions-app/::' ../../po/POTFILES.in > po/POTFILES.in

for l in $(<po/LINGUAS)
do
  cp ../../po/$l.po po/$l.po
done

builddir=$(mktemp -d -p.)

meson $builddir
ninja -C $builddir gnome-extensions-app-pot
ninja -C $builddir gnome-extensions-app-update-po

rm -rf $builddir
