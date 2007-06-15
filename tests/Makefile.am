noinst_PROGRAMS = test-textures test-events test-offscreen test-scale \
                  test-actors test-behave test-text test-entry test-project \
		  test-boxes

INCLUDES = -I$(top_srcdir)/
LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@.la
AM_CFLAGS = $(CLUTTER_CFLAGS)
AM_LDFLAGS = $(CLUTTER_LIBS) 

test_textures_SOURCES  = test-textures.c
test_events_SOURCES    = test-events.c
test_offscreen_SOURCES = test-offscreen.c
test_scale_SOURCES     = test-scale.c
test_actors_SOURCES     = test-actors.c
test_behave_SOURCES    = test-behave.c 
test_text_SOURCES      = test-text.c 
test_entry_SOURCES     = test-entry.c
test_project_SOURCES   = test-project.c
test_boxes_SOURCES     = test-boxes.c
