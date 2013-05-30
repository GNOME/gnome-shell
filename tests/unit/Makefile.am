include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

noinst_PROGRAMS = test-unit

test_unit_SOURCES = test-unit-main.c

if OS_WIN32
SHEXT =
else
SHEXT = $(EXEEXT)
endif

# For convenience, this provides a way to easily run individual unit tests:
.PHONY: wrappers clean-wrappers

wrappers: stamp-test-unit
	@true
stamp-test-unit: Makefile test-unit
	@mkdir -p wrappers
	source $(top_builddir)/cogl/libcogl.la ; \
	  $(NM) $(top_builddir)/cogl/.libs/"$$dlname"| \
	  grep 'D unit_test_'|sed 's/.\+ D //' > unit-tests
	@chmod +x $(top_srcdir)/tests/test-launcher.sh
	@( echo "/stamp-test-unit" ; \
	   echo "/test-unit$(EXEEXT)" ; \
	   echo "*.o" ; \
	   echo ".gitignore" ; \
	   echo "unit-tests" ; ) > .gitignore
	@for i in `cat unit-tests`; \
	do \
		unit=`echo $$i | sed -e s/_/-/g | sed s/unit-test-//`; \
		echo "  GEN    $$unit"; \
		( echo "#!/bin/sh" ; echo "$(top_srcdir)/tests/test-launcher.sh $(abs_builddir)/test-unit$(EXEEXT) 'unit_test_' '$$i' \"\$$@\"" ) > $$unit$(SHEXT) ; \
		chmod +x $$unit$(SHEXT); \
		echo "/$$unit$(SHEXT)" >> .gitignore; \
	done \
	&& echo timestamp > $(@F)

clean-wrappers:
	@for i in `cat unit-tests`; \
	do \
		unit=`echo $$i | sed -e s/_/-/g | sed s/unit-test-//`; \
		echo "  RM     $$unit"; \
		rm -f $$unit$(SHEXT) ; \
	done \
	&& rm -f unit-tests \
	&& rm -f stamp-test-unit

# NB: BUILT_SOURCES here a misnomer. We aren't building source, just inserting
# a phony rule that will generate symlink scripts for running individual tests
BUILT_SOURCES = wrappers

# The include of the $(buildir)/cogl directory here is to make it so
# that tests that directly include Cogl source code for whitebox
# testing (such as test-bitmask) will still compile
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/test-fixtures \
	-I$(top_builddir)/cogl

if !USE_GLIB
AM_CPPFLAGS += -I$(top_builddir)/deps/glib
endif

AM_CPPFLAGS += \
	-DCOGL_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\" \
	-DCOGL_COMPILATION

test_unit_CFLAGS = -g3 -O0 $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)
test_unit_LDADD = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/cogl/libcogl.la \
	$(top_builddir)/test-fixtures/libtest-fixtures.la \
	$(LIBM)
if !USE_GLIB
test_unit_LDADD += $(top_builddir)/deps/glib/libglib.la
endif
test_unit_LDFLAGS = -export-dynamic

test: wrappers
	@$(top_srcdir)/tests/run-tests.sh $(abs_builddir)/../config.env $(abs_builddir)/test-unit$(EXEEXT)

# XXX: we could prevent the unit test suite from running
#      by simply defining this variable conditionally
TEST_PROGS = test-unit

.PHONY: test

DISTCLEANFILES = .gitignore

# we override the clean-generic target to clean up the wrappers so
# we cannot use CLEANFILES
clean-generic: clean-wrappers
	$(QUIET_RM)rm -f .log
