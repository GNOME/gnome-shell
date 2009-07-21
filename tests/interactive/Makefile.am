
UNIT_TESTS = \
	test-textures.c \
	test-texture-async.c \
	test-events.c \
	test-offscreen.c \
	test-scale.c \
        test-actors.c \
        test-actor-clone.c \
	test-behave.c \
	test-project.c \
	test-rotate.c \
	test-depth.c \
	test-threads.c \
	test-score.c \
	test-script.c \
	test-model.c \
	test-grab.c \
	test-fullscreen.c \
	test-shader.c \
	test-unproject.c \
	test-viewport.c \
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
	test-random-text.c \
	test-clip.c \
	test-paint-wrapper.c \
	test-texture-quality.c \
	test-layout.c \
        test-animation.c \
        test-easing.c \
        test-binding-pool.c \
        test-text.c \
        test-text-field.c \
	test-clutter-cairo-flowers.c \
	test-cogl-vertex-buffer.c

if X11_TESTS
UNIT_TESTS += test-pixmap.c
UNIT_TESTS += test-devices.c
endif

#FIXME - this is is a bit of a yukky way of ensuring the tests find our data:
test-script.json:
	ln -sf $(top_srcdir)/tests/data/test-script.json
redhand.png:
	ln -sf $(top_srcdir)/tests/data/redhand.png
redhand_alpha.png:
	ln -sf $(top_srcdir)/tests/data/redhand_alpha.png
light0.png:
	ln -sf $(top_srcdir)/tests/data/light0.png
light1.png:
	ln -sf $(top_srcdir)/tests/data/light1.png

# For convenience, this provides a way to easily run individual unit tests:
.PHONY: wrappers
wrappers: test-interactive$(EXEEXT)
	for i in $(UNIT_TESTS); \
	do \
		ln -sf $(top_srcdir)/tests/interactive/wrapper.sh $${i%*.c}; \
	done

# NB: BUILT_SOURCES here a misnomer. We aren't building source, just inserting
# a phony rule that will generate symlink scripts for running individual tests
BUILT_SOURCES = wrappers redhand.png redhand_alpha.png light0.png light1.png test-script.json

INCLUDES = \
	-I$(top_srcdir)	\
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter \
	-DG_DISABLE_SINGLE_INCLUDES

common_ldadd = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@.la

noinst_PROGRAMS = test-interactive

test_interactive_SOURCES = test-main.c $(UNIT_TESTS)
test_interactive_CPPFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)
test_interactive_LDFLAGS = -export-dynamic
test_interactive_LDADD = $(CLUTTER_LIBS) $(common_ldadd)
