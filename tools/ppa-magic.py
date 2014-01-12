#!/usr/bin/python 
#
# This is a heavily experimental script to upload nightly snapshots
# to Canonical's Launchpad PPA system.
#
# Copyright (C) 2008 Thomas Thurman <tthurman@gnome.org>
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

import time
import os

app = 'metacity'
try_number = 1 # if you mess it up within a day
upstream_version = '2.25.0' # should take this from configure.in, really
version = '1:%s~%s-0ubuntu~ppa%d' % (upstream_version, 
                                     time.strftime('%Y%m%d'),
                                     try_number)

pkg_name = app # according to motu people
svn_url = 'http://svn.gnome.org/svn/'+app+'/trunk'
maintainer = 'Thomas Thurman <tthurman@gnome.org>'
key = 'D5743F03'
basedir = os.getcwd()+'/'+pkg_name # or, if you prefer, '/tmp/'+pkg_name

def write_to_files(path):
	
	file(path+'/changelog', 'w').write(\
pkg_name+""" ("""+version+""") hardy; urgency=low

   * Nightly release from trunk.

 -- """+maintainer+'  '+time.strftime("%a, %d %b %Y %H:%M:%S %z")+"""
""")

	file(path+'/rules', 'w').write(\
"""#!/usr/bin/make -f
include /usr/share/cdbs/1/rules/debhelper.mk
include /usr/share/cdbs/1/class/gnome.mk
""")

	os.chmod(path+'/rules', 0777)

	# Compat should be 6 to keep debhelper happy.
	file(path+'/compat', 'w').write(\
"""6
""")

	file(path+'/control', 'w').write(\
"""Source: """+pkg_name+"""
Section: devel
Priority: optional
Maintainer: """+maintainer+"""
Standards-Version: 3.8.0
Build-Depends: cdbs (>= 0.4.41),
               debhelper (>= 5),
               gettext,
               libgtk2.0-dev (>= 2.10.0-1~),
               liborbit2-dev (>= 1:2.10.2-1.1~),
               libpopt-dev,
               libxml2-dev (>= 2.4.23),
               libgconf2-dev (>= 2.6.1-2),
               libglade2-dev (>= 2.4.0-1~),
               libice-dev,
               libsm-dev,
               libx11-dev,
               libxt-dev,
               libxext-dev,
               libxrandr-dev,
               x11proto-core-dev,
               libxinerama-dev,
               libstartup-notification0-dev (>= 0.7),
               libxml-parser-perl,
               gnome-pkg-tools (>= 0.10),
               dpkg-dev (>= 1.13.19),
               libxcomposite-dev
Homepage: http://blogs.gnome.org/metacity/

Package: """+pkg_name+"""
Architecture: any
Depends: ${shlibs:Depends}
Description: Lightweight GTK2 compositing window manager (nightly trunk)
 Metacity is a small window manager, using gtk2 to do everything.
 .
 As the author says, metacity is a "Boring window manager for the adult in
 you. Many window managers are like Marshmallow Froot Loops; Metacity is
 like Cheerios."
 .
 This is the nightly trunk upload.  It may not be the epitome of stability.
""")

	file(path+'/copyright', 'w').write(\
"""This package was automatically debianised by a script.

It was downloaded from """+svn_url+"""

	Upstream Author and Copyright Holder: Havoc Pennington - hp@redhat.com
and others.

License:

   This package is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This package is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this package; if not, see <http://www.gnu.org/licenses/>.

On Debian systems, the complete text of the GNU General
Public License can be found in `/usr/share/common-licenses/GPL'.
""")

#######################

if basedir!='.' and basedir!='..' and os.access(basedir, os.F_OK):
	os.system('rm -rf '+basedir)
	print 'Warning: deleted old version of '+basedir+'.'

os.system('svn export -q '+svn_url+' '+basedir)
os.mkdir(basedir+'/debian')

write_to_files(basedir+'/debian')

os.chdir(basedir)

# Make sure we get up to having a "configure", or it won't build.
os.system('NOCONFIGURE=1 ./autogen.sh')

os.chdir(basedir+'/debian')

os.system('debuild -rfakeroot -S -k'+key)

os.system('dput -f '+pkg_name+' '+basedir+'_'+version[2:]+'_source.changes')

# And then we should clean up.

