include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_srcdir)/clutter/cogl/common \
	-I$(top_srcdir)/clutter/cogl/$(CLUTTER_COGL) \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
	-DG_DISABLE_SINGLE_INCLUDES \
        -DG_LOG_DOMAIN=\"Cogl-Common\" \
        -DCLUTTER_COMPILATION

cogl_public_h = \
	$(top_srcdir)/clutter/cogl/cogl-bitmap.h	\
	$(top_srcdir)/clutter/cogl/cogl-color.h		\
	$(top_srcdir)/clutter/cogl/cogl-debug.h		\
	$(top_srcdir)/clutter/cogl/cogl-fixed.h		\
	$(top_srcdir)/clutter/cogl/cogl-material.h	\
	$(top_srcdir)/clutter/cogl/cogl-matrix.h	\
	$(top_srcdir)/clutter/cogl/cogl-offscreen.h	\
	$(top_srcdir)/clutter/cogl/cogl-path.h		\
	$(top_srcdir)/clutter/cogl/cogl-shader.h	\
	$(top_srcdir)/clutter/cogl/cogl-texture.h	\
	$(top_srcdir)/clutter/cogl/cogl-types.h		\
	$(top_srcdir)/clutter/cogl/cogl-vertex-buffer.h	\
	$(top_builddir)/clutter/cogl/cogl.h		\
	$(NULL)

noinst_LTLIBRARIES = libclutter-cogl-common.la

cogl-enum-types.h: stamp-cogl-enum-types.h
	@true
stamp-cogl-enum-types.h: $(cogl_public_h) Makefile
	$(QUIET_GEN)( $(GLIB_MKENUMS) \
		--template $(srcdir)/cogl-enum-types.h.in \
	$(cogl_public_h) ) > xgen-ceth \
	&& (cmp -s xgen-ceth cogl-enum-types.h || cp -f xgen-ceth cogl-enum-types.h) \
	&& cp -f cogl-enum-types.h $(top_builddir)/clutter/cogl/cogl-enum-types.h \
	&& rm -f xgen-ceth \
	&& echo timestamp > $(@F)

cogl-enum-types.c: cogl-enum-types.h
	$(QUIET_GEN)( $(GLIB_MKENUMS) \
		--template $(srcdir)/cogl-enum-types.c.in \
	$(cogl_public_h) ) > xgen-cetc \
	&& cp -f xgen-cetc cogl-enum-types.c \
	&& rm -f xgen-cetc

BUILT_SOURCES = cogl-enum-types.h cogl-enum-types.c

libclutter_cogl_common_la_CPPFLAGS = $(CLUTTER_CFLAGS) $(COGL_DEBUG_CFLAGS) $(CLUTTER_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS)
libclutter_cogl_common_la_LIBADD = -lm $(CLUTTER_LIBS)
libclutter_cogl_common_la_SOURCES = 	\
	$(top_builddir)/clutter/cogl/common/cogl-enum-types.h	\
	$(top_builddir)/clutter/cogl/common/cogl-enum-types.c	\
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
	cogl-debug.c			\
	$(NULL)

EXTRA_DIST = stb_image.c cogl-enum-types.h.in cogl-enum-types.c.in
CLEANFILES = stamp-cogl-enum-types.h
DISTCLEANFILES = cogl-enum-types.h cogl-enum-types.c
