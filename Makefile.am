SUBDIRS = cogl

if BUILD_COGL_PANGO
SUBDIRS += cogl-pango
endif

SUBDIRS += examples doc po

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	README.in \
	config-custom.h

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
	--enable-glx

include $(top_srcdir)/build/autotools/Makefile.am.release
