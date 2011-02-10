include $(top_srcdir)/build/autotools/Makefile.am.silent

UNIT_TESTS = \
	test-textures.c \
	test-texture-async.c \
	test-texture-material.c \
	test-events.c \
	test-scale.c \
        test-actors.c \
        test-actor-clone.c \
	test-behave.c \
	test-shader-effects.c \
	test-depth.c \
	test-threads.c \
	test-script.c \
	test-model.c \
	test-grab.c \
	test-fullscreen.c \
	test-shader.c \
	test-cogl-shader-arbfp.c \
	test-cogl-shader-glsl.c \
	test-animator.c \
	test-state.c \
	test-state-animator.c \
	test-unproject.c \
	test-fbo.c \
	test-multistage.c \
	test-cogl-primitives.c \
	test-cogl-tex-tile.c \
	test-cogl-tex-convert.c \
	test-cogl-tex-foreign.c \
	test-cogl-tex-getset.c \
	test-cogl-offscreen.c \
	test-cogl-tex-polygon.c \
	test-cogl-multitexture.c \
	test-stage-read-pixels.c \
	test-clip.c \
	test-paint-wrapper.c \
	test-texture-quality.c \
	test-layout.c \
        test-animation.c \
        test-easing.c \
        test-binding-pool.c \
        test-text.c \
        test-text-field.c \
	test-cairo-flowers.c \
	test-cogl-vertex-buffer.c \
	test-bin-layout.c \
	test-flow-layout.c \
	test-box-layout.c \
	test-stage-sizing.c \
	test-drag.c \
	test-constraints.c \
	test-scrolling.c \
	test-cogl-point-sprites.c \
	test-table-layout.c \
	test-path-constraint.c \
	test-snap-constraint.c

if X11_TESTS
UNIT_TESTS += test-pixmap.c test-devices.c
endif

if OS_WIN32
SHEXT =
else
SHEXT = $(EXEEXT)
endif

# For convenience, this provides a way to easily run individual unit tests:
wrappers: stamp-test-interactive
	@true
stamp-test-interactive: Makefile test-interactive$(EXEEXT)
	@wrapper=$(abs_builddir)/wrapper.sh ; \
	chmod +x $$wrapper && \
	( echo "/stamp-test-interactive" ; \
	  echo "/test-interactive" ; \
	  echo "*.o" ; \
	  echo ".gitignore" ) > .gitignore ; \
	for i in $(UNIT_TESTS); \
	do \
		test_bin=$${i%*.c} ; \
		echo "  GEN    $$test_bin" ; \
		( echo "#!/bin/sh" ; \
		  echo "$$wrapper $$test_bin \$$@" \
		) > $$test_bin$(SHEXT) ; \
		chmod +x $$test_bin$(SHEXT) ; \
		echo "/$$test_bin$(SHEXT)" >> .gitignore ; \
	done \
	&& echo timestamp > $(@F)

clean-wrappers:
	@for i in $(UNIT_TESTS); \
	do \
		test_bin=$${i%*.c} ; \
		echo "  RM    $$test_bin"; \
		rm -f $$test_bin$(SHEXT); \
	done \
	&& rm -f stamp-test-interactive

.PHONY: wrappers clean-wrappers

INCLUDES = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/clutter 	\
	-I$(top_srcdir)/clutter/cogl 	\
	-I$(top_builddir)/clutter 	\
	-I$(top_builddir)/clutter/cogl

common_ldadd = $(top_builddir)/clutter/libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la


noinst_PROGRAMS = test-interactive

test_interactive_SOURCES = test-main.c $(UNIT_TESTS)
test_interactive_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)
test_interactive_CPPFLAGS = \
	-DTESTS_DATADIR=\""$(abs_top_srcdir)/tests/data"\" \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED \
	-DCLUTTER_DISABLE_DEPRECATED
test_interactive_LDFLAGS = -export-dynamic
test_interactive_LDADD = $(CLUTTER_LIBS) $(common_ldadd)

EXTRA_DIST = wrapper.sh.in
DISTCLEANFILES = wrapper.sh .gitignore

BUILT_SOURCES = wrappers

clean-local: clean-wrappers
