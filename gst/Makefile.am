lib_LTLIBRARIES = libclutterimagesink.la

# HACK HACK HACK HACK for now
libdir= /usr/lib/gstreamer-0.10/

INCLUDES=@GST_CFLAGS@ $(CLUTTER_CFLAGS)

LIBS=@GST_LIBS@

libclutterimagesink_la_SOURCES = clutterimagesink.c 
libclutterimagesink_la_INCLUDES = $(CLUTTER_CFLAGS)
libclutterimagesink_LIBDIR = ${libdir}/gst
libclutterimagesink_la_LIBADD = @CLUTTER_LIBS@
libclutterimagesink_la_LDFLAGS = -module -avoid-version

noinst_HEADERS = \
    clutterimagesink.h