#!/usr/bin/python
# Copyright (C) 2008 Thomas Thurman
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.


import os
import xml.sax

standard = ['x', 'y', 'width', 'height']

expressions = {
  'line': ['x1', 'x2', 'y1', 'y2'],
  'rectangle': standard,
  'arc': standard,
  'clip': standard,
  'gradient': standard,
  'image': standard,
  'gtk_arrow': standard,
  'gtk_box': standard,
  'gtk_vline': standard,
  'icon': standard,
  'title': standard,
  'include': standard,
  'tile': ['x', 'y', 'width', 'height',
           'tile_xoffset', 'tile_yoffset',
           'tile_width', 'tile_height'],
}

all_themes = '../../../all-themes/'

result = {}

class themeparser:
  def __init__(self, name):
    self.filename = name

  def processingInstruction(self):
    pass

  def characters(self, what):
    pass

  def setDocumentLocator(self, where):
    pass

  def startDocument(self):
    pass

  def startElement(self, name, attrs):
    if expressions.has_key(name):
      for attr in expressions[name]:
        if attrs.has_key(attr):
          expression = attrs[attr]
          if not result.has_key(expression): result[expression] = {}
          result[expression][self.filename] = 1

  def endElement(self, name):
    pass # print "end element"

  def endDocument(self):
    pass

def maybe_parse(themename, filename):
  if os.access(all_themes+filename, os.F_OK):
    parser = themeparser(themename)
    xml.sax.parse(all_themes+filename, parser)
  
for theme in os.listdir(all_themes):
  maybe_parse(theme, theme+'/metacity-1/metacity-theme-1.xml')
  maybe_parse(theme, theme+'/metacity-theme-1.xml')

print '[tokentest0]'

for expr in sorted(result.keys()):
  print "# %s" % (', '.join(sorted(result[expr])))
  print "%s=REQ" % (expr)
  print
