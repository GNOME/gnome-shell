source_c = \
	json-array.c \
	json-generator.c \
	json-marshal.c \
	json-node.c \
	json-object.c \
	json-parser.c

source_h = \
	json-generator.h \
	json-glib.h \
	json-marshal.h \
	json-parser.h \
	json-types.h

noinst_LTLIBRARIES = libclutter-json.la

libclutter_json_la_SOURCES = $(source_c) $(source_h)

INCLUDES = \
	-I$(top_srcdir) \
	-DG_DISABLE_DEPRECATED \
	$(GCC_FLAGS) \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS)

LDADD = -export-symbols-regex "^json.*"

clutterjsondir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter/json
clutterjson_HEADERS = $(source_h)
