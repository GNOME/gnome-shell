libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
libclutterinclude_HEADERS = \
        $(top_builddir)/clutter/cogl/cogl.h \
        $(top_builddir)/clutter/cogl/cogl-defines-gl.h \
        $(top_builddir)/clutter/cogl/cogl-color.h \
        $(top_builddir)/clutter/cogl/cogl-fixed.h \
        $(top_builddir)/clutter/cogl/cogl-offscreen.h \
        $(top_builddir)/clutter/cogl/cogl-path.h \
        $(top_builddir)/clutter/cogl/cogl-shader.h \
        $(top_builddir)/clutter/cogl/cogl-texture.h \
        $(top_builddir)/clutter/cogl/cogl-types.h

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_srcdir)/clutter/cogl/common \
	-I$(top_srcdir)/clutter/cogl/$(CLUTTER_COGL) \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
        -DCLUTTER_COMPILATION \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-cogl.la

libclutter_cogl_la_SOURCES = \
	$(top_builddir)/clutter/cogl/cogl.h \
	$(top_builddir)/clutter/cogl/cogl-defines-gl.h \
        $(top_builddir)/clutter/cogl/cogl-color.h \
        $(top_builddir)/clutter/cogl/cogl-fixed.h \
        $(top_builddir)/clutter/cogl/cogl-offscreen.h \
        $(top_builddir)/clutter/cogl/cogl-path.h \
        $(top_builddir)/clutter/cogl/cogl-shader.h \
        $(top_builddir)/clutter/cogl/cogl-texture.h \
        $(top_builddir)/clutter/cogl/cogl-types.h \
	cogl-internal.h \
	cogl-texture-private.h \
	cogl-fbo.h \
	cogl-shader-private.h \
	cogl-program.h \
	cogl-context.h \
	cogl.c \
	cogl-primitives.c \
	cogl-texture.c \
	cogl-fbo.c \
	cogl-shader.c \
	cogl-program.c \
	cogl-context.c

EXTRA_DIST = cogl-defines.h.in

libclutter_cogl_la_LIBADD = $(top_builddir)/clutter/cogl/common/libclutter-cogl-common.la
