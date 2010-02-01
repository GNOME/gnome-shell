SUBDIRS = data interactive micro-bench tools

if BUILD_TESTS
SUBDIRS += conform
endif

DIST_SUBDIRS = data conform interactive micro-bench tools

EXTRA_DIST = README

if BUILD_TESTS
test conform:
	( cd ./conform && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?

test-report full-report:
	( cd ./conform && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?
else
test conform:
	@true

test-report full-report:
	@true
endif # BUILD_TESTS

.PHONY: test conform test-report full-report

# run make test as part of make check
check-local: test
