libclutterincludedir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterEGL\" \
	-I$(top_srcdir) \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-egl.la

libclutter_egl_la_SOURCES = \
	clutter-backend-egl.h \
	clutter-backend-egl.c \
	clutter-event-egl.c \
	clutter-stage-egl.h \
	clutter-stage-egl.c
