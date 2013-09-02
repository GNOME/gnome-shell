include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

noinst_PROGRAMS = test-conformance

common_sources = \
	test-conform-main.c \
	$(NULL)

unported_test_sources = \
	test-fixed.c \
	test-materials.c \
	test-viewport.c \
	test-multitexture.c \
	test-npot-texture.c \
	test-object.c \
	test-readpixels.c \
	test-texture-mipmaps.c \
	test-texture-pixmap-x11.c \
	test-texture-rectangle.c \
	test-vertex-buffer-contiguous.c \
	test-vertex-buffer-interleved.c \
	test-vertex-buffer-mutability.c \
	$(NULL)

test_sources = \
	test-atlas-migration.c \
	test-blend-strings.c \
	test-blend.c \
	test-depth-test.c \
	test-color-hsl.c \
	test-color-mask.c \
	test-backface-culling.c \
	test-just-vertex-shader.c \
	test-pipeline-user-matrix.c \
	test-pipeline-uniforms.c \
	test-pixel-buffer.c \
	test-premult.c \
	test-snippets.c \
	test-wrap-modes.c \
	test-sub-texture.c \
	test-custom-attributes.c \
	test-offscreen.c \
	test-primitive.c \
	test-texture-3d.c \
	test-sparse-pipeline.c \
	test-read-texture-formats.c \
	test-write-texture-formats.c \
	test-point-size.c \
	test-point-size-attribute.c \
	test-point-sprite.c \
	test-no-gl-header.c \
	test-version.c \
	test-gles2-context.c \
	test-euler-quaternion.c \
	test-layer-remove.c \
	test-alpha-test.c \
	test-map-buffer-range.c \
	test-npot-texture.c \
	test-alpha-textures.c \
	test-wrap-rectangle-textures.c \
	test-texture-get-set-data.c \
	test-framebuffer-get-bits.c \
	test-primitive-and-journal.c \
	test-copy-replace-texture.c \
	test-pipeline-cache-unrefs-texture.c \
	test-texture-no-allocate.c \
	$(NULL)

if !USING_EMSCRIPTEN
# test-fence depends on the glib mainloop so it won't compile if using
# emscripten which builds in standalone mode.
test_sources += test-fence.c
endif

if BUILD_COGL_PATH
test_sources += test-path.c
endif

test_conformance_SOURCES = $(common_sources) $(test_sources)

if OS_WIN32
SHEXT =
else
SHEXT = $(EXEEXT)
endif

# For convenience, this provides a way to easily run individual unit tests:
.PHONY: wrappers clean-wrappers

wrappers: stamp-test-conformance
	@true
stamp-test-conformance: Makefile $(srcdir)/test-conform-main.c
	@mkdir -p wrappers
	@sed -n -e 's/^ \{1,\}ADD_TEST *( *\([a-zA-Z0-9_]\{1,\}\).*/\1/p' $(srcdir)/test-conform-main.c > unit-tests
	@chmod +x $(top_srcdir)/tests/test-launcher.sh
	@( echo "/stamp-test-conformance" ; \
	   echo "/test-conformance$(EXEEXT)" ; \
	   echo "*.o" ; \
	   echo ".gitignore" ; \
	   echo "unit-tests" ; ) > .gitignore
	@for i in `cat unit-tests`; \
	do \
		unit=`basename $$i | sed -e s/_/-/g`; \
		echo "  GEN    $$unit"; \
		( echo "#!/bin/sh" ; echo "$(top_srcdir)/tests/test-launcher.sh $(abs_builddir)/test-conformance$(EXEEXT) '' '$$i' \"\$$@\"" ) > $$unit$(SHEXT) ; \
		chmod +x $$unit$(SHEXT); \
		echo "/$$unit$(SHEXT)" >> .gitignore; \
	done \
	&& echo timestamp > $(@F)

clean-wrappers:
	@for i in `cat unit-tests`; \
	do \
		unit=`basename $$i | sed -e s/_/-/g`; \
		echo "  RM     $$unit"; \
		rm -f $$unit$(SHEXT) ; \
	done \
	&& rm -f unit-tests \
	&& rm -f stamp-test-conformance

# NB: BUILT_SOURCES here a misnomer. We aren't building source, just inserting
# a phony rule that will generate symlink scripts for running individual tests
BUILT_SOURCES = wrappers

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir)/cogl \
	-I$(top_srcdir)/test-fixtures

if !USE_GLIB
AM_CPPFLAGS += -I$(top_builddir)/deps/glib
endif

AM_CPPFLAGS += \
	-DCOGL_ENABLE_EXPERIMENTAL_API \
	-DCOGL_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\" \
	-DCOGL_COMPILATION

test_conformance_CFLAGS = -g3 -O0 $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)
test_conformance_LDADD = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/cogl/libcogl.la \
	$(LIBM)
if !USE_GLIB
test_conformance_LDADD += $(top_builddir)/deps/glib/libglib.la
endif
if BUILD_COGL_PATH
test_conformance_LDADD += $(top_builddir)/cogl-path/libcogl-path.la
endif
test_conformance_LDFLAGS = -export-dynamic

test: wrappers
	@$(top_srcdir)/tests/run-tests.sh $(abs_builddir)/../config.env $(abs_builddir)/test-conformance$(EXEEXT)

# XXX: we could prevent the conformance test suite from running
#      by simply defining this variable conditionally
TEST_PROGS = test-conformance

.PHONY: test

DISTCLEANFILES = .gitignore

dist-hook: ../../build/win32/vs9/test-conformance-cogl.vcproj ../../build/win32/vs10/test-conformance-cogl.vcxproj ../../build/win32/vs10/test-conformance-cogl.vcxproj.filters

../../build/win32/vs9/test-conformance-cogl.vcproj: $(top_srcdir)/build/win32/vs9/test-conformance-cogl.vcprojin
	for F in $(test_conformance_SOURCES); do \
		case $$F in \
		*.c)	echo '   <File RelativePath="..\..\..\tests\conform\'$$F'" />' \
			;; \
		esac; \
	done >testconformance.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/test-conformance-cogl.vcprojin >$@
	rm testconformance.sourcefiles

../../build/win32/vs10/test-conformance-cogl.vcxproj: $(top_srcdir)/build/win32/vs10/test-conformance-cogl.vcxprojin
	for F in $(test_conformance_SOURCES); do \
		case $$F in \
		*.c)	echo '    <ClCompile Include="..\..\..\tests\conform\'$$F'" />' \
			;; \
		esac; \
	done >testconformance.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/test-conformance-cogl.vcxprojin >$@
	rm testconformance.vs10.sourcefiles

../../build/win32/vs10/test-conformance-cogl.vcxproj.filters: $(top_srcdir)/build/win32/vs10/test-conformance-cogl.vcxproj.filtersin
	for F in $(test_conformance_SOURCES); do \
		case $$F in \
		*.c)	echo '    <ClCompile Include="..\..\..\tests\conform\'$$F'"><Filter>Sources</Filter></ClCompile>' \
			;; \
		esac; \
	done >testconformance.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/test-conformance-cogl.vcxproj.filtersin >$@
	rm testconformance.vs10.sourcefiles.filters

# Let the VS9/VS10 Project files be cleared out before they are re-expanded...
DISTCLEANFILES += ../../build/win32/vs9/test-conformance-cogl.vcproj	\
		  ../../build/win32/vs10/test-conformance-cogl.vcxproj	\
		  ../../build/win32/vs10/test-conformance-cogl.vcxproj.filters

# we override the clean-generic target to clean up the wrappers so
# we cannot use CLEANFILES
clean-generic: clean-wrappers
	$(QUIET_RM)rm -f .log


if ENABLE_INSTALLED_TESTS

insttestdir = $(libexecdir)/installed-tests/$(PACKAGE)/conform
insttest_PROGRAMS = test-conformance
insttest_DATA = unit-tests

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = conform.test

conform.test:
	echo "  GEN      $@"; \
	echo "[Test]" > $@.tmp; \
	echo "Type=session" >> $@.tmp; \
	echo "Exec=sh -c \"cd $(libexecdir)/installed-tests/$(PACKAGE)/conform; ../run-tests.sh ../config.env ./test-conformance\"" >> $@.tmp; \
	mv $@.tmp $@

CLEANFILES = conform.test

endif
