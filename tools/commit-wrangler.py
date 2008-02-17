#!/usr/bin/python
#
# commit-wrangler.py - basic script to commit patches, primarily for
# Metacity, might be useful for others. In early stages of
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

import time
import commands
import sys
import os
import posixpath

# FIXME: Needs tidying into separate functions.

# FIXME: Some of this is duplicated from release-wrangler.
# This should go in a common library when the dust has settled.

# FIXME: Some way of updating Bugzilla when we mention a bug,
# and including the revision number, would be really useful.

# FIXME: This should co-operate with patch-wrangler in that
# p-w should find out the name of a patch's submitter from bugzilla
# and store it in a dotfile, and then we can use it here.
# Also, it should find out and store the bug number, so we can
# write it as "(Bug #nnn)", which will make the previous paragraph's
# idea even more useful.

def get_up_to_date():
  "First step is always to get up to date."
  os.system("svn up")

def report_error(message):
  print message
  sys.exit(255)

def favourite_editor():
  e = os.environ
  if e.has_key('VISUAL'): return e['VISUAL']
  if e.has_key('EDITOR'): return e['EDITOR']
  if os.access('/usr/bin/nano', os.F_OK):
    return '/usr/bin/nano'
  report_error("I can't find an editor for you!")

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

#####################

get_up_to_date()

discoveries = {}

current_file = '?'

diff = commands.getstatusoutput('svn diff --diff-cmd $(which diff) -x -up')[1]

for line in diff.split('\n'):
    if line.startswith('---'):
        current_file = line[4:line.find('\t')]
    elif line.startswith('@@'):
        atatpos = line.find('@@', 3)+3
        discoveries.setdefault(current_file,{})[line[atatpos:line.find(' ',atatpos)]] = 1

# yes, I know this is MY username. I will come back and fix it
# later, but for now there is a lot else to do. FIXME
your_username = 'Thomas Thurman  <tthurman@gnome.org>'

change_filename = posixpath.expanduser("~/.commit-wrangler.txt")
change = open(change_filename, 'w')
change.write('# You are checking in a single changeset.\n')
change.write('# The message below is the one which will be used\n')
change.write('# both in the checkin and the changelog.\n')
change.write('# Any lines starting with a "#" don\'t go in.\n')
change.write('#\n')

change.write('%s  %s\n\n' % (
    time.strftime('%Y-%m-%d',time.gmtime()),
    your_username))

for filename in discoveries:
    change.write(wordwrap('* %s (%s): something' % (
                filename, ', '.join(discoveries[filename])),
                '        ')+'\n')

change.write('\n#\n#\n##############\n#\n#\n#\n')
change.write('# And this is the original diff:\n#\n#')
change.write(diff.replace('\n','\n#'))
change.write('\n#\n#\n##############\n# EOF\n')
change.close()

time_before = os.stat(change_filename)[8]
os.system(favourite_editor()+' +6 %s ' % (change_filename))

if os.stat(change_filename)[8] == time_before:
    print 'No change; aborting.'
    sys.exit(0)

# Update the changelog

changelog_new = open('ChangeLog.tmp', 'w')
changelog_justfunc = open(change_filename+'.justfunc', 'w')
change = open(change_filename, 'r')
for line in change.readlines():
    if not line.startswith('#'):
        changelog_new.write(line)
        changelog_justfunc.write(line)
change.close()
changelog_justfunc.close()

changelog = open('ChangeLog', 'r')
for line in changelog.readlines():
    changelog_new.write(line)

changelog.close()
changelog_new.close()

os.rename('ChangeLog.tmp', 'ChangeLog')

committing = commands.getstatusoutput('svn commit --file %s.justfunc' % (change_filename))[1]

print 'Committed: ', committing
# Ends with message "Committed revision 3573." or whatever;
# this number will be useful in the future for updating
# Bugzilla.
