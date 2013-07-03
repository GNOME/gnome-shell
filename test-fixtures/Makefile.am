
noinst_LTLIBRARIES = libtest-fixtures.la

libtest_fixtures_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir)/cogl \
	-Wall \
	$(NULL)

if !USE_GLIB
libtest_fixtures_la_CPPFLAGS += -I$(top_builddir)/deps/glib
endif

libtest_fixtures_la_CPPFLAGS += \
	-DCOGL_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\" \
	-DCOGL_COMPILATION

libtest_fixtures_la_CFLAGS = -g3 -O0 $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)

libtest_fixtures_la_SOURCES = \
	test-unit.h \
	test-utils.h \
	test-utils.c

