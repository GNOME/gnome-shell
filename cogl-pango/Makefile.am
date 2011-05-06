include $(top_srcdir)/build/autotools/Makefile.am.silent

source_c = \
	cogl-pango-display-list.c   \
	cogl-pango-fontmap.c        \
	cogl-pango-render.c         \
	cogl-pango-glyph-cache.c    \
	cogl-pango-pipeline-cache.c

source_h = cogl-pango.h

source_h_priv = \
	cogl-pango-display-list.h   \
	cogl-pango-private.h        \
	cogl-pango-glyph-cache.h    \
	cogl-pango-pipeline-cache.h

lib_LTLIBRARIES = libcogl-pango.la

libcogl_pango_la_SOURCES = $(source_c) $(source_h) $(source_h_priv)
libcogl_pango_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libcogl_pango_la_LIBADD = $(COGL_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl_pango_la_LIBADD += $(top_builddir)/cogl/libcogl.la
libcogl_pango_la_LDFLAGS = \
	-export-dynamic \
	-export-symbols-regex "^cogl.*"
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@

INCLUDES = \
	-DG_DISABLE_SINGLE_INCLUDES	\
	-DCLUTTER_COMPILATION		\
	-DG_LOG_DOMAIN=\"CoglPango\"	\
	-I$(top_srcdir)

cogl_pangoheadersdir = $(includedir)/cogl/cogl
cogl_pangoheaders_HEADERS = $(source_h)

pc_files = cogl-pango-1.0.pc \
	   cogl-pango-$(COGL_API_VERSION).pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST = cogl-pango.pc.in
DISTCLEANFILES = $(pc_files)
