import commands
import xml.dom.minidom
import glob
import wordpresslib # http://www.blackbirdblog.it/programmazione/progetti/28
import ConfigParser
import os
import re

doaps = glob.glob("*.doap")

if len(doaps)==0:
    print 'Please run this from the top-level directory.'

description=str(xml.dom.minidom.parse(doaps[0]).getElementsByTagName('shortdesc')[0].firstChild.toxml().strip())

program_name = doaps[0][:-5]
print program_name

markup = {
    'text': {
        'open': '  *',
        'newline': '   ',
        'close': '',
        },
    'html': {
        'open': '<li>',
        'newline': '',
        'close': '</li>',
        },
}

def text_list(list, type):
    result = []
    for entry in list:
        result.append(markup[type]['open'])
        for word in entry.split():
            if len(result[-1]+word)>75:
                result.append(markup[type]['newline'])
            result[-1] = result[-1] + ' ' + word
        if result[-1].strip()=='':
            result = result[:-1]
        result[-1] = result[-1] + markup[type]['close']
    return '\n'.join(result)

news = file('NEWS')
news_entry = []
header_count = 0

while header_count<2:
    line = news.readline().replace('\n', '')
    news_entry.append(line)
    if line.startswith('='):
        header_count = header_count + 1

news.close()

version = news_entry[0]
news_entry = news_entry[2:-2]

print version
majorminor = '.'.join(version.split('.')[:2])
md5s = commands.getoutput('ssh master.gnome.org md5sum /ftp/pub/GNOME/sources/metacity/%s/%s-%s.tar*' % (majorminor, program_name, version)).split()
if len(md5s)!=4:
    print 'WARNING: There were not two known tarballs'

md5_values = {}

for i in range(0, len(md5s), 2):
    a = md5s[i+1]
    md5_values[a[a.rindex('.'):]] = md5s[i]

print md5_values

changes = []
translations = ''

reading_changes = False
reading_translations = False
for line in news_entry:
    line = line.replace('(#', '(GNOME bug ').strip()
    if line.startswith('-'):
        changes.append(line[2:])
        reading_changes = True
    elif reading_changes:
        if line=='':
            reading_changes = False
        else:
            changes[-1] = changes[-1] + ' ' + line
    elif line=='Translations':
        reading_translations = True
    elif reading_translations:
        translations = translations + ' ' + line

translations = translations[1:]

text_links = []
for i in ('.bz2', '.gz'):
    text_links.append('%s http://download.gnome.org/sources/metacity/%s/%s-%s.tar%s' % (
            md5_values[i], majorminor, program_name, version, i))

text_version = """\
What is it ?
============
%s

What's changed ?
================
%s

Translations:
%s

Where can I get it ?
====================
%s""" % (text_list([description], 'text'),
                text_list(changes, 'text'),
                text_list([translations], 'text'),
                text_list(text_links, 'text'))

print "============ x8 x8 x8 ===== SEND THIS TO gnome-announce-list"
print text_version
print "============ x8 x8 x8 ===== ENDS"

translations = re.sub('\((.*)\)',
                      '(<a href="http://svn.gnome.org/viewvc/metacity/trunk/po/\\1.po">\\1</a>)',
                      translations)

html_version = """\
<b>What is it ?</b><br />
<ul>%s</ul>

<b>What's changed ?</b><br />
<ul>%s</ul>

<i>Translations:</i><br />
<ul>%s</ul>

<b>Where can I get it ?</b><br />
<ul>%s</ul>""" % (text_list([description], 'html'),
                text_list(changes, 'html'),
                text_list([translations], 'html'),
                text_list(text_links, 'html'))

cp = ConfigParser.ConfigParser()
cp.read(os.environ['HOME']+'/.config/metacity/tools.ini')

wp = wordpresslib.WordPressClient(
    cp.get('release-wrangler', 'blogurl'),
    cp.get('release-wrangler', 'bloguser'),
    cp.get('release-wrangler', 'blogpass'))
wp.selectBlog(cp.getint('release-wrangler', 'blognumber'))
post = wordpresslib.WordPressPost()
post.title = '%s %s released' % (program_name, version)
post.description = html_version
# appears to have been turned off-- ask jdub
#idPost = wp.newPost(post, False)

print html_version

