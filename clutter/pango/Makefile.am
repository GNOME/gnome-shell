source_c = pangoclutter-fontmap.c  \
           pangoclutter-render.c   \
	   pangoclutter-glyph-cache.c

source_h = pangoclutter.h

source_h_priv = pangoclutter-private.h      \
		pangoclutter-glyph-cache.h

noinst_LTLIBRARIES = libpangoclutter.la

libpangoclutter_la_SOURCES = $(source_c)     \
		             $(source_h)     \
			     $(source_h_priv)

INCLUDES = \
	@GCC_FLAGS@ @CLUTTER_CFLAGS@ 			\
	$(CLUTTER_DEBUG_CFLAGS)				\
        -DCLUTTER_COMPILATION                           \
	-I$(top_srcdir)                         	\
	-I$(top_srcdir)/clutter				\
	-I$(top_builddir)/clutter

pangoclutterheadersdir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter
pangoclutterheaders_HEADERS = $(source_h)
