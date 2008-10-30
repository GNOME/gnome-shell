libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter
libclutterinclude_HEADERS = clutter-eglx.h

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterEGL\" \
        -DCLUTTER_COMPILATION \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/x11 \
	-I$(top_builddir)/clutter \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS) $(top_builddir)/clutter/x11/libclutter-x11.la

noinst_LTLIBRARIES = libclutter-eglx.la

libclutter_eglx_la_DEPENDENCIES = \
	$(top_builddir)/clutter/x11/libclutter-x11.la

libclutter_eglx_la_SOURCES = \
	clutter-backend-egl.h \
	clutter-backend-egl.c \
	clutter-stage-egl.h \
	clutter-stage-egl.c \
	clutter-eglx.h \
	clutter-egl-headers.h
