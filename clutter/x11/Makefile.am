include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterX11\"           \
        -DCLUTTER_COMPILATION                   \
	-I$(top_srcdir)                         \
	-I$(top_srcdir)/clutter 	        \
	-I$(top_srcdir)/clutter/cogl 	        \
	-I$(top_builddir)/clutter 	        \
	-I$(top_builddir)/clutter/cogl 	        \
	$(CLUTTER_CFLAGS)                       \
	$(CLUTTER_DEBUG_CFLAGS)                 \
	$(MAINTAINER_CFLAGS)

LDADD = $(CLUTTER_LIBS)

source_h = \
	$(top_srcdir)/clutter/x11/clutter-x11-texture-pixmap.h \
	$(top_srcdir)/clutter/x11/clutter-x11.h

BUILT_SOURCES =

EXTRA_DIST =

CLEANFILES =

DISTCLEANFILES =

# glib-mkenums rules
glib_enum_h = clutter-x11-enum-types.h
glib_enum_c = clutter-x11-enum-types.c
glib_enum_headers = $(source_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

noinst_LTLIBRARIES = libclutter-x11.la

libclutter_x11_la_SOURCES = \
	$(srcdir)/clutter-backend-x11.h 	\
	$(srcdir)/clutter-backend-x11.c 	\
	$(srcdir)/clutter-device-manager-x11.h 	\
	$(srcdir)/clutter-device-manager-x11.c 	\
	$(srcdir)/clutter-event-x11.c 		\
	$(srcdir)/clutter-input-device-x11.h 	\
	$(srcdir)/clutter-input-device-x11.c 	\
	$(srcdir)/clutter-stage-x11.h 		\
	$(srcdir)/clutter-stage-x11.c 		\
        $(srcdir)/clutter-x11-texture-pixmap.c 	\
	clutter-x11-enum-types.h 		\
	clutter-x11-enum-types.c 		\
	$(NULL)

libclutter_x11_includedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter/x11
libclutter_x11_include_HEADERS = $(source_h) clutter-x11-enum-types.h

clutter-x11-$(CLUTTER_API_VERSION).pc: clutter-x11.pc
	@cp -f $< $(@F)

pkgconfig_DATA = clutter-x11-@CLUTTER_API_VERSION@.pc
pkgconfigdir   = $(libdir)/pkgconfig

CLEANFILES += clutter-x11-$(CLUTTER_API_VERSION).pc

EXTRA_DIST += clutter-x11.pc.in
