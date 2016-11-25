NULL =

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir)/cogl \
	-DCOGL_DISABLE_DEPRECATED

test_conformance_CPPFLAGS = \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\"


noinst_PROGRAMS =

noinst_PROGRAMS += test-journal

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)

common_ldadd = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/cogl/libmutter-cogl-$(LIBMUTTER_API_VERSION).la \
	$(LIBM)

test_journal_SOURCES = test-journal.c
test_journal_LDADD = $(common_ldadd)
