include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

INCLUDES = \
	-I$(top_srcdir)

test_conformance_CPPFLAGS = \
	-DCOGL_ENABLE_EXPERIMENTAL_API \
	-DCOGL_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\"


noinst_PROGRAMS = test-journal

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)

test_journal_SOURCES = test-journal.c
test_journal_LDADD = $(COGL_DEP_LIBS) $(top_builddir)/cogl/libcogl.la
