noinst_PROGRAMS = test-textures test-events test-offscreen test-scale

INCLUDES = -I$(top_srcdir)/
LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@.la
AM_CFLAGS = $(CLUTTER_CFLAGS)
AM_LDFLAGS = $(CLUTTER_LIBS) 

test_textures_SOURCES  = test-textures.c
test_events_SOURCES    = test-events.c
test_offscreen_SOURCES = test-offscreen.c
test_scale_SOURCES     = test-scale.c
