SUBDIRS = deps test-fixtures

SUBDIRS += cogl

if BUILD_COGL_PATH
SUBDIRS += cogl-path
endif

if BUILD_COGL_PANGO
SUBDIRS += cogl-pango
endif

if BUILD_COGL_GLES2
SUBDIRS += cogl-gles2
endif

if BUILD_COGL_GST
SUBDIRS += cogl-gst
endif

SUBDIRS += tests examples doc po build

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	README.in \
	config-custom.h \
	config.h.win32 \
	config.h.win32.in

# .changelog expects these to be initializes
CLEANFILES=
DISTCLEANFILES=

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-maintainer-flags \
	--enable-profile \
	--enable-gles1 \
	--enable-gles2 \
	--enable-gl \
	--enable-xlib-egl-platform \
	--enable-wayland-egl-platform \
	--enable-glx \
	--enable-wayland-egl-server  \
	--enable-cogl-gst

include $(top_srcdir)/build/autotools/Makefile.am.release
