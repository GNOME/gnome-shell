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
	cogl-primitives.c \
	cogl-texture-driver.c \
	cogl-shader-private.h \
	cogl-shader.c \
	cogl-program.h \
	cogl-program.c \
	cogl-context-driver.h \
	cogl-context-driver.c \
	$(NULL)

EXTRA_DIST = cogl-defines.h.in

