include $(top_srcdir)/build/autotools/Makefile.am.silent

source_c = \
	$(srcdir)/json-array.c 	\
	$(srcdir)/json-generator.c \
	$(srcdir)/json-marshal.c 	\
	$(srcdir)/json-node.c 	\
	$(srcdir)/json-object.c 	\
	$(srcdir)/json-parser.c

source_h = \
	$(top_srcdir)/clutter/json/json-generator.h 	\
	$(top_srcdir)/clutter/json/json-glib.h 		\
	$(top_srcdir)/clutter/json/json-marshal.h 	\
	$(top_srcdir)/clutter/json/json-parser.h 	\
	$(top_srcdir)/clutter/json/json-types.h

source_h_priv = \
	$(top_srcdir)/clutter/json/json-types-private.h

noinst_LTLIBRARIES = libclutter-json.la

INCLUDES = \
	-I$(top_srcdir) \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DG_LOG_DOMAIN=\"Clutter-Json\" \
	-DG_DISABLE_DEPRECATED \
	-DJSON_COMPILATION

AM_CFLAGS = $(MAINTAINER_CFLAGS)
AM_CPPFLAGS = $(CLUTTER_CFLAGS) $(CLUTTER_DEBUG_CFLAGS)

clutterjsondir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter/json
clutterjson_HEADERS = $(source_h)

libclutter_json_la_SOURCES = $(source_c) $(source_h) $(source_h_priv)

noinst_DATA =
CLEANFILES =

-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =

if HAVE_INTROSPECTION

BUILT_GIRSOURCES =

ClutterJson-@CLUTTER_API_VERSION@.gir: libclutter-json.la Makefile

ClutterJson_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = ClutterJson
ClutterJson_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
ClutterJson_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-json.la
ClutterJson_@CLUTTER_API_VERSION_AM@_gir_FILES = $(clutterjson_HEADERS) $(source_c)
ClutterJson_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(INCLUDES) $(AM_CPPFLAGS)
ClutterJson_@CLUTTER_API_VERSION_AM@_gir_INCLUDES = GObject-2.0
ClutterJson_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--identifier-prefix=Json \
	--symbol-prefix=json

INTROSPECTION_GIRS += ClutterJson-@CLUTTER_API_VERSION@.gir

girdir = $(datadir)/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

endif
