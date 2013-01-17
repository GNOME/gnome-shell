include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

AM_CPPFLAGS = \
	-I$(top_srcdir)

test_conformance_CPPFLAGS = \
	-DCOGL_ENABLE_EXPERIMENTAL_API \
	-DCOGL_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\"


noinst_PROGRAMS =

if USE_GLIB
noinst_PROGRAMS += test-journal
endif

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)

test_journal_SOURCES = test-journal.c
test_journal_LDADD = $(COGL_DEP_LIBS) $(top_builddir)/cogl/libcogl.la
