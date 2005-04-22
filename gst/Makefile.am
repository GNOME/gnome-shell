# see http://cvs.sourceforge.net/viewcvs.py/wechselspieler/wechselspieler/plugins/Makefile.am?rev=1.6&view=auto

lib_LTLIBRARIES = libcltrimagesink.la

# HACK HACK HACK HACK for now
libdir= /usr/lib/gstreamer-0.8/

INCLUDES=@GST_CFLAGS@ $(CLTR_CFLAGS)

LIBS=@GST_LIBS@


libcltrimagesink_la_SOURCES = cltrimagesink.c 
libcltrimagesink_la_INCLUDES = $(CLTR_CFLAGS)
libcltrimagesink_LIBDIR = ${libdir}/gst
libcltrimagesink_la_LIBADD = @CLTR_LIBS@
libcltrimagesink_la_LDFLAGS = -module -avoid-version

noinst_HEADERS = \
    cltrimagesink.h