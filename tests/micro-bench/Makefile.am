include $(top_srcdir)/build/autotools/Makefile.am.silent

common_ldadd = $(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la

noinst_PROGRAMS = \
	test-text \
	test-picking \
	test-text-perf \
	test-random-text \
	test-cogl-perf

AM_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)

AM_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-DCOGL_DISABLE_DEPRECATION_WARNINGS \
	-DCLUTTER_DISABLE_DEPRECATION_WARNINGS \
	-DTESTS_DATA_DIR=\""$(top_srcdir)/tests/data/"\" \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter

LDADD = $(common_ldadd) $(CLUTTER_LIBS) -lm

test_text_SOURCES = test-text.c
test_picking_SOURCES = test-picking.c
test_text_perf_SOURCES = test-text-perf.c
test_random_text_SOURCES = test-random-text.c
test_cogl_perf_SOURCES = test-cogl-perf.c

-include $(top_srcdir)/build/autotools/Makefile.am.gitignore
