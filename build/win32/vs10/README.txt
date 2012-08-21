Note that all this is rather experimental.

A more detailed description on using Visual C++ to compile Clutter with
its dependencies can be found on the following GNOME Live! page:

https://live.gnome.org/GTK%2B/Win32/MSVCCompilationOfGTKStack

Please do not attempt to compile Clutter in a path that contains spaces
to avoid potential problems during compilation, linking or usage.

This VS10 solution and the projects it includes are intented to be used
in a Clutter source tree unpacked from a tarball. In a git checkout you
first need to use some Unix-like environment or manual work to expand
the files needed, like config.h.win32.in into config.h.win32 and the
.vcprojxin and .vcxproj.filtersin files here into corresponding actual
.vcxproj and .vcxproj.filters files.

You will need the parts from GNOME: Cogl, JSON-GLib, GDK-Pixbuf,
Pango*, atk and GLib. External dependencies are at least zlib, libpng,
gettext-runtime* and Cairo*, and glext.h from
http://www.opengl.org/registry/api/glext.h (which need to be in the GL folder
in your include directories or in <root>\vs10\<PlatformName>\include\GL).

Please see the README file in the root directory of this Clutter source package
for the versions of the dependencies required.  See also
build/win32/vs10/README.txt in the GLib source package for details
where to unpack them.  It is recommended that at least the dependencies
from GNOME are also built with VS10 to avoid crashes caused by mixing different
CRTs-please see also the build/win32/vs10/README.txt in those respective packages.

The recommended build sequence of the dependencies are as follows (the non-GNOME
packages that are not downloaded as binaries from ftp://ftp.gnome.org have
makefiles and/or VS project files that can be used to compile with VS directly,
except the optional PCRE, which is built on VS using CMake; GLib & ATK have
VS10 project files in the latest stable versions, GDK-Pixbuf have VS10 project files
in the latest unstable version, and Cogl, JSON-GLib and Pango should have VS10 project
files in the next unstable release):
-Unzip the binary packages for gettext-runtime, freetype, expat and fontconfig
 downloaded from ftp://ftp.gnome.org*
-zlib
-libpng
-(optional for GLib) PCRE (8.12 or later, building PCRE using CMake is
 recommended-please see build/win32/vs10/README.txt in the GLib source package)
-(for gdk-pixbuf, if GDI+ is not to be used) IJG JPEG
-(for gdk-pixbuf, if GDI+ is not to be used) jasper [JPEG-2000 library]
-(for gdk-pixbuf, if GDI+ is not to be used, requires zlib and IJG JPEG) libtiff
-GLib
-Cairo
-Pango
-ATK
-GDK-Pixbuf
-JSON-GLib
-Cogl
 (Note that Pango, ATK, GDK-Pixbuf and JSON-GLib are not dependent on each
 other, so building them in any order will do)

Note that a working PERL installation (such as ActiveState or Strawberry PERL)
is also required for compilation of Clutter, as the glib-mkenums PERL utility
script available from the GLib package will be used.  Please ensure that the
PERL interpretor is in your PATH.  If a glib-mkenums script is not available,
please extract glib-mkenums.in from the gobject subdirectory of the
GLib source package, and change the lines and save as glib-mkenums in
<root>\vs10\<PlatformName>\bin:

#! @PERL_PATH@ (circa line 1)
to
#! c:/perl/bin (or whereever your PERL interpretor executable is)

-and-

print "glib-mkenums version glib-@GLIB_VERSION@\n";
to
print "glib-mkenums version glib-<your GLib version>\n";

The "install" project will copy build results and headers into their
appropriate location under <root>\vs10\<PlatformName>. For instance,
built DLLs go into <root>\vs10\<PlatformName>\bin, built LIBs into
<root>\vs10\<PlatformName>\lib and Clutter headers into
<root>\vs10\<PlatformName>\include\clutter-1.0.
 
*There is no known official VS10 build support for fontconfig
 (required for Pango and Pango at the moment-I will see whether this
 requirement can be made optional for VS builds)
 (along with freetype and expat) and gettext-runtime, so
 please use the binaries from: 

 ftp://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/ (32 bit)
 ftp://ftp.gnome.org/pub/GNOME/binaries/win64/dependencies/ (64 bit)

Note: If you see C4819 warnings and you are compiling Clutter on a DBCS
(Chinese/Korean/Japanese) version of Windows, you may need to switch
to an English locale in Control Panel->Region and Languages->System->
Change System Locale, reboot and rebuild to ensure Clutter and its
dependencies are built correctly. This is due to a bug in Visual C++
running on DBCS locales.

--Chun-wei Fan <fanc999@yahoo.com.tw>
  (Adopted from the GTK+ Win32 VS README.txt file originally by Tor Lillqvist)
