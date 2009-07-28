include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

INCLUDES = \
	-I$(top_srcdir) \
	-I$(srcdir) \
	-I$(srcdir)/.. \
	-I$(srcdir)/../driver/$(COGL_DRIVER) \
	-I$(srcdir)/../.. \
	-I.. \
	-I../.. \
	-DG_DISABLE_SINGLE_INCLUDES \
        -DG_LOG_DOMAIN=\"Cogl-Winsys\" \
        -DCLUTTER_COMPILATION

noinst_LTLIBRARIES = libclutter-cogl-winsys.la

# Automake can't determine the full list if we are using autoconf substitutions
# to specify the files required for libclutter-cogl-winsys
all_winsys_sources = 	\
	cogl-glx.c \
	cogl-eglx.c \
	cogl-eglnative.c \
	cogl-sdl.c \
	cogl-win32.c \
	cogl-osx.c \
	cogl-winsys.h
	$(NULL)

libclutter_cogl_winsys_la_CPPFLAGS = \
	$(CLUTTER_CFLAGS) \
	$(COGL_DEBUG_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(MAINTAINER_CFLAGS)
libclutter_cogl_winsys_la_LIBADD = -lm $(CLUTTER_LIBS)
libclutter_cogl_winsys_la_SOURCES = 	\
	cogl-@COGL_WINSYS@.h		\
	cogl-@COGL_WINSYS@.c		\
	$(NULL)

EXTRA_DIST=$(all_winsys_sources)

