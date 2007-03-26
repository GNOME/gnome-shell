libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter
libclutterinclude_HEADERS = $(top_srcdir)/clutter/cogl/cogl.h

INCLUDES = \
	-I$(top_srcdir)              \
	-I$(top_srcdir)/clutter/cogl \
	$(CLUTTER_CFLAGS)            \
	$(CLUTTER_DEBUG_CFLAGS)      \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-cogl.la

libclutter_cogl_la_SOURCES =              \
	$(top_srcdir)/clutter/cogl/cogl.h         \
	cogl.c
