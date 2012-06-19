include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = clutter doc po build

if BUILD_TESTS
SUBDIRS += tests
endif

if BUILD_EXAMPLES
SUBDIRS += examples
endif

DIST_SUBDIRS = clutter tests examples doc po build

# XXX - this is a massive hack to make autoreconf honour the ACLOCAL_FLAGS
# that jhbuild sets while still retaining build/autotools as the authoritative
# source for m4 macros
ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	ChangeLog.pre-git-import	\
	README.in			\
	README				\
	config.h.win32		\
	config.h.win32.in	\
	$(NULL)

CLEANFILES = $(pcfiles)

DISTCLEANFILES =

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-maintainer-flags --enable-docs

# .release requires .changelog rules
include $(top_srcdir)/build/autotools/Makefile.am.changelog
include $(top_srcdir)/build/autotools/Makefile.am.release

# proxy rules for tests
test-report full-report:
	$(MAKE) -C tests/conform $(@)

perf-report:
	$(MAKE) -C tests/performance $(@)

.PHONY: test-report full-report perf-report
