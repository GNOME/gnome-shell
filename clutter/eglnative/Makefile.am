libclutterincludedir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter
libclutterinclude_HEADERS = clutter-egl.h

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterEGL\" \
        -DCLUTTER_COMPILATION \
	-I$(top_srcdir) \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-eglnative.la

libclutter_eglnative_la_SOURCES = \
	clutter-backend-egl.h \
	clutter-backend-egl.c \
	clutter-event-egl.c \
	clutter-stage-egl.h \
	clutter-stage-egl.c \
	clutter-egl-headers.h
