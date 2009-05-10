INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_srcdir)/clutter/cogl/common \
	-I$(top_srcdir)/clutter/cogl/$(CLUTTER_COGL) \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
        -DG_LOG_DOMAIN=\"Cogl-Common\" \
        -DCLUTTER_COMPILATION

noinst_LTLIBRARIES = libclutter-cogl-common.la
EXTRA_DIST = stb_image.c

libclutter_cogl_common_la_CPPFLAGS = $(CLUTTER_CFLAGS) $(COGL_DEBUG_CFLAGS) $(CLUTTER_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS)
libclutter_cogl_common_la_LIBADD = -lm $(CLUTTER_LIBS)
libclutter_cogl_common_la_SOURCES = 	\
	cogl-handle.h 			\
	cogl-internal.h			\
	cogl.c				\
	cogl-util.h 			\
	cogl-util.c 			\
	cogl-bitmap-private.h 		\
	cogl-bitmap.c 			\
	cogl-bitmap-fallback.c 		\
	cogl-current-matrix.c		\
	cogl-current-matrix.h		\
	cogl-primitives.h 		\
	cogl-primitives.c 		\
	cogl-bitmap-pixbuf.c 		\
	cogl-clip-stack.h 		\
	cogl-clip-stack.c               \
        cogl-fixed.c                    \
        cogl-color.c			\
	cogl-vertex-buffer-private.h 	\
	cogl-vertex-buffer.c		\
	cogl-matrix.c			\
	cogl-matrix-stack.c		\
	cogl-matrix-stack.h		\
	cogl-material.c			\
	cogl-material-private.h		\
	cogl-blend-string.c		\
	cogl-blend-string.h		\
	cogl-debug.c
