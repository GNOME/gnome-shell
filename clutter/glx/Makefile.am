libclutterincludedir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterGLX\" \
	-I$(top_srcdir) \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-glx.la

libclutter_glx_la_SOURCES = \
	clutter-backend-glx.h \
	clutter-backend-glx.c \
	clutter-event-glx.c \
	clutter-stage-glx.h \
	clutter-stage-glx.c \
	clutter-glx.h
