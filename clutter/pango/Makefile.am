source_c = pangoclutter-font.c     \
           pangoclutter-fontmap.c  \
           pangoclutter-render.c

source_h = pangoclutter.h

source_h_priv = pangoclutter-private.h

noinst_LTLIBRARIES = libpangoclutter.la

libpangoclutter_la_SOURCES = $(source_c)     \
        	             $(source_h)     \
                             $(source_h_priv)

INCLUDES = @GCC_FLAGS@ @CLUTTER_CFLAGS@ -I$(top_srcdir)

pangoclutterheadersdir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter
pangoclutterheaders_HEADERS = $(source_h)
