libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter
libclutterinclude_HEADERS = clutter-x11.h \
                            clutter-x11-texture-pixmap.h

clutter-x11-$(CLUTTER_API_VERSION).pc: clutter-x11.pc
	@cp -f $< $(@F)

pkgconfig_DATA = clutter-x11-@CLUTTER_API_VERSION@.pc
pkgconfigdir   = $(libdir)/pkgconfig

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterX11\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter/cogl 	        \
	-I$(top_srcdir)/clutter/cogl/@CLUTTER_COGL@ \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-x11.la

libclutter_x11_la_SOURCES = \
	clutter-backend-x11.h \
	clutter-backend-x11.c \
	clutter-event-x11.c \
	clutter-stage-x11.h \
	clutter-stage-x11.c \
        clutter-x11-texture-pixmap.h \
        clutter-x11-texture-pixmap.c \
	clutter-x11.h

CLEANFILES = clutter-x11-$(CLUTTER_API_VERSION).pc

EXTRA_DIST = clutter-x11.pc.in
