libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter
libclutterinclude_HEADERS = clutter-win32.h

clutter-win32-$(CLUTTER_API_VERSION).pc: clutter-win32.pc
	@cp -f $< $(@F)

pkgconfig_DATA = clutter-win32-@CLUTTER_API_VERSION@.pc
pkgconfigdir   = $(libdir)/pkgconfig

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterWin32\" \
        -DCLUTTER_COMPILATION \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(MAINTAINER_CFLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-win32.la

libclutter_win32_la_SOURCES = \
	clutter-backend-win32.h \
	clutter-backend-win32.c \
	clutter-device-manager-win32.h \
	clutter-device-manager-win32.c \
	clutter-event-win32.c \
	clutter-stage-win32.h \
	clutter-stage-win32.c \
	clutter-win32.h

CLEANFILES = clutter-win32-$(CLUTTER_API_VERSION).pc

EXTRA_DIST = clutter-win32.pc.in resources.rc invisible-cursor.cur
