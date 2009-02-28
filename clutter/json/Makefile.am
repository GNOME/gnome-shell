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

noinst_LTLIBRARIES = libclutter-json.la

libclutter_json_la_SOURCES = $(source_c) $(source_h)

INCLUDES = \
	-I$(top_srcdir) \
	-DG_DISABLE_DEPRECATED \
	$(GCC_FLAGS) \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS)

clutterjsondir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter/json
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
		--libtool="$(top_builddir)/doltlibtool" \
		--pkg gobject-2.0 \
		--output $@ \
		$(clutterjson_HEADERS) \
		$(source_c)

BUILT_GIRSOURCES += ClutterJson-@CLUTTER_API_VERSION@.gir

noinst_DATA += $(BUILT_GIRSOURCES)

CLEANFILES += $(BUILT_GIRSOURCES)
endif
