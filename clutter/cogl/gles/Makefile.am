libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
libclutterinclude_HEADERS = $(top_builddir)/clutter/cogl/cogl.h \
                            $(top_builddir)/clutter/cogl/cogl-defines-gles.h

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_srcdir)/clutter/cogl/common \
	-I$(top_srcdir)/clutter/cogl/$(CLUTTER_COGL) \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-cogl.la

libclutter_cogl_la_SOURCES = \
	$(top_builddir)/clutter/cogl/cogl.h \
	$(top_builddir)/clutter/cogl/gles/cogl-defines.h \
        cogl-defines.h \
	cogl-internal.h \
	cogl-primitives.h \
	cogl-texture.h \
	cogl-fbo.h \
	cogl-context.h \
	cogl.c \
	cogl-primitives.c \
	cogl-texture.c \
	cogl-fbo.c \
	cogl-context.c

libclutter_cogl_la_LIBADD =  $(top_builddir)/clutter/cogl/common/libclutter-cogl-common.la
