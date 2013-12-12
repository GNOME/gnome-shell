include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = build clutter tests doc po

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

if ENABLE_GCOV
# use recursive makes in order to ignore errors during check/perf
lcov:
	-$(MAKE) $(AM_MAKEFLAGS) -C clutter check
	-$(MAKE) $(AM_MAKEFLAGS) -C tests/conform test
	$(MAKE) $(AM_MAKEFLAGS) genlcov

# we have to massage the lcov.info file slightly to hide the effect of libtool
# placing the objects files in the .libs/ directory separate from the *.c
genlcov:
	$(LTP) --directory $(top_builddir) --capture --output-file clutter-lcov.info --test-name CLUTTER_TEST --no-checksum
	$(SED) -e 's#.libs/##' < clutter-lcov.info > clutter-lcov.info.tmp
	LANG=C $(LTP_GENHTML) --prefix $(top_builddir) --output-directory clutter-lcov --title "Clutter Code Coverage" --show-details clutter-lcov.info.tmp
	rm -f clutter-lcov.info.tmp

lcov-clean:
	-$(LTP) --directory $(top_builddir) -z
	-$(RM) -rf clutter-lcov.info clutter-lcov
else
lcov genlcov lcov-clean:
	@echo You need to configure Clutter with support for gcov enabled.
	@echo e.g., ./configure --enable-gcov
endif

.PHONY: test-report full-report perf-report lcov genlcov lcov-clean
