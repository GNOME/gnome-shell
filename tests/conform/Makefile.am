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
	test-path.c \
	test-pick.c \
	test-label-cache.c \
	test-clutter-entry.c \
	test-clutter-rectangle.c \
	test-clutter-fixed.c \
        test-actor-invariants.c \
        test-paint-opacity.c \
	test-backface-culling.c \
        test-binding-pool.c \
        test-clutter-text.c

# For convenience, this provides a way to easily run individual unit tests:
.PHONY: wrappers
wrappers: test-conformance$(EXEEXT)
	for i in `./test-conformance -l -m thorough|grep '^/'`; \
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

.PHONY: test
.PHONY: test-report test-report-normal test-report-disable-npots
.PHONY: full-report full-report-normal full-report-disable-npots
.PHONY: full-report-generate

test:
	gtester -o=test-conformance-results.xml ./test-conformance

test-report-normal:
	gtester -o=test-conformance-results.xml -k ./test-conformance \
	  && ( gtester-report test-conformance-results.xml \
	      | sed 's/>GTester Unit Test Report</>GTester Unit Test Report (normal)</' \
              > test-conformance-results.html ) \
	  && gnome-open ./test-conformance-results.html

test-report-disable-npots:
	../tools/disable-npots.sh \
	  gtester -o=test-conformance-results-dn.xml -k ./test-conformance \
	  && ( gtester-report test-conformance-results-dn.xml \
	      | sed 's/>GTester Unit Test Report</>GTester Unit Test Report (no NPOTs)</' \
              > test-conformance-results-dn.html ) \
	  && gnome-open ./test-conformance-results-dn.html

test-report: test-report-normal

full-report-normal:
	gtester -o=test-conformance-results.xml -k -m=slow ./test-conformance \
	  && ( gtester-report test-conformance-results.xml \
	      | sed 's/>GTester Unit Test Report</>GTester Unit Test Report (normal)</' \
              > test-conformance-results.html )

full-report-disable-npots:
	../tools/disable-npots.sh \
	  gtester -o=test-conformance-results-dn.xml -k -m=slow ./test-conformance \
	  && ( gtester-report test-conformance-results-dn.xml \
	      | sed 's/>GTester Unit Test Report</>GTester Unit Test Report (no NPOTs)</' \
              > test-conformance-results-dn.html )

REPORTS = test-conformance-results.html

if HAVE_LIBDL

REPORTS += test-conformance-results-dn.html

full-report-generate: full-report-normal full-report-disable-npots

else

full-report-generate: full-report-normal

endif

full-report: full-report-generate
	for x in $(REPORTS); do \
		gnome-open "$$x"; \
	done

EXTRA_DIST = ADDING_NEW_TESTS

