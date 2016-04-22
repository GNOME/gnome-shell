SUBDIRS = conform

if UNIT_TESTS
SUBDIRS += unit
endif

SUBDIRS += micro-perf data

DIST_SUBDIRS = conform unit micro-perf data

EXTRA_DIST = README test-launcher.sh run-tests.sh

if UNIT_TESTS
test conform:
	( cd ./conform && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?
	( cd ./unit && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?
else
test conform:
	( cd ./conform && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?
endif

.PHONY: test conform

# run make test as part of make check
check-local: test

if ENABLE_INSTALLED_TESTS
insttestdir = $(libexecdir)/installed-tests/$(PACKAGE)
insttest_SCRIPTS = run-tests.sh
insttest_DATA = config.env
endif
