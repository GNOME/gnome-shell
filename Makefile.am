include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = build clutter tests doc po

ACLOCAL_AMFLAGS = -I build/autotools

EXTRA_DIST = \
	ChangeLog.pre-git-import	\
	$(NULL)

CLEANFILES = $(pcfiles)

DISTCLEANFILES =

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-maintainer-flags

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
