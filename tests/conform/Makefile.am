include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

noinst_PROGRAMS = test-conformance

# the common sources
common_sources = \
	test-conform-common.h 	\
	test-conform-common.c 	\
	test-conform-main.c	\
	$(NULL)

# the unit-specific sources; please: keep all sections in alphabetical order!
units_sources =

# animation tests
units_sources += \
	test-animator.c			\
	test-behaviours.c		\
	test-score.c			\
	test-state.c			\
	test-timeline.c			\
	test-timeline-interpolate.c 	\
	test-timeline-rewind.c 		\
	$(NULL)

# cogl tests
units_sources += \
	test-cogl-backface-culling.c 		\
	test-cogl-blend-strings.c		\
	test-cogl-depth-test.c			\
	test-cogl-fixed.c 			\
	test-cogl-materials.c			\
	test-cogl-pipeline-user-matrix.c	\
	test-cogl-viewport.c			\
	test-cogl-multitexture.c        	\
	test-cogl-npot-texture.c        	\
	test-cogl-object.c			\
	test-cogl-offscreen.c			\
	test-cogl-path.c			\
	test-cogl-pixel-buffer.c		\
	test-cogl-premult.c			\
	test-cogl-readpixels.c			\
	test-cogl-sub-texture.c         	\
	test-cogl-texture-3d.c          	\
	test-cogl-texture-get-set-data.c 	\
	test-cogl-texture-mipmaps.c     	\
	test-cogl-texture-pixmap-x11.c  	\
	test-cogl-texture-rectangle.c   	\
	test-cogl-atlas-migration.c		\
	test-cogl-vertex-buffer-contiguous.c 	\
	test-cogl-vertex-buffer-interleved.c 	\
	test-cogl-vertex-buffer-mutability.c 	\
	test-cogl-wrap-modes.c          	\
	test-cogl-primitive.c			\
	test-cogl-just-vertex-shader.c		\
	$(NULL)

# actors tests
units_sources += \
	test-actor-destroy.c		\
	test-actor-size.c		\
	test-actor-invariants.c 	\
	test-anchors.c                  \
	test-binding-pool.c		\
	test-clutter-cairo-texture.c    \
	test-clutter-rectangle.c 	\
        test-clutter-text.c             \
	test-clutter-texture.c		\
	test-group.c			\
	test-path.c 			\
	test-paint-opacity.c 		\
	test-pick.c 			\
	test-texture-fbo.c		\
        test-text-cache.c               \
	$(NULL)

# objects tests
units_sources += \
	test-clutter-units.c		\
	test-color.c			\
	test-model.c			\
	test-script-parser.c		\
        $(NULL)

test_conformance_SOURCES = $(common_sources) $(units_sources)

if OS_WIN32
SHEXT =
else
SHEXT = $(EXEEXT)
endif

# For convenience, this provides a way to easily run individual unit tests:
.PHONY: wrappers clean-wrappers

#UNIT_TESTS = `./test-conformance -l -m thorough | $(GREP) '^/'`

wrappers: stamp-test-conformance
	@true
stamp-test-conformance: Makefile $(srcdir)/test-conform-main.c
	@mkdir -p wrappers
	@sed -n \
		-e 's/^ \{1,\}TEST_CONFORM_SIMPLE *(.*"\([^",]\{1,\}\)", *\([a-zA-Z0-9_]\{1,\}\).*/\/conform\1\/\2/p' \
		-e 's/^ \{1,\}TEST_CONFORM_SKIP *(.*"\([^",]\{1,\}\)", *\([a-zA-Z0-9_]\{1,\}\).*/\/conform\1\/\2/p' \
		-e 's/^ \{1,\}TEST_CONFORM_TODO *(.*"\([^",]\{1,\}\)", *\([a-zA-Z0-9_]\{1,\}\).*/\/conform\1\/\2/p' \
	$(srcdir)/test-conform-main.c > unit-tests
	@chmod +x test-launcher.sh
	@( echo "/stamp-test-conformance" ; \
	   echo "/test-conformance" ; \
	   echo "*.o" ; \
	   echo "*.xml" ; \
	   echo "*.html" ; \
	   echo ".gitignore" ; \
	   echo "unit-tests" ; \
	   echo "/wrappers/" ) > .gitignore
	@for i in `cat unit-tests`; \
	do \
		unit=`basename $$i | sed -e s/_/-/g`; \
		echo "  GEN    $$unit"; \
		( echo "#!/bin/sh" ; echo "$(abs_builddir)/test-launcher.sh '$$i' \"\$$@\"" ) > $$unit$(SHEXT) ; \
		( echo "#!/bin/sh" ; echo "exec $(abs_builddir)/test-conformance$(EXEEXT) -p $$i \"\$$@\"" ) > wrappers/$$unit$(SHEXT) ; \
		chmod +x $$unit$(SHEXT); \
		chmod +x wrappers/$$unit$(SHEXT); \
		echo "/$$unit$(SHEXT)" >> .gitignore; \
	done \
	&& echo timestamp > $(@F)

clean-wrappers:
	@for i in `cat unit-tests`; \
	do \
		unit=`basename $$i | sed -e s/_/-/g`; \
		echo "  RM     $$unit"; \
		rm -f $$unit$(SHEXT) ; \
		rm -f wrappers/$$unit$(SHEXT) ; \
	done \
	&& rm -f unit-tests \
	&& rm -f stamp-test-conformance

# NB: BUILT_SOURCES here a misnomer. We aren't building source, just inserting
# a phony rule that will generate symlink scripts for running individual tests
BUILT_SOURCES = wrappers

INCLUDES = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl

test_conformance_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_ENABLE_EXPERIMENTAL_API \
	-DCOGL_DISABLE_DEPRECATED \
	-DCLUTTER_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\"

test_conformance_CFLAGS = -g $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)

test_conformance_LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la $(CLUTTER_LIBS)

test_conformance_LDFLAGS = -export-dynamic

test: wrappers
	@$(top_srcdir)/tests/conform/run-tests.sh \
	  ./test-conformance$(EXEEXT) -o test-report.xml

test-verbose: wrappers
	@$(top_srcdir)/tests/conform/run-tests.sh \
	  ./test-conformance$(EXEEXT) -o test-report.xml --verbose

GTESTER = gtester
GTESTER_REPORT = gtester-report

# XXX: we could prevent the conformance test suite from running
#      by simply defining this variable conditionally
TEST_PROGS = test-conformance

.PHONY: test
.PHONY: test-report perf-report full-report
.PHONY: test-report-npot perf-report-npot full-report-npot

# test-report: run tests and generate report
# perf-report: run tests with -m perf and generate report
# full-report: like test-report: with -m perf and -m slow
test-report perf-report full-report:	${TEST_PROGS}
	@test -z "${TEST_PROGS}" || { \
	  export GTESTER_LOGDIR=`mktemp -d "$(srcdir)/.testlogs-XXXXXX"` ; \
	  if test -d "$(top_srcdir)/.git"; then \
	    export REVISION="`git describe`" ;  \
	  else \
	    export REVISION="$(VERSION) $(CLUTTER_RELEASE_STATUS)" ; \
	  fi ; \
	  export TIMESTAMP=`date +%Y-%m-%dT%H:%M:%S%z` ; \
	  case $@ in \
	  test-report) test_options="-k";; \
	  perf-report) test_options="-k -m=perf";; \
	  full-report) test_options="-k -m=perf -m=slow";; \
	  esac ; \
	  $(top_srcdir)/tests/conform/run-tests.sh \
	    ./test-conformance$(EXEEXT) \
	    --verbose \
	    $$test_options \
	    -o `mktemp "$$GTESTER_LOGDIR/log-XXXXXX"` ; \
	  echo '<?xml version="1.0"?>'              > $@.xml ; \
	  echo '<report-collection>'               >> $@.xml ; \
	  echo '<info>'                            >> $@.xml ; \
	  echo '  <package>$(PACKAGE)</package>'   >> $@.xml ; \
	  echo '  <version>$(VERSION)</version>'   >> $@.xml ; \
	  echo "  <revision>$$REVISION</revision>" >> $@.xml ; \
	  echo "  <date>$$TIMESTAMP</date>"        >> $@.xml ; \
	  echo '</info>'                           >> $@.xml ; \
	  for lf in `ls -L "$$GTESTER_LOGDIR"/.` ; do \
	    sed '1,1s/^<?xml\b[^>?]*?>//' <"$$GTESTER_LOGDIR"/"$$lf" >> $@.xml ; \
	  done ; \
	  echo >> $@.xml ; \
	  echo '</report-collection>' >> $@.xml ; \
	  ${GTESTER_REPORT} --version 2>/dev/null 1>&2 ; test "$$?" != 0 || ${GTESTER_REPORT} $@.xml >$@.html ; \
	  rm -rf "$$GTESTER_LOGDIR" ; \
	}

# same as above, but with a wrapper that forcibly disables non-power of
# two textures
test-report-npot perf-report-npot full-report-npot:	${TEST_PROGS}
	@test -z "${TEST_PROGS}" || { \
	  export COGL_DEBUG="$COGL_DEBUG,disable-npot-textures"; \
	  export GTESTER_LOGDIR=`mktemp -d "$(srcdir)/.testlogs-XXXXXX"` ; \
	  if test -d "$(top_srcdir)/.git"; then \
	    export REVISION="`git describe`" ;  \
	  else \
	    export REVISION="$(VERSION) $(CLUTTER_RELEASE_STATUS)" ; \
	  fi ; \
	  export TIMESTAMP=`date +%Y-%m-%dT%H:%M:%S%z` ; \
	  case $@ in \
	  test-report-npot) test_options="-k";; \
	  perf-report-npot) test_options="-k -m=perf";; \
	  full-report-npot) test_options="-k -m=perf -m=slow";; \
	  esac ; \
	  $(top_srcdir)/tests/conform/run-tests.sh \
	    ./test-conformance$(EXEEXT) \
	    --verbose \
	    $$test_options \
	    -o `mktemp "$$GTESTER_LOGDIR/log-XXXXXX"` ; \
	  echo '<?xml version="1.0"?>'              > $@.xml ; \
	  echo '<report-collection>'               >> $@.xml ; \
	  echo '<info>'                            >> $@.xml ; \
	  echo '  <package>$(PACKAGE)</package>'   >> $@.xml ; \
	  echo '  <version>$(VERSION)</version>'   >> $@.xml ; \
	  echo "  <revision>$$REVISION</revision>" >> $@.xml ; \
	  echo "  <date>$$TIMESTAMP</date>"        >> $@.xml ; \
	  echo '</info>'                           >> $@.xml ; \
	  for lf in `ls -L "$$GTESTER_LOGDIR"/.` ; do \
	    sed '1,1s/^<?xml\b[^>?]*?>//' <"$$GTESTER_LOGDIR"/"$$lf" >> $@.xml ; \
	  done ; \
	  echo >> $@.xml ; \
	  echo '</report-collection>' >> $@.xml ; \
	  ${GTESTER_REPORT} --version 2>/dev/null 1>&2 ; test "$$?" != 0 || ${GTESTER_REPORT} $@.xml >$@.html ; \
	  rm -rf "$$GTESTER_LOGDIR" ; \
	}

XML_REPORTS = \
	test-report.xml 	\
	perf-report.xml 	\
	full-report.xml 	\
	test-report-npot.xml 	\
	perf-report-npot.xml 	\
	full-report-npot.xml

HTML_REPORTS = \
	test-report.html 	\
	perf-report.html 	\
	full-report.html 	\
	test-report-npot.html 	\
	perf-report-npot.html 	\
	full-report-npot.html

EXTRA_DIST = ADDING_NEW_TESTS test-launcher.sh.in run-tests.sh
DISTCLEANFILES = test-launcher.sh .gitignore

# we override the clean-generic target to clean up the wrappers so
# we cannot use CLEANFILES
clean-generic: clean-wrappers
	$(QUIET_RM)rm -f $(XML_REPORTS) $(HTML_REPORTS)
