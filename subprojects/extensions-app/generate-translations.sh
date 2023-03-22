#!/usr/bin/bash

cd $(dirname $0)

sed -e '/subprojects\/extensions-app/!d' \
    -e 's:subprojects/extensions-app/::' ../../po/POTFILES.in > po/POTFILES.in

for l in $(<po/LINGUAS)
do
  cp ../../po/$l.po po/$l.po
done

builddir=$(mktemp -d -p.)

meson setup $builddir
meson compile -C $builddir gnome-extensions-app-pot
meson compile -C $builddir gnome-extensions-app-update-po

rm -rf $builddir
