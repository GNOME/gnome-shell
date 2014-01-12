#!/usr/bin/python

# Little script to grab a patch, compile it, and make it.

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
import sys
import posixpath

# FIXME: What would be lovely is an Epiphany extension to call this from Bugzilla pages

# FIXME: Should be possible (but currently isn't) to get this from the patch number
program = 'metacity'

if len(sys.argv)<2:
  print 'Specify patch number'
  sys.exit(255)

patchnum = sys.argv[1]

patchdir = posixpath.expanduser('~/patch/%s/%s' % (program, patchnum))

os.makedirs(patchdir)
os.chdir(patchdir)

if os.system("svn co svn+ssh://svn.gnome.org/svn/%s/trunk ." % (program))!=0:
  print "Checkout failed."
  sys.exit(255)

if os.system("wget http://bugzilla.gnome.org/attachment.cgi?id=%s -O - -q|patch -p 0" % (patchnum))!=0:
  print "Patch failed."
  sys.exit(255)

if os.system("./autogen.sh")!=0:
  print "Autogen failed."
  sys.exit(255)

if os.system("make")!=0:
  print "Make failed."
  sys.exit(255)

print
print "It all seems to have worked. Don't look so surprised."
print "Dropping you into a new shell now so you're in the right"
print "directory; this does mean you'll have to exit twice when"
print "you're finished."
print

shell = '/bin/sh'
if os.environ.has_key('SHELL'):
  shell = os.environ['SHELL']

if os.system(shell):
  print "Couldn't launch the shell, though."
  sys.exit(255)
