NULL =

cogl_headers = \
	$(top_builddir)/clutter/cogl/cogl-defines-gl.h	\
	$(top_srcdir)/clutter/cogl/cogl-color.h 	\
	$(top_srcdir)/clutter/cogl/cogl-deprecated.h 	\
	$(top_srcdir)/clutter/cogl/cogl-fixed.h 	\
	$(top_srcdir)/clutter/cogl/cogl-offscreen.h 	\
	$(top_srcdir)/clutter/cogl/cogl-path.h 		\
	$(top_srcdir)/clutter/cogl/cogl-shader.h 	\
	$(top_srcdir)/clutter/cogl/cogl-bitmap.h 	\
	$(top_srcdir)/clutter/cogl/cogl-texture.h 	\
	$(top_srcdir)/clutter/cogl/cogl-types.h 	\
	$(top_srcdir)/clutter/cogl/cogl-vertex-buffer.h \
	$(top_srcdir)/clutter/cogl/cogl-material.h 	\
	$(top_srcdir)/clutter/cogl/cogl-matrix.h 	\
	$(top_srcdir)/clutter/cogl/cogl-debug.h		\
	$(NULL)

cogl_priv_headers = \
	cogl-texture-private.h 	\
	cogl-fbo.h 		\
	cogl-shader-private.h 	\
	cogl-program.h 		\
	cogl-context.h		\
	$(NULL)

cogl_sources = \
	cogl.c 			\
	cogl-primitives.c 	\
	cogl-texture.c 		\
	cogl-fbo.c 		\
	cogl-shader.c 		\
	cogl-program.c 		\
	cogl-context.c		\
	$(NULL)

coglincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
coglinclude_HEADERS = \
	$(cogl_headers) \
	$(top_builddir)/clutter/cogl/cogl.h

INCLUDES = \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_srcdir)/clutter/cogl/common \
	-I$(top_srcdir)/clutter/cogl/$(CLUTTER_COGL) \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
	-DG_LOG_DOMAIN=\"Cogl-GL\" \
	-DCLUTTER_COMPILATION

noinst_LTLIBRARIES = libclutter-cogl.la

libclutter_cogl_la_CPPFLAGS = $(CLUTTER_CFLAGS) $(COGL_DEBUG_CFLAGS) $(CLUTTER_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS)
libclutter_cogl_la_LIBADD = -lm $(CLUTTER_LIBS) $(top_builddir)/clutter/cogl/common/libclutter-cogl-common.la
libclutter_cogl_la_DEPENDENCIES = $(top_builddir)/clutter/cogl/common/libclutter-cogl-common.la
libclutter_cogl_la_SOURCES = \
	$(top_builddir)/clutter/cogl/cogl.h 	\
	$(cogl_headers) 			\
	$(cogl_priv_headers)			\
	$(cogl_sources)				\
	$(NULL)

EXTRA_DIST = cogl-defines.h.in
