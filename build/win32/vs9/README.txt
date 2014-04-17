Note that all this is rather experimental.

A more detailed description on using Visual C++ to compile COGL with
its dependencies can be found on the following GNOME Live! page:

https://live.gnome.org/GTK%2B/Win32/MSVCCompilationOfGTKStack

Please do not attempt to compile Cogl in a path that contains spaces
to avoid potential problems during compilation, linking or usage.

This VS9 solution and the projects it includes are intented to be used
in a Cogl source tree unpacked from a tarball. In a git checkout you
first need to use some Unix-like environment or manual work to expand
the files needed, like config.h.win32.in into config.h.win32 and the
.vcprojin files here into corresponding actual .vcproj files.

You will need the parts from GNOME: GDK-Pixbuf, Pango* and GLib.
External dependencies are at least zlib, libpng,
gettext-runtime* and Cairo*, and glext.h from
http://www.opengl.org/registry/api/glext.h (which need to be in the GL folder
in your include directories or in <root>\vs9\<PlatformName>\include\GL).  Please
note that although the Cogl source package does allow one to build Cogl without
a previously built and installed GLib, the Visual Studio projects only support
builds that does depend on GLib.

As Cogl use C99 types in lieu of GLib types, a compatible implementation of
stdint.h for Visual C++ is required, such as the one from
http://code.google.com/p/msinttypes/, so one would need to download and extract
the .zip file from that website and extract stdint.h into
<root>\vs9\<PlatformName>\include or somewhere where it can be found
automatically found by the compiler.  Note that Visual C++ 2010 and later
ships with stdint.h, so it is only required for Visual C++ 2008 builds.

If building the SDL2 winsys is desired (the *_SDL configs), you will also need the
SDL2 libraries from www.libsdl.org-building the SDL source package with Visual C++ 2008
is recommended via CMake, but one may want to use the Visual C++ binary packages
from that website.  Since Cogl-1.18.x, the Visual Studio Projects have been updated
to support the build of the SDL2 winsys in place of the original SDL-1.3 winsys
as SDL-2.x has been released for some time.  Please note, as builds with the SDL2
winsys includes the SDL2 headers, main() will be defined to SDL2's special main()
implementation on Windows, which will require linking to SDL2.lib and SDL2main.lib
for all apps that link to Cogl, unless SDL_MAIN_HANDLED is defined in your
"preprocessor definitions" options.

Please see the README file in the root directory of this Cogl source package
for the versions of the dependencies required.  See also
build/win32/vs9/README.txt in the GLib source package for details
where to unpack them.  It is recommended that at least the dependencies
from GNOME are also built with VS9 to avoid crashes caused by mixing different
CRTs-please see also the build/win32/vs9/README.txt in those respective packages.

The recommended build sequence of the dependencies are as follows (the non-GNOME
packages that are not downloaded as binaries from ftp://ftp.gnome.org have
makefiles and/or VS project files that can be used to compile with VS directly,
except the optional PCRE, which is built on VS using CMake; GLib has
VS9 project files in the latest stable versions, GDK-Pixbuf have VS9 project files
in the latest unstable version, and Pango should have VS9 project files
in the next unstable release):
-Unzip the binary packages for gettext-runtime, freetype, expat and fontconfig
 downloaded from ftp://ftp.gnome.org*
-zlib
-libpng
-(optional for GLib) PCRE (8.12 or later, building PCRE using CMake is
 recommended-please see build/win32/vs9/README.txt in the GLib source package)
-(for gdk-pixbuf, if GDI+ is not to be used) IJG JPEG or libjpeg-turbo
-(for gdk-pixbuf, if GDI+ is not to be used) jasper [JPEG-2000 library]
-(for gdk-pixbuf, if GDI+ is not to be used, requires zlib and IJG JPEG/libjpeg-turbo)
 libtiff
-GLib
-Cairo
-Pango
-GDK-Pixbuf

The "install" project will copy build results and headers into their
appropriate location under <root>\vs9\<PlatformName>. For instance,
built DLLs go into <root>\vs9\<PlatformName>\bin, built LIBs into
<root>\vs9\<PlatformName>\lib and Cogl headers into
<root>\vs9\<PlatformName>\include\Cogl-2.0.

*There is no known official VS9 build support for fontconfig
 (along with freetype and expat) and gettext-runtime, so
 please use the binaries from: 

  ftp://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/ (32 bit)
  ftp://ftp.gnome.org/pub/GNOME/binaries/win64/dependencies/ (64 bit)

Note: If you see C4819 errors and you are compiling Cogl on a DBCS
(Chinese/Korean/Japanese) version of Windows, you may need to switch
to an English locale in Control Panel->Region and Languages->System->
Change System Locale, reboot and rebuild to ensure Cogl and its
dependencies are built correctly.  This is due to a bug in Visual C++
running on DBCS locales.

--Chun-wei Fan <fanc999@yahoo.com.tw>
  (Adopted from the GTK+ Win32 VS README.txt file originally by Tor Lillqvist)
