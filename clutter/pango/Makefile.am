source_c = \
	cogl-pango-display-list.c \
	cogl-pango-fontmap.c    \
	cogl-pango-render.c     \
	cogl-pango-glyph-cache.c

source_h = cogl-pango.h

source_h_priv = \
	cogl-pango-display-list.h \
	cogl-pango-private.h    \
	cogl-pango-glyph-cache.h

noinst_LTLIBRARIES = libcoglpango.la

libcoglpango_la_SOURCES = $(source_c) $(source_h) $(source_h_priv)
libcoglpango_la_CPPFLAGS = $(CLUTTER_CFLAGS) $(COGL_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS)
libcoglpango_la_LIBADD = $(CLUTTER_LIBS)

INCLUDES = \
	-DG_DISABLE_SINGLE_INCLUDES	\
	-DCLUTTER_COMPILATION		\
	-DG_LOG_DOMAIN=\"CoglPango\"	\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/clutter		\
	-I$(top_srcdir)/clutter/cogl	\
	-I$(top_builddir)/clutter	\
	-I$(top_builddir)/clutter/cogl

coglpangoheadersdir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
coglpangoheaders_HEADERS = $(source_h)
