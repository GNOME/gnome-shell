libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter
libclutterinclude_HEADERS = clutter-glx.h clutter-glx-texture-pixmap.h

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterGLX\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter/cogl 	        \
	-I$(top_srcdir)/clutter/cogl/@CLUTTER_COGL@ \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-glx.la

libclutter_glx_la_SOURCES = \
	clutter-backend-glx.h \
	clutter-backend-glx.c \
	clutter-stage-glx.h \
	clutter-stage-glx.c \
        clutter-glx-texture-pixmap.h \
        clutter-glx-texture-pixmap.c \
	clutter-glx.h
