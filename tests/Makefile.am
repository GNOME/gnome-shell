SUBDIRS = conform micro-perf data

DIST_SUBDIRS = conform data

EXTRA_DIST = README

test conform:
	( cd ./conform && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?

.PHONY: test conform

# run make test as part of make check
check-local: test
