SUBDIRS = accessibility data conform interactive micro-bench performance

EXTRA_DIST = README

test conform:
	( cd ./conform && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?

test-report full-report:
	( cd ./conform && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $$?

.PHONY: test conform test-report full-report

# run make test as part of make check
check-local: test
