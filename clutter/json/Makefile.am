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

libclutter_json_la_SOURCES = $(source_c) $(source_h) $(source_h_priv)
libclutter_json_la_CPPFLAGS = $(CLUTTER_CFLAGS) $(CLUTTER_DEBUG_CFLAGS)

INCLUDES = \
	-I$(top_srcdir) \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DG_LOG_DOMAIN=\"Clutter-Json\" \
	-DG_DISABLE_DEPRECATED

AM_CFLAGS = $(MAINTAINER_CFLAGS)

clutterjsondir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter/json
clutterjson_HEADERS = $(source_h)

noinst_DATA =
CLEANFILES =

if HAVE_INTROSPECTION
BUILT_GIRSOURCES =

ClutterJson-@CLUTTER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) libclutter-json.la
	$(INTROSPECTION_SCANNER) -v --namespace ClutterJson --nsversion=@CLUTTER_API_VERSION@ \
		--add-include-path=$(srcdir) --add-include-path=. \
		--include=GObject-2.0 \
		--library=clutter-json \
		--libtool="$(top_builddir)/libtool" \
		--pkg gobject-2.0 \
		--output $@ \
		$(clutterjson_HEADERS) \
		$(source_c)

BUILT_GIRSOURCES += ClutterJson-@CLUTTER_API_VERSION@.gir

noinst_DATA += $(BUILT_GIRSOURCES)

CLEANFILES += $(BUILT_GIRSOURCES)
endif
