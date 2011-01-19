include $(top_srcdir)/build/autotools/Makefile.am.silent

noinst_PROGRAMS = \
	test-picking \
	test-text-perf \
	test-state \
	test-state-interactive \
	test-state-hidden \
	test-state-mini \
	test-state-pick

INCLUDES = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl
LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la
AM_CFLAGS = \
	$(CLUTTER_CFLAGS) \
	$(MAINTAINER_CFLAGS) \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DTESTS_DATA_DIR=\""$(top_srcdir)/tests/data/"\"

perf-report: check

check:
	for a in $(noinst_PROGRAMS);do ./$$a;done;true

AM_LDFLAGS = $(CLUTTER_LIBS)

test_picking_SOURCES = test-picking.c
test_text_perf_SOURCES = test-text-perf.c
test_state_SOURCES = test-state.c
test_state_hidden_SOURCES = test-state-hidden.c
test_state_pick_SOURCES = test-state-pick.c
test_state_interactive_SOURCES = test-state-interactive.c
test_state_mini_SOURCES = test-state-mini.c

EXTRA_DIST = Makefile-retrospect Makefile-tests create-report.rb test-common.h

-include $(top_srcdir)/build/autotools/Makefile.am.gitignore
