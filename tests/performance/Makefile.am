include $(top_srcdir)/build/autotools/Makefile.am.silent

check_PROGRAMS = \
	test-picking \
	test-text-perf \
	test-state \
	test-state-interactive \
	test-state-hidden \
	test-state-mini \
	test-state-pick

common_ldadd = $(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la

LDADD = $(common_ldadd) $(CLUTTER_LIBS) -lm

AM_CFLAGS = $(CLUTTER_CFLAGS)

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

perf-report: check

check:
	for a in $(noinst_PROGRAMS);do ./$$a;done;true

test_picking_SOURCES = test-picking.c
test_text_perf_SOURCES = test-text-perf.c
test_state_SOURCES = test-state.c
test_state_hidden_SOURCES = test-state-hidden.c
test_state_pick_SOURCES = test-state-pick.c
test_state_interactive_SOURCES = test-state-interactive.c
test_state_mini_SOURCES = test-state-mini.c

EXTRA_DIST = Makefile-retrospect Makefile-tests create-report.rb test-common.h

-include $(top_srcdir)/build/autotools/Makefile.am.gitignore
