noinst_PROGRAMS = test-conformance

test_conformance_SOURCES = \
	test-conform-main.c \
	test-conform-common.c \
	test-conform-common.h \
	\
	test-timeline-dup-frames.c \
	test-timeline-interpolate.c \
	test-timeline-rewind.c \
	test-timeline-smoothness.c \
	test-timeline.c \
	test-mesh-contiguous.c \
	test-mesh-interleved.c \
	test-mesh-mutability.c \
	test-pick.c \
	test-label-cache.c \
	test-clutter-entry.c \
	test-clutter-rectangle.c \
	test-clutter-fixed.c \
        test-actor-invariants.c

# For convenience, this provides a way to easily run individual unit tests:
.PHONY: wrappers
wrappers: test-conformance
	for i in `./test-conformance -l -m thorough`; \
	do \
		ln -sf $(top_srcdir)/tests/conform/wrapper.sh `basename $$i`; \
	done
# NB: BUILT_SOURCES here a misnomer. We aren't building source, just inserting
# a phony rule that will generate symlink scripts for running individual tests
BUILT_SOURCES = wrappers
	
test_conformance_CFLAGS = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter \
	$(CLUTTER_CFLAGS)
test_conformance_LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@.la

.PHONY: test test-report full-report
test:
	gtester -o=test-conformance-results.xml ./test-conformance

test-report:
	gtester -o=test-conformance-results.xml -k ./test-conformance \
	  && gtester-report test-conformance-results.xml > test-conformance-results.html \
	  && gnome-open ./test-conformance-results.html

full-report:
	gtester -o=test-conformance-results.xml -k -m=slow ./test-conformance \
	  && gtester-report test-conformance-results.xml > test-conformance-results.html \
	  && gnome-open ./test-conformance-results.html

EXTRA_DIST = ADDING_NEW_TESTS

