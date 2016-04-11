SUBDIRS = test-fixtures

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

SUBDIRS += tests

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config-custom.h

# .changelog expects these to be initializes
CLEANFILES=
DISTCLEANFILES=

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-maintainer-flags \
	--enable-profile \
	--enable-gles2 \
	--enable-gl \
	--enable-xlib-egl-platform \
	--enable-wayland-egl-platform \
	--enable-glx \
	--enable-wayland-egl-server  \
	--enable-cogl-gst
