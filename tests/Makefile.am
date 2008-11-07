
SUBDIRS = data conform interactive micro-bench tools

EXTRA_DIST = README

.PHONY: test conform
test conform:
	$(MAKE) -C ./conform test
.PHONY: test-report full-report
test-report full-report:
	$(MAKE) -C ./conform $(@)

# run make test as part of make check
check-local: test

