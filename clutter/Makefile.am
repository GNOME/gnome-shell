source_h = pixbuf.h util.h fonts.h \
          cltr.h                  \
          cltr-private.h          \
          cltr-glu.h              \
          cltr-events.h           \
          cltr-texture.h          \
          cltr-widget.h           \
          cltr-window.h           \
          cltr-photo-grid.h       \
          cltr-video.h            \
          cltr-list.h             \
          cltr-overlay.h          \
          cltr-button.h           \
          cltr-scratch.h

source_c = pixbuf.c util.c fonts.c \
           cltr-core.c             \
           cltr-glu.c              \
           cltr-texture.c          \
           cltr-widget.c           \
           cltr-events.c	   \
           cltr-window.c           \
           cltr-photo-grid.c  	   \
           cltr-video.c  	   \
           cltr-list.c         	   \
           cltr-overlay.c          \
           cltr-button.c           \
           cltr-scratch.c

AM_CFLAGS = @GCC_FLAGS@ @CLTR_CFLAGS@  $(GST_CFLAGS) $(GCONF_CFLAGS)

lib_LTLIBRARIES      = libclutter.la
libclutter_la_SOURCES     = $(source_c) $(source_h)
libclutter_la_LIBADD      = @CLTR_LIBS@ $(GST_LIBS) $(GCONF_LIBS)

# http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91
# current : revision : age

libclutter_la_LDFLAGS = -version-info 0:0:0

clutterheadersdir = $(includedir)/clutter
clutterheaders_DATA = $(source_h)
