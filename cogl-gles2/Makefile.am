include $(top_srcdir)/build/autotools/Makefile.am.silent

# preamble

NULL =

lib_LTLIBRARIES = libcogl-gles2.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir)

if !USE_GLIB
INCLUDES += -I$(top_builddir)/deps/glib
endif

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)

libcogl_gles2_la_SOURCES = cogl-gles2-api.c
libcogl_gles2_la_LDFLAGS = \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-export-dynamic \
	-export-symbols-regex "^gl*"

coglgles2includedir = $(includedir)/cogl/cogl-gles2/GLES2
coglgles2include_HEADERS = \
	GLES2/gl2.h \
	GLES2/gl2ext.h \
	GLES2/gl2platform.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cogl-gles2-experimental.pc
