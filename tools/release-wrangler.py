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
# along with this program; if not, see <http://www.gnu.org/licenses/>.

import os
import posixpath
import re
import sys
import commands
import time
import commands

def report_error(message):
  print message
  sys.exit(255)

def get_up_to_date():
  "First step is always to get up to date."
  os.system("svn up")

# yes, I know this is MY username. I will come back and fix it
# later, but for now there is a lot else to do. FIXME
your_username = 'Thomas Thurman  <tthurman@gnome.org>'

def changelog_and_checkin(filename, message):
  changelog = open('ChangeLog.tmp', 'w')
  changelog.write('%s  %s\n\n        * %s: %s\n\n' % (
    time.strftime('%Y-%m-%d',time.gmtime()),
    your_username,
    filename,
    message))

  for line in open('ChangeLog').readlines():
    changelog.write(line)

  changelog.close()
  os.rename('ChangeLog.tmp', 'ChangeLog')

  if os.system('svn commit -m "%s"' % (message.replace('"','\\"')))!=0:
    report_error("Could not commit; bailing.")

def check_we_are_up_to_date():
  changed = []
  for line in commands.getoutput('/usr/bin/svn status').split('\n'):
    if line!='' and (line[0]=='C' or line[0]=='M'):
      if line.find('release-wrangler.py')==-1 and line.find('ChangeLog')==-1:
        # we should be insensitive to changes in this script itself
        # to avoid chicken-and-egg problems
        changed.append(line[1:].lstrip())

  if changed:
    report_error('These files are out of date; I can\'t continue until you fix them: ' + \
      ', '.join(changed))

def version_numbers():
  # FIXME: This is all very metacity-specific. Compare fusa, etc
  """Okay, read through configure.in and find who and where we are.

  We also try to figure out where the next micro version number
  will be; some programs (e.g. Metacity) use a custom numbering
  scheme, and if they have a list of numbers on the line before the
  micro version then that will be used. Otherwise we will just
  increment."""

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
              version_index = versions.index(version_value)+1

              if versions[version_index] == version['micro']:
                # work around metacity giving "1" twice
                version_index += 1

              version['micro_next'] = versions[version_index]
            except:
              report_error("You gave a list of micro version numbers, but we've used them up!")
          else:
            report_error("You gave a list of micro version numbers, but the current one wasn't in it! Current is %s and your list is %s" % (
              `version_value`, `versions`))

    previous_line = line

  if not 'micro_next' in version:
    version['micro_next'] = version['micro']+1

  version['string'] = '%(major)s.%(minor)s.%(micro)s' % (version)
  version['filename'] = '%(name)s-%(string)s.tar.gz' % (version)

  return version

def check_file_does_not_exist(version):
  if os.access(version['filename'], os.F_OK):
    report_error("Sorry, you already have a file called %s! Please delete it or move it first." % (version['filename']))

def is_date(str):
  return len(str)>4 and str[4]=='-'

def scan_changelog(version):
  changelog = file("ChangeLog").readlines()

  # Find the most recent release.

  release_date = None

  for line in changelog:
    if is_date(line):
      release_date = line[:10]

    if "Post-release bump to" in line:
      changelog = changelog[:changelog.index(line)+1]
      break

  contributors = {}
  thanks = ''
  entries = []

  def assumed_surname(name):
    if name=='': return ''
    # might get more complicated later, but for now...
    return name.split()[-1]

  def assumed_forename(name):
    if name=='': return ''
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

  # FIXME: getting complex enough we should be returning a dictionary
  return (contributors, changelog, entries, release_date)

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

def favourite_editor():
  e = os.environ
  if e.has_key('VISUAL'): return e['VISUAL']
  if e.has_key('EDITOR'): return e['EDITOR']
  if os.access('/usr/bin/nano', os.F_OK):
    return '/usr/bin/nano'
  report_error("I can't find an editor for you!")

def edit_news_entry(version):

  # FIXME: still needs a lot of tidying up. Translator stuff especially needs to be
  # factored out into a separate function.

  (contributors, changelog, entries, release_date) = scan_changelog(version)

  contributors_list = contributors.keys()
  contributors_list.sort()
  thanksline = ', '.join([contributors[x] for x in contributors_list])
  thanksline = thanksline.replace(contributors[contributors_list[-1]], 'and '+contributors[contributors_list[-1]])

  thanks = '%s\n%s\n\n' % (version['string'], '='*len(version['string']))
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

    if ',' in language:
      language = language[:language.find(',')].replace('.po','')

    filename = 'po/%s.po' % (language)

    if not os.access(filename, os.F_OK):
      # Never mind the translator being unknown, we don't even
      # know about the language!
      return 'Mystery translator (%s)'  % (language)

    for line in file(filename).readlines():
      match = last_translator_re.search(line)
      if match:
        name = match.group(1).rstrip().lstrip()
        break

    return "%s (%s)" % (name, language)

  thanks += '\nTranslations\n'
  thanks += wordwrap(', '.join([translator_name(x) for x in translator_list]), '  ')
  thanks += '\n\n'

  changes = '## '+ ' '.join(changelog).replace('\n', '\n## ')

  filename = posixpath.expanduser("~/.release-wrangler-%(name)s-%(string)s.txt" % version)
  tmp = open(filename, 'w')
  tmp.write('## You are releasing %(name)s, version %(major)s.%(minor)s.%(micro)s.\n' % version)
  tmp.write('## The text at the foot of the page is the part of the ChangeLog which\n')
  tmp.write('## has changed since the last release. Please summarise it.\n')
  tmp.write('## Anything preceded by a # is ignored.\n')
  tmp.write(thanks)
  tmp.write(changes)
  tmp.close()

  os.system(favourite_editor()+' +6 %s ' % (filename))
  # FIXME: if they abort, would be useful to abort here too

  # Write it out to NEWS

  version['announcement'] = ''

  news_tmp = open('NEWS.tmp', 'a')
  for line in open(filename, 'r').readlines():
    if line=='' or line[0]!='#':
      news_tmp.write(line)
      version['announcement'] += line

  for line in open('NEWS').readlines():
    news_tmp.write(line)

  news_tmp.close()

  os.rename('NEWS.tmp', 'NEWS')
  changelog_and_checkin('NEWS', '%(major)s.%(minor)s.%(micro)s release.' % (version))

def build_it_all(version):
  "Now build the thing."
  autogen_prefix= '/prefix' # FIXME: this is specific to tthurman's laptop!

  # FIXME: These should use os.system

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

  if not os.access(version['filename'], os.F_OK):
    print "Sorry, we don't appear to have a file called %s!" % (archive_filename)
    sys.exit(255)

def upload(version):
  # No, we won't have a configuration option to set your name on master.g.o; that's
  # what ~/.ssh/config is for.

  print "Uploading..."
  upload_result = commands.getstatusoutput('scp %s master.gnome.org:' % (version['filename']))

  if upload_result[0]!=0:
    report_error("There appears to have been an uploading problem: %d\n%s\n" % (upload_result[0], upload_result[1]))

def increment_version(version):
  configure_in = file('configure.in.tmp', 'w')
  for line in file('configure.in'):
    if re.search("^m4_define\(\[.*_micro_version\], \[(\d+)\]", line):
      line = line.replace('[%(micro)s]' % version, '[%(micro_next)s]' % version)
    configure_in.write(line)
  
  configure_in.close()
  os.rename('configure.in.tmp', 'configure.in')

  changelog_and_checkin('configure.in', 'Post-release bump to %(major)s.%(minor)s.%(micro_next)s.' % version)

def tag_the_release(version):
  version['ucname'] = version['name'].upper()
  if os.system("svn cp -m release . svn+ssh://svn.gnome.org/svn/%(name)s/tags/%(ucname)s_%(major)s_%(minor)s_%(micro)s" % (version))!=0:
    report_error("Could not tag; bailing.")

def md5s(version):
  return commands.getstatusoutput('ssh master.gnome.org "cd /ftp/pub/GNOME/sources/%(name)s/%(major)s.%(minor)s/;md5sum $(name)s-%(major)s.%(minor)s.%(micro)s.tar*"' % (version))

def main():
  get_up_to_date()
  check_we_are_up_to_date()
  version = version_numbers()
  check_file_does_not_exist(version)
  edit_news_entry(version)
  build_it_all(version)
  tag_the_release(version)
  increment_version(version)
  upload(version)
  print version['announcement']
  print "-- Done --"

if __name__=='__main__':
  main()

