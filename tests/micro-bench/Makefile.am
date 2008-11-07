noinst_PROGRAMS = \
	test-text

INCLUDES = -I$(top_srcdir)/ -I$(top_srcdir)/clutter -I$(top_builddir)/clutter
LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@.la
AM_CFLAGS = $(CLUTTER_CFLAGS) -DTESTS_DATA_DIR=$(top_srcdir)/tests/data/
AM_LDFLAGS = $(CLUTTER_LIBS)

test_text_SOURCES = test-text.c

