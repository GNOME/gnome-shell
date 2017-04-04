#!/usr/bin/sh

srcdir=`dirname $0`
for scss in $srcdir/*.scss
do
  sassc -a $scss ${scss%%.scss}.css
done
