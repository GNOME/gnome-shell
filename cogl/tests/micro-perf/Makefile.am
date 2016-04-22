NULL =

AM_CPPFLAGS = \
	-I$(top_srcdir)

test_conformance_CPPFLAGS = \
	-DCOGL_ENABLE_EXPERIMENTAL_API \
	-DCOGL_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\"


noinst_PROGRAMS =

noinst_PROGRAMS += test-journal

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)

common_ldadd = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/cogl/libmutter-cogl.la \
	$(LIBM)

test_journal_SOURCES = test-journal.c
test_journal_LDADD = $(common_ldadd)
