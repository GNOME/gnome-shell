# preamble

NULL =

DISTCLEANFILES =

mutterlibdir = $(libdir)/mutter
mutterlib_LTLIBRARIES = libmutter-cogl-gles2-@LIBMUTTER_API_VERSION@.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir)

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)

libmutter_cogl_gles2_@LIBMUTTER_API_VERSION@_la_SOURCES = cogl-gles2-api.c
libmutter_cogl_gles2_@LIBMUTTER_API_VERSION@_la_LDFLAGS = \
	-no-undefined \
	-rpath $(mutterlibdir) \
	-avoid-version \
	-export-dynamic \
	-export-symbols-regex "^gl*"

coglgles2includedir = $(includedir)/mutter/cogl/cogl-gles2/GLES2
coglgles2include_HEADERS = \
	GLES2/gl2.h \
	GLES2/gl2ext.h \
	GLES2/gl2platform.h

pc_files = mutter-cogl-gles2-$(LIBMUTTER_API_VERSION).pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

DISTCLEANFILES += $(pc_files)
