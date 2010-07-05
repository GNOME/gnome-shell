#!/usr/bin/python
import re
import sys

# gobject-introspection currently has a bug where an alias like
# 'typedef GdkRectangle cairo_rect_int_t' is stored un-namespaced,
# so it is taken to apply to all *Rectangle types. Fixing this
# requires a significant rework of g-ir-scanner, so for the moment
# we fix up the output using this script.
#
# https://bugzilla.gnome.org/show_bug.cgi?id=622609

GDK_RECTANGLE = re.compile(r'Gdk\.Rectangle')
META_RECTANGLE = re.compile(r'MetaRectangle')

i = open(sys.argv[1], 'r')
o = open(sys.argv[2], 'w')
for line in i:
    if GDK_RECTANGLE.search(line) and META_RECTANGLE.search(line):
        line = re.sub('Gdk.Rectangle', 'Rectangle', line)
    o.write(line)
