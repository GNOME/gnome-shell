include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = build clutter tests doc po

# XXX - this is a massive hack to make autoreconf honour the ACLOCAL_FLAGS
# that jhbuild sets while still retaining build/autotools as the authoritative
# source for m4 macros
ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	ChangeLog.pre-git-import	\
	$(NULL)

CLEANFILES = $(pcfiles)

DISTCLEANFILES =

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-maintainer-flags --enable-docs

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = 	\
	aclocal.m4 	\
	config.guess 	\
	config.h.in 	\
        configure 	\
	gtk-doc.make 	\
	Makefile.in 	\
	$(NULL)

include $(top_srcdir)/build/autotools/Makefile.am.changelog
include $(top_srcdir)/build/autotools/Makefile.am.release

# proxy rule for gcov
gcov:
	@( cd clutter && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?

# proxy rules for tests
test-report full-report:
	$(MAKE) -C tests/conform $(@)

.PHONY: gcov test-report full-report
