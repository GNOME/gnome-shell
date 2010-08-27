libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter/egl
libclutterinclude_HEADERS = clutter-egl.h clutter-egl-headers.h

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterEGL\" \
        -DCLUTTER_COMPILATION \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(MAINTAINER_CFLAGS)
if SUPPORT_X11
INCLUDES += -I$(top_srcdir)/clutter/x11
endif

LDADD = $(CLUTTER_LIBS)
if SUPPORT_X11
LDADD += $(top_builddir)/clutter/x11/libclutter-x11.la
endif

noinst_LTLIBRARIES = libclutter-egl.la

if SUPPORT_X11
libclutter_egl_la_DEPENDENCIES = \
	$(top_builddir)/clutter/x11/libclutter-x11.la
endif

libclutter_egl_la_SOURCES = \
	clutter-backend-egl.h \
	clutter-backend-egl.c \
	clutter-stage-egl.h \
	clutter-stage-egl.c \
	clutter-egl.h \
	clutter-egl-headers.h

if USE_TSLIB
libclutter_egl_la_SOURCES += clutter-event-tslib.c
endif

if SUPPORT_CEX100
libclutter_egl_la_SOURCES += clutter-backend-cex100.c clutter-backend-cex100.h
endif
