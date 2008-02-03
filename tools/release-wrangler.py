#!/usr/bin/python
#
# release-wrangler.py - very basic release system, primarily for
# Metacity, might be useful for others. In very early stages of
# development.
#
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

# This script doesn't do all the work yet, but it will soon.

import os
import posixpath
import re
import sys
import commands

# First step is always to get up to date.
os.system("svn up")

################################################################

# Are we up to date now?

changed = []
for line in commands.getoutput('/usr/bin/svn status').split('\n'):
  if line!='' and (line[0]=='C' or line[0]=='M'):
    changed.append(line[1:].lstrip())

if changed:
  print 'These files are out of date; I can\'t continue until you fix them.'
  print ', '.join(changed)
  sys.exit(255)

################################################################

# FIXME: This is all very metacity-specific. Compare fusa, etc
#
# Okay, read through configure.in and find who and where we are.
#
# We also try to figure out where the next micro version number
# will be; some programs (e.g. Metacity) use a custom numbering
# scheme, and if they have a list of numbers on the line before the
# micro version then that will be used. Otherwise we will just
# increment.
version = {}
previous_line = ''
for line in file("configure.in").readlines():
  product_name = re.search("^AC_INIT\(\[([^\]]*)\]", line)
  if product_name:
    version['name'] = product_name.group(1)

  version_number = re.search("^m4_define\(\[.*_(.*)_version\], \[(\d+)\]", line)

  if version_number:
    version_type = version_number.group(1)
    version_value = int(version_number.group(2))

    version[version_type] = version_value

    if version_type == 'micro':
      group_of_digits = re.search("^\#\s*([0-9, ]+)\n$", previous_line)
      if group_of_digits:
        versions = [int(x) for x in group_of_digits.group(1).split(',')]

        if version_value in versions:
          try:
            version['micro_next'] = versions[versions.index(version_value)+1]
          except:
            print "You gave a list of micro version numbers, but we've used them up!"
            sys.exit(255)
        else:
          print "You gave a list of micro version numbers, but the current one wasn't in it!"
          print "Current is ",version_value
          print "Your list is ",versions
          sys.exit(255)

  previous_line = line

if not 'micro_next' in version:
  version['micro_next'] = version['micro']+1

################################################################

archive_filename = '%(name)s-%(major)s.%(minor)s.%(micro)s.tar.gz' % (version)
if os.access(archive_filename, os.F_OK):
  print "Sorry, you already have a file called %s! Please delete it or move it first." % (archive_filename)
  sys.exit(255)

################################################################

changelog = file("ChangeLog").readlines()

# Find the most recent release.

def is_date(str):
  return len(str)>3 and str[4]=='-'

release_date = None

for line in changelog:
  if is_date(line):
    release_date = line[:10]
  if "Post-release bump to %s.%s.%s." % (version['major'], version['minor'], version['micro']) in line:
    changelog = changelog[:changelog.index(line)+1]
    break

contributors = {}
thanks = ''
entries = []

def assumed_surname(name):
  # might get more complicated later, but for now...
  return name.split()[-1]

def assumed_forename(name):
  return name.split()[0]

bug_re = re.compile('bug \#?(\d+)', re.IGNORECASE)
hash_re = re.compile('\#(\d+)')

for line in changelog:
  if is_date(line):
    line = line[10:].lstrip()
    line = line[:line.find('<')].rstrip()
    contributors[assumed_surname(line)] = line
    entries.append('(%s)' % (assumed_forename(line)))
  else:
    match = bug_re.search(line)
    if not match: match = hash_re.search(line)
    if match:
      entries[-1] += ' (#%s)' % (match.group(1))

contributors_list = contributors.keys()
contributors_list.sort()
thanksline = ', '.join([contributors[x] for x in contributors_list])
thanksline = thanksline.replace(contributors[contributors_list[-1]], 'and '+contributors[contributors_list[-1]])

version_string = '%(major)s.%(minor)s.%(micro)s' % (version)

def wordwrap(str, prefix=''):
  "Really simple wordwrap"

  # Ugly hack:
  # We know that all open brackets are preceded by spaces.
  # We don't want to split on these spaces. Therefore:
  str = str.replace(' (','(')

  result = ['']
  for word in str.split():

    if result[-1]=='':
      candidate = prefix + word
    else:
      candidate = '%s %s' % (result[-1], word)

    if len(candidate)>80:
      result.append(prefix+word)
    else:
      result[-1] = candidate

  return '\n'.join(result).replace('(',' (')

thanks = '%s\n%s\n\n' % (version_string, '='*len(version_string))
thanks += wordwrap('Thanks to %s for improvements in this version.' % (thanksline))
thanks += '\n\n'
for line in entries:
  thanks += '  - xxx %s\n' % (line)

# and now pick up the translations.

translations = {}
language_re = re.compile('\*\s*(.+)\.po')

for line in file("po/ChangeLog").readlines():
  match = language_re.search(line)
  if match:
    translations[match.group(1)] = 1
  if is_date(line) and line[:10]<release_date:
    break

translator_list = translations.keys()
translator_list.sort()

last_translator_re = re.compile('Last-Translator:([^<"]*)', re.IGNORECASE)

def translator_name(language):
  name = 'unknown'
  for line in file('po/%s.po' % (language)).readlines():
    match = last_translator_re.search(line)
    if match:
      name = match.group(1).rstrip().lstrip()
      break
  return "%s (%s)" % (name, language)

thanks += '\nTranslations\n'
thanks += wordwrap(', '.join([translator_name(x) for x in translator_list]), '  ')
thanks += '\n\n'

changes = '## '+ ' '.join(changelog).replace('\n', '\n## ')

filename = posixpath.expanduser("~/.release-wrangler-%(name)s-%(major)s-%(minor)s-%(micro)s.txt" % version)
tmp = open(filename, 'w')
tmp.write('## You are releasing %(name)s, version %(major)s.%(minor)s.%(micro)s.\n' % version)
tmp.write('## The text at the foot of the page is the part of the ChangeLog which\n')
tmp.write('## has changed since the last release. Please summarise it.\n')
tmp.write('## Anything preceded by a # is ignored.\n')
tmp.write(thanks)
tmp.write(changes)
tmp.close()

os.spawnl(os.P_WAIT, '/bin/nano', 'nano', '+6', filename)

################################################################

# Write it out to NEWS

news_tmp = open('NEWS.tmp', 'a')
for line in open(filename, 'r').readlines():
  if line=='' or line[0]!='#':
    news_tmp.write(line)

for line in open('NEWS').readlines():
  news_tmp.write(line)

news_tmp.close()

os.rename('NEWS.tmp', 'NEWS')

################################################################

# Now build the thing.

autogen_prefix= '/prefix' # FIXME: this is specific to tthurman's laptop!

if os.spawnl(os.P_WAIT, './autogen.sh', './autogen.sh', '--prefix', autogen_prefix) != 0:
    print 'autogen failed'
    sys.exit(255)
    
if os.spawnl(os.P_WAIT, '/usr/bin/make', '/usr/bin/make') != 0:
    print 'make failed'
    sys.exit(255)

if os.spawnl(os.P_WAIT, '/usr/bin/make', '/usr/bin/make', 'install') != 0:
    print 'install failed'
    sys.exit(255)

if os.spawnl(os.P_WAIT, '/usr/bin/make', '/usr/bin/make', 'distcheck') != 0:
    print 'distcheck failed'
    sys.exit(255)

if not os.access(archive_filename, os.F_OK):
  print "Sorry, we don't appear to have a file called %s!" % (archive_filename)
  sys.exit(255)

# No, we won't have a configuration option to set your name on svn.g.o; that's
# what ~/.ssh/config is for.

print "Uploading..."
upload_result = commands.getstatusoutput('scp %s master.gnome.org:' % (archive_filename))

if upload_result[0]!=0:
  print "There appears to have been an uploading problem: %d\n%s\n" % (upload_result[0], upload_result[1])
