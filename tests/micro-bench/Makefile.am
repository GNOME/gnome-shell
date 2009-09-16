include $(top_srcdir)/build/autotools/Makefile.am.silent

noinst_PROGRAMS = \
	test-text \
	test-picking \
	test-text-perf

INCLUDES = -I$(top_srcdir)/ -I$(top_srcdir)/clutter -I$(top_builddir)/clutter
LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_API_VERSION@.la
AM_CFLAGS = \
	$(CLUTTER_CFLAGS) \
	$(MAINTAINER_CFLAGS) \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DTESTS_DATA_DIR=\""$(top_srcdir)/tests/data/"\"

AM_LDFLAGS = $(CLUTTER_LIBS)

test_text_SOURCES = test-text.c
test_picking_SOURCES = test-picking.c
test_text_perf_SOURCES = test-text-perf.c

