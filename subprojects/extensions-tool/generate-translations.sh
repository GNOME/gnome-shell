#!/usr/bin/bash

cd $(dirname $0)

sed -e '/subprojects\/extensions-tool/!d' \
    -e 's:subprojects/extensions-tool/::' ../../po/POTFILES.in > po/POTFILES.in

for l in $(<po/LINGUAS)
do
  cp ../../po/$l.po po/$l.po
done

builddir=$(mktemp -d -p.)

meson -Dman=False $builddir
ninja -C $builddir gnome-extensions-tool-pot
ninja -C $builddir gnome-extensions-tool-update-po

rm -rf $builddir
