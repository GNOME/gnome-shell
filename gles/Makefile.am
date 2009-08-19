libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
libclutterinclude_HEADERS = \
        $(top_builddir)/clutter/cogl/cogl.h \
        $(top_builddir)/clutter/cogl/cogl-defines-gles.h \
        $(top_builddir)/clutter/cogl/cogl-color.h \
        $(top_builddir)/clutter/cogl/cogl-deprecated.h \
        $(top_builddir)/clutter/cogl/cogl-fixed.h \
        $(top_builddir)/clutter/cogl/cogl-offscreen.h \
        $(top_builddir)/clutter/cogl/cogl-path.h \
        $(top_builddir)/clutter/cogl/cogl-shader.h \
        $(top_builddir)/clutter/cogl/cogl-bitmap.h \
        $(top_builddir)/clutter/cogl/cogl-texture.h \
        $(top_builddir)/clutter/cogl/cogl-types.h \
        $(top_builddir)/clutter/cogl/cogl-vertex-buffer.h \
        $(top_builddir)/clutter/cogl/cogl-material.h \
        $(top_builddir)/clutter/cogl/cogl-matrix.h \
        $(top_builddir)/clutter/cogl/cogl-debug.h

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_srcdir)/clutter/cogl/common \
	-I$(top_srcdir)/clutter/cogl/$(CLUTTER_COGL) \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
	-DG_DISABLE_SINGLE_INCLUDES \
        -DG_LOG_DOMAIN=\"Cogl-GLES\" \
        -DCLUTTER_COMPILATION

noinst_LTLIBRARIES = libclutter-cogl-gles.la

libclutter_cogl_gles_la_CPPFLAGS = $(CLUTTER_CFLAGS) $(COGL_DEBUG_CFLAGS) $(CLUTTER_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS)
libclutter_cogl_gles_la_LIBADD = -lm $(CLUTTER_LIBS) $(top_builddir)/clutter/cogl/common/libclutter-cogl-common.la
libclutter_cogl_gles_la_DEPENDENCIES = $(top_builddir)/clutter/cogl/common/libclutter-cogl-common.la
libclutter_cogl_gles_la_SOURCES = \
	$(top_builddir)/clutter/cogl/cogl.h \
	$(top_builddir)/clutter/cogl/cogl-defines-gles.h \
        $(top_builddir)/clutter/cogl/cogl-color.h \
        $(top_builddir)/clutter/cogl/cogl-deprecated.h \
        $(top_builddir)/clutter/cogl/cogl-fixed.h \
        $(top_builddir)/clutter/cogl/cogl-offscreen.h \
        $(top_builddir)/clutter/cogl/cogl-path.h \
        $(top_builddir)/clutter/cogl/cogl-shader.h \
        $(top_builddir)/clutter/cogl/cogl-bitmap.h \
        $(top_builddir)/clutter/cogl/cogl-texture.h \
        $(top_builddir)/clutter/cogl/cogl-types.h \
        $(top_builddir)/clutter/cogl/cogl-debug.h \
	cogl-texture-private.h \
	cogl-fbo.h \
	cogl-context.h \
	cogl.c \
	cogl-primitives.c \
	cogl-texture.c \
	cogl-fbo.c \
	cogl-context.c \
	cogl-gles2-wrapper.h \
	cogl-program.h \
	cogl-program.c \
	cogl-shader-private.h \
	cogl-shader.c

if USE_GLES2_WRAPPER
libclutter_cogl_gles_la_SOURCES += \
	cogl-gles2-wrapper.c \
	cogl-fixed-vertex-shader.h \
	cogl-fixed-vertex-shader.c \
	cogl-fixed-fragment-shader.h \
	cogl-fixed-fragment-shader.c
endif

EXTRA_DIST = cogl-defines.h.in \
	stringify.sh \
	cogl-fixed-vertex-shader.glsl \
	cogl-fixed-fragment-shader.glsl

BUILT_SOURCES = \
	cogl-fixed-vertex-shader.h \
	cogl-fixed-vertex-shader.c \
	cogl-fixed-fragment-shader.h \
	cogl-fixed-fragment-shader.c

%.h: $(srcdir)/%.glsl
	/bin/sh $(srcdir)/stringify.sh -h $< > $@
%.c: $(srcdir)/%.glsl
	/bin/sh $(srcdir)/stringify.sh $< > $@

