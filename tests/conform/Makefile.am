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
	timeline.c			\
	timeline-interpolate.c 		\
	timeline-progress.c		\
	timeline-rewind.c 		\
	$(NULL)

# actors tests
units_sources += \
	actor-graph.c			\
	actor-invariants.c 		\
	actor-iter.c			\
	actor-size.c			\
	binding-pool.c			\
	interval.c			\
	path.c 				\
        text.c             		\
	$(NULL)

# objects tests
units_sources += \
	color.c				\
	units.c				\
        $(NULL)

# cally tests
units_sources += \
	cally-text.c			\
	$(NULL)

# events tests
units_sources += \
	events-touch.c			\
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
		( echo "test-conformance-clutter$(EXEEXT) -p $$i" ) > $(top_builddir)/build/win32/$$unit-clutter.bat ; \
		( echo "test-conformance-clutter$(EXEEXT) -p $$i" ) >> $(top_builddir)/build/win32/test-conformance-clutter.bat ; \
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
	&& rm -f $(top_builddir)/build/win32/*.bat \
	&& rm -f stamp-test-conformance

# NB: BUILT_SOURCES here a misnomer. We aren't building source, just inserting
# a phony rule that will generate symlink scripts for running individual tests
BUILT_SOURCES = wrappers

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter

test_conformance_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_ENABLE_EXPERIMENTAL_API \
	-DG_DISABLE_DEPRECATION_WARNINGS \
	-DCLUTTER_DISABLE_DEPRECATION_WARNINGS \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\"

test_conformance_CFLAGS = -g $(CLUTTER_CFLAGS)

test_conformance_LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la $(CLUTTER_LIBS) -lm

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

dist-hook: $(top_builddir)/build/win32/vs9/test-conformance-clutter.vcproj $(top_builddir)/build/win32/vs10/test-conformance-clutter.vcxproj $(top_builddir)/build/win32/vs10/test-conformance-clutter.vcxproj.filters

$(top_builddir)/build/win32/vs9/test-conformance-clutter.vcproj: $(top_srcdir)/build/win32/vs9/test-conformance-clutter.vcprojin
	@for F in $(test_conformance_SOURCES); do \
		case $$F in \
		*.c)	echo '   <File RelativePath="..\..\..\tests\conform\'$$F'" />' \
			;; \
		esac; \
	done > testconformance.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/test-conformance-clutter.vcprojin >$@
	rm -f testconformance.sourcefiles

$(top_builddir)/build/win32/vs10/test-conformance-clutter.vcxproj: $(top_srcdir)/build/win32/vs10/test-conformance-clutter.vcxprojin
	@for F in $(test_conformance_SOURCES); do \
		case $$F in \
		*.c)	echo '    <ClCompile Include="..\..\..\tests\conform\'$$F'" />' \
			;; \
		esac; \
	done >testconformance.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/test-conformance-clutter.vcxprojin >$@
	rm -f testconformance.vs10.sourcefiles
	
$(top_builddir)/build/win32/vs10/test-conformance-clutter.vcxproj.filters: $(top_srcdir)/build/win32/vs10/test-conformance-clutter.vcxproj.filtersin
	@for F in $(test_conformance_SOURCES); do \
		case $$F in \
		*.c)	echo '    <ClCompile Include="..\..\..\tests\conform\'$$F'"><Filter>Sources</Filter></ClCompile>' \
			;; \
		esac; \
	done > testconformance.vs10.sourcefiles.filters
	$(CPP) -P - < $(top_srcdir)/build/win32/vs10/test-conformance-clutter.vcxproj.filtersin > $@
	rm -f testconformance.vs10.sourcefiles.filters

# Let the VS9/VS10 Project files be cleared out before they are re-expanded...
DISTCLEANFILES += \
	$(top_builddir)/build/win32/vs9/test-conformance-clutter.vcproj	\
	$(top_builddir)/build/win32/vs10/test-conformance-clutter.vcxproj \
	$(top_builddir)/build/win32/vs10/test-conformance-clutter.vcxproj.filters

# we override the clean-generic target to clean up the wrappers so
# we cannot use CLEANFILES
clean-generic: clean-wrappers clean-tests
	$(QUIET_RM)rm -f $(XML_REPORTS) $(HTML_REPORTS)

if ENABLE_INSTALLED_TESTS
# installed tests
insttestdir = $(libexecdir)/installed-tests/$(PACKAGE)/conform
insttest_PROGRAMS = test-conformance

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(wildcard *.test)

BUILT_SOURCES += tests
endif

.PHONY: tests clean-tests

tests: stamp-test-conformance
	@for i in `cat unit-tests`; do \
		unit=`basename $$i | sed -e s/_/-/g`; \
		echo "  GEN      $$unit"; \
		echo "[Test]" > $$unit.test; \
		echo "Type=session" >> $$unit.test; \
		echo "Exec=$(libexecdir)/installed-tests/$(PACKAGE)/conform/test-conformance -p $$i" >> $$unit.test; \
	done

clean-tests:
	$(QUIET_RM) rm -f *.test
