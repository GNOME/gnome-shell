NULL =

noinst_PROGRAMS = test-conformance

test_conformance_SOURCES = 		\
	test-conform-main.c 		\
	test-conform-common.c 		\
	test-conform-common.h 		\
	\
	test-timeline-interpolate.c 	\
	test-timeline-rewind.c 		\
	test-timeline.c 		\
	test-vertex-buffer-contiguous.c \
	test-vertex-buffer-interleved.c \
	test-vertex-buffer-mutability.c \
	test-path.c 			\
	test-pick.c 			\
	test-clutter-rectangle.c 	\
	test-clutter-fixed.c 		\
	test-actor-invariants.c 	\
	test-paint-opacity.c 		\
	test-backface-culling.c 	\
	test-binding-pool.c		\
        test-clutter-text.c             \
        test-text-cache.c               \
	test-anchors.c                  \
	test-npot-texture.c             \
	test-model.c			\
	test-blend-strings.c		\
	test-color.c			\
	test-clutter-units.c		\
	test-premult.c			\
	test-materials.c		\
        $(NULL)

# For convenience, this provides a way to easily run individual unit tests:
.PHONY: wrappers clean-wrappers

UNIT_TESTS = `./test-conformance -l -m thorough | $(GREP) '^/'`

wrappers: stamp-test-conformance
	@true
stamp-test-conformance: test-conformance$(EXEEXT)
	@for i in $(UNIT_TESTS); \
	do \
		unit=`basename $$i | sed -e s/_/-/g`; \
		echo "  GEN   $$unit"; \
		( echo "#!/bin/sh" ; echo "$(top_srcdir)/tests/conform/test-launcher.sh '$$i'" ) > $$unit$(EXEEXT) ; \
		chmod +x $$unit$(EXEEXT); \
	done \
	&& echo timestamp > $(@F)

clean-wrappers:
	@for i in $(UNIT_TESTS); \
	do \
		unit=`basename $$i | sed -e s/_/-/g`; \
		echo "RM $$unit"; \
		rm -f $$unit$(EXEEXT) ; \
	done \
	&& rm -f stamp-test-conformance

# NB: BUILT_SOURCES here a misnomer. We aren't building source, just inserting
# a phony rule that will generate symlink scripts for running individual tests
BUILT_SOURCES = wrappers

test_conformance_CFLAGS = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter \
	-DG_DISABLE_SINGLE_INCLUDES \
	$(CLUTTER_CFLAGS) \
	$(MAINTAINER_CFLAGS)

test_conformance_LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_API_VERSION@.la

.PHONY: test
.PHONY: test-report test-report-normal test-report-disable-npots
.PHONY: full-report full-report-normal full-report-disable-npots
.PHONY: full-report-generate

test:
	@gtester -o=test-conformance-results.xml ./test-conformance

test-verbose:
	@gtester --verbose -o=test-conformance-result.xml ./test-conformance

test-report-normal:
	@gtester -o=test-conformance-results.xml -k ./test-conformance \
	  && ( gtester-report test-conformance-results.xml \
	      | sed 's/>GTester Unit Test Report</>GTester Unit Test Report (normal)</' \
	      > test-conformance-results.html ) \
	  && gnome-open ./test-conformance-results.html

test-report-disable-npots:
	@../tools/disable-npots.sh \
	  gtester -o=test-conformance-results-dn.xml -k ./test-conformance \
	  && ( gtester-report test-conformance-results-dn.xml \
	      | sed 's/>GTester Unit Test Report</>GTester Unit Test Report (no NPOTs)</' \
	      > test-conformance-results-dn.html ) \
	  && gnome-open ./test-conformance-results-dn.html

test-report: test-report-normal

full-report-normal:
	@gtester -o=test-conformance-results.xml -k -m=slow ./test-conformance \
	  && ( gtester-report test-conformance-results.xml \
	      | sed 's/>GTester Unit Test Report</>GTester Unit Test Report (normal)</' \
	      > test-conformance-results.html )

full-report-disable-npots:
	@../tools/disable-npots.sh \
	  gtester -o=test-conformance-results-dn.xml -k -m=slow ./test-conformance \
	  && ( gtester-report test-conformance-results-dn.xml \
	      | sed 's/>GTester Unit Test Report</>GTester Unit Test Report (no NPOTs)</' \
	      > test-conformance-results-dn.html )

XML_REPORTS = test-conformance-results.xml
HTML_REPORTS = test-conformance-results.html

if HAVE_LIBDL
XML_REPORTS += test-conformance-results-dn.xml
HTML_REPORTS += test-conformance-results-dn.html
full-report-generate: full-report-normal full-report-disable-npots

else
full-report-generate: full-report-normal

endif

full-report: full-report-generate
	@for x in $(HTML_REPORTS); do \
		gnome-open "$$x"; \
	done

EXTRA_DIST = ADDING_NEW_TESTS test-launcher.sh

# we override the clean-generic target to clean up the wrappers so
# we cannot use CLEANFILES
clean-generic: clean-wrappers
	rm -f $(XML_REPORTS) $(HTML_REPORTS)
