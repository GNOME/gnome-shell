include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

INCLUDES = \
	-I$(top_srcdir) \
	-I$(srcdir) \
	-I$(srcdir)/../.. \
	-I$(srcdir)/../../.. \
	-I$(srcdir)/winsys \
	-I$(srcdir)/driver/$(COGL_DRIVER) \
	-I../.. \
	-I../../.. \
	-DG_DISABLE_SINGLE_INCLUDES \
        -DG_LOG_DOMAIN=\"Cogl-Driver\" \
        -DCLUTTER_COMPILATION

noinst_LTLIBRARIES = libclutter-cogl-driver.la
libclutter_cogl_driver_la_CPPFLAGS = \
	$(CLUTTER_CFLAGS) \
	$(COGL_DEBUG_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(MAINTAINER_CFLAGS)
libclutter_cogl_driver_la_SOURCES = \
	cogl.c \
	cogl-texture-driver.c \
	cogl-context-driver.c \
	cogl-context-driver.h \
	cogl-gles2-wrapper.h \
	cogl-program.h \
	cogl-program.c \
	cogl-shader-private.h \
	cogl-shader.c \
	cogl-feature-functions.h

if USE_GLES2_WRAPPER
libclutter_cogl_driver_la_SOURCES += \
	cogl-gles2-wrapper.c \
	cogl-fixed-vertex-shader.h \
	cogl-fixed-vertex-shader.c \
	cogl-fixed-fragment-shader.h \
	cogl-fixed-fragment-shader.c
endif

EXTRA_DIST = \
	cogl-defines.h.in \
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

