noinst_PROGRAMS = test video-player video-cube super-oh behave

INCLUDES = -I$(top_srcdir)/

test_SOURCES = test.c
test_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
test_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(GCONF_LIBS) \
    $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la 

video_player_SOURCES = video-player.c
video_player_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
video_player_LDFLAGS = \
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

super_oh_SOURCES = super-oh.c
super_oh_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
super_oh_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(GCONF_LIBS) \
    $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la 

behave_SOURCES = behave.c
behave_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GCONF_CFLAGS)
behave_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(GCONF_LIBS) \
    $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la 


EXTRA_DIST = redhand.png              \
             clutter-logo-800x600.png \
             media-actions-pause.png  \
             media-actions-start.png  \
             vid-panel.png            \
             README 
