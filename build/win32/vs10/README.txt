Note that all this is rather experimental.

A more detailed description on using Visual C++ to compile COGL with
its dependencies can be found on the following GNOME Live! page:

https://live.gnome.org/GTK%2B/Win32/MSVCCompilationOfGTKStack

Please do not attempt to compile COGL in a path that contains spaces
to avoid potential problems during compilation, linking or usage.

This VS10 solution and the projects it includes are intented to be used
in a Cogl source tree unpacked from a tarball. In a git checkout you
first need to use some Unix-like environment or manual work to expand
the files needed, like config.h.win32.in into config.h.win32 and the
.vcprojin files here into corresponding actual .vcproj files.

You will need the parts from GNOME: GDK-Pixbuf, Pango* and GLib.
External dependencies are at least zlib, libpng,
gettext-runtime* and Cairo*, and glext.h from
http://www.opengl.org/registry/api/glext.h (which need to be in the GL folder
in your include directories or in <root>\vs10\<PlatformName>\include\GL).

Please see the README file in the root directory of this Cogl source package
for the versions of the dependencies required.  See also
build/win32/vs10/README.txt in the GLib source package for details
where to unpack them.  It is recommended that at least the dependencies
from GNOME are also built with VS10 to avoid crashes caused by mixing different
CRTs-please see also the build/win32/vs10/README.txt in those respective packages.

If building the SDL winsys is desired, you will also need the SDL libraries
from www.libsdl.org-building the SDL source package with Visual C++ 2010
is recommended (working Visual C++ 2005 projects are included with it, upgrade
the projects one prompted), but one may want to use the VC8 binary packages
from that website.

The recommended build sequence of the dependencies are as follows (the non-GNOME
packages that are not downloaded as binaries from ftp://ftp.gnome.org have
makefiles and/or VS project files that can be used to compile with VS directly,
except the optional PCRE, which is built on VS using CMake; GLib has
VS10 project files in the latest stable versions, GDK-Pixbuf have VS10 project files
in the latest unstable version, and Pango should have VS10 project files
in the next unstable release):
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
-GDK-Pixbuf

The "install" project will copy build results and headers into their
appropriate location under <root>\vs10\<PlatformName>. For instance,
built DLLs go into <root>\vs10\<PlatformName>\bin, built LIBs into
<root>\vs10\<PlatformName>\lib and Cogl headers into
<root>\vs10\<PlatformName>\include\Cogl-2.0.
 
*There is no known official VS10 build support for fontconfig
 (required for Pango and Pango at the moment-I will see whether this
 requirement can be made optional for VS builds)
 (along with freetype and expat) and gettext-runtime, so
  please use the binaries from: 

 ftp://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/ (32 bit)
 ftp://ftp.gnome.org/pub/GNOME/binaries/win64/dependencies/ (64 bit)

Note: If you see C4819 warnings and you are compiling Cogl on a DBCS
(Chinese/Korean/Japanese) version of Windows, you may need to switch
to an English locale in Control Panel->Region and Languages->System->
Change System Locale, reboot and rebuild to ensure Cogl and its
dependencies are built correctly.  This is due to a bug in Visual C++
running on DBCS locales.

--Chun-wei Fan <fanc999@yahoo.com.tw>
  (Adopted from the GTK+ Win32 VS README.txt file originally by Tor Lillqvist)
