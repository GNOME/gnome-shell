libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter
libclutterinclude_HEADERS = clutter-egl.h

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterEGL\"	\
        -DCLUTTER_COMPILATION 		\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/clutter 	\
	-I$(top_srcdir)/clutter/cogl 	\
	-I$(top_builddir)/clutter 	\
	-I$(top_builddir)/clutter/cogl

common_ldadd = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-eglnative.la

libclutter_eglnative_la_SOURCES = \
	clutter-backend-egl.h \
	clutter-backend-egl.c \
	clutter-event-egl.c \
	clutter-stage-egl.h \
	clutter-stage-egl.c \
	clutter-egl-headers.h
libclutter_eglnative_la_CPPFLAGS = $(CLUTTER_CFLAGS) $(CLUTTER_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS)
libclutter_eglnative_la_LIBADD = $(common_ldadd)
