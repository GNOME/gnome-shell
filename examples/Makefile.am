noinst_PROGRAMS = test super-oh behave

INCLUDES = -I$(top_srcdir)/
LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la

test_SOURCES = test.c
test_CFLAGS = $(CLUTTER_CFLAGS) $(GCONF_CFLAGS)
test_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GCONF_LIBS) 

super_oh_SOURCES = super-oh.c
super_oh_CFLAGS = $(CLUTTER_CFLAGS) $(GCONF_CFLAGS)
super_oh_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GCONF_LIBS) 

behave_SOURCES = behave.c
behave_CFLAGS = $(CLUTTER_CFLAGS) $(GCONF_CFLAGS)
behave_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GCONF_LIBS) 


EXTRA_DIST = redhand.png              \
             clutter-logo-800x600.png \
             README 
