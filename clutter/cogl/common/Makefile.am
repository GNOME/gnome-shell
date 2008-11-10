INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_srcdir)/clutter/cogl/$(CLUTTER_COGL) \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
        -DCLUTTER_COMPILATION \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-cogl-common.la
EXTRA_DIST = stb_image.c

libclutter_cogl_common_la_SOURCES = 	\
	cogl-handle.h 			\
	cogl-util.h 			\
	cogl-util.c 			\
	cogl-bitmap.h 			\
	cogl-bitmap.c 			\
	cogl-bitmap-fallback.c 		\
	cogl-primitives.h 		\
	cogl-primitives.c 		\
	cogl-bitmap-pixbuf.c 		\
	cogl-clip-stack.h 		\
	cogl-clip-stack.c               \
        cogl-fixed.c                    \
        cogl-color.c			\
	cogl-mesh.c			\
	cogl-mesh.h
