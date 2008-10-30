source_c = \
	cogl-pango-fontmap.c    \
	cogl-pango-render.c     \
	cogl-pango-glyph-cache.c

source_h = cogl-pango.h

source_h_priv = \
	cogl-pango-private.h    \
	cogl-pango-glyph-cache.h

noinst_LTLIBRARIES = libcoglpango.la

libcoglpango_la_SOURCES = \
	$(source_c)     \
	$(source_h)     \
	$(source_h_priv)

INCLUDES = \
	@GCC_FLAGS@ @CLUTTER_CFLAGS@ 			\
	$(CLUTTER_DEBUG_CFLAGS)				\
	-DCLUTTER_COMPILATION                           \
	-I$(top_srcdir)	                 		\
	-I$(top_srcdir)/clutter				\
	-I$(top_srcdir)/clutter/cogl                    \
	-I$(top_builddir)/clutter	               	\
	-I$(top_builddir)/clutter/cogl

coglpangoheadersdir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/cogl
coglpangoheaders_HEADERS = $(source_h)
