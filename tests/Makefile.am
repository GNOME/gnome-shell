noinst_PROGRAMS = test-textures test-events

INCLUDES = -I$(top_srcdir)/
LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@.la
AM_CFLAGS = $(CLUTTER_CFLAGS)
LDFLAGS = $(CLUTTER_LIBS) 

test_textures_SOURCES = test-textures.c

test_events_SOURCES = test-events.c
