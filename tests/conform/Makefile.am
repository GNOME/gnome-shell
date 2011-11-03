include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

noinst_PROGRAMS = test-conformance

common_sources = \
	test-utils.h \
	test-utils.c \
	test-conform-main.c \
	$(NULL)

unported_test_sources = \
	test-fixed.c \
	test-materials.c \
	test-viewport.c \
	test-multitexture.c \
	test-npot-texture.c \
	test-object.c \
	test-offscreen.c \
	test-pixel-buffer.c \
	test-premult.c \
	test-readpixels.c \
	test-texture-3d.c \
	test-texture-get-set-data.c \
	test-texture-mipmaps.c \
	test-texture-pixmap-x11.c \
	test-texture-rectangle.c \
	test-atlas-migration.c \
	test-vertex-buffer-contiguous.c \
	test-vertex-buffer-interleved.c \
	test-vertex-buffer-mutability.c \
	test-primitive.c \
	$(NULL)

test_sources = \
	test-bitmask.c \
	test-blend-strings.c \
	test-depth-test.c \
	test-color-mask.c \
	test-backface-culling.c \
	test-just-vertex-shader.c \
	test-path.c \
	test-pipeline-user-matrix.c \
	test-pipeline-uniforms.c \
	test-wrap-modes.c \
	test-sub-texture.c \
	$(NULL)

test_conformance_SOURCES = $(common_sources) $(test_sources)

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
		-e 's/^ \{1,\}ADD_TEST *(.*"\([^",]\{1,\}\)", *\([a-zA-Z0-9_]\{1,\}\).*/\/conform\1\/\2/p' \
		-e 's/^ \{1,\}ADD_CONDITIONAL_TEST *(.*"\([^",]\{1,\}\)", *\([a-zA-Z0-9_]\{1,\}\).*/\/conform\1\/\2/p' \
		-e 's/^ \{1,\}ADD_TODO_TEST *(.*"\([^",]\{1,\}\)", *\([a-zA-Z0-9_]\{1,\}\).*/\/conform\1\/\2/p' \
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

# The include of the $(buildir)/cogl directory here is to make it so
# that tests that directly include Cogl source code for whitebox
# testing (such as test-bitmask) will still compile
INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir)/cogl

test_conformance_CPPFLAGS = \
	-DCOGL_ENABLE_EXPERIMENTAL_API \
	-DCOGL_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\" \
	-DCLUTTER_COMPILATION

test_conformance_CFLAGS = -g3 -O0 $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)
test_conformance_LDADD = $(COGL_DEP_LIBS) $(top_builddir)/cogl/libcogl.la
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

EXTRA_DIST = test-launcher.sh.in run-tests.sh
DISTCLEANFILES = test-launcher.sh .gitignore

# we override the clean-generic target to clean up the wrappers so
# we cannot use CLEANFILES
clean-generic: clean-wrappers
	$(QUIET_RM)rm -f $(XML_REPORTS) $(HTML_REPORTS)
