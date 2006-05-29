noinst_PROGRAMS = test test-video video-cube test-text super-oh

INCLUDES = -I$(top_srcdir)/clutter

test_SOURCES = test.c
test_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
test_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(GCONF_LIBS) \
    $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la 

test_video_SOURCES = test-video.c
test_video_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
test_video_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(GCONF_LIBS) \
    $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la 

video_cube_SOURCES = video-cube.c
video_cube_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
video_cube_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(GCONF_LIBS) \
    $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la 

test_text_SOURCES = test-text.c
test_text_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
test_text_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(GCONF_LIBS) \
    $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la 

super_oh_SOURCES = super-oh.c
super_oh_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
super_oh_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(GCONF_LIBS) \
    $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la 

EXTRA_DIST = redhand.png clutter-logo-800x600.png