include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =

source_c = \
	cogl-gst-video-sink.c \
	$(NULL)

source_h = \
	cogl-gst.h \
	cogl-gst-video-sink.h \
	$(NULL)

lib_LTLIBRARIES = libcogl-gst.la

libcogl_gst_la_SOURCES = $(source_c) $(source_h)
libcogl_gst_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_GST_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libcogl_gst_la_LIBADD = $(top_builddir)/cogl/libcogl.la
libcogl_gst_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_GST_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl_gst_la_LDFLAGS = \
	-export-dynamic \
	-export-symbols-regex "^cogl_gst_.*" \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-rpath $(libdir)

AM_CPPFLAGS = \
	-DCOGL_COMPILATION		\
	-DG_LOG_DOMAIN=\"CoglGst\"	\
	-I$(top_srcdir)/cogl		\
	-I$(top_builddir)/cogl		\
	-I$(top_srcdir)/cogl/winsys	\
	-I$(top_srcdir)			\
	-I$(top_builddir)

cogl_gstheadersdir = $(includedir)/cogl/cogl-gst
cogl_gstheaders_HEADERS = $(source_h)

plugin_source_c = \
	$(srcdir)/cogl-gst-plugin.c \
	$(NULL)

libgstcogl_la_SOURCES =	\
	$(plugin_source_c)      \
	$(NULL)

plugin_LTLIBRARIES = libgstcogl.la

libgstcogl_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_GST_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libgstcogl_la_LIBADD = \
	$(top_builddir)/cogl/libcogl.la \
	$(builddir)/libcogl-gst.la
libgstcogl_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_GST_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libgstcogl_la_LDFLAGS =	\
	-avoid-version -no-undefined		\
	$(NULL)

pc_files = cogl-gst-1.0.pc \
	   cogl-gst-2.0-experimental.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cogl-gst.pc.in
DISTCLEANFILES += $(pc_files)

-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =

if HAVE_INTROSPECTION
INTROSPECTION_COMPILER_ARGS=--includedir=$(top_builddir)/cogl

CoglGst-2.0.gir: libcogl-gst.la Makefile

CoglGst_2_0_gir_NAMESPACE = CoglGst
CoglGst_2_0_gir_VERSION = 2.0
CoglGst_2_0_gir_LIBS = $(top_builddir)/cogl/libcogl.la libcogl-gst.la
CoglGst_2_0_gir_FILES = $(source_h) $(source_c)
CoglGst_2_0_gir_CFLAGS = $(AM_CPPFLAGS) $(COGL_GST_DEP_CFLAGS)
CoglGst_2_0_gir_INCLUDES = GObject-2.0 Gst-1.0 GstBase-1.0
CoglGst_2_0_gir_EXPORT_PACKAGES = cogl-gst-2.0-experimental
CoglGst_2_0_gir_SCANNERFLAGS = \
	--warn-all \
	--identifier-prefix=CoglGst \
	--symbol-prefix=cogl_gst \
	--c-include='cogl-gst/cogl-gst.h' \
	--c-include="gst/gst.h" \
	--include-uninstalled=$(top_builddir)/cogl/Cogl-2.0.gir \
	--pkg gstreamer-1.0 \
	--pkg cogl-2.0-experimental \
	--add-init-section="gst_init(NULL, NULL);"

INTROSPECTION_GIRS += CoglGst-2.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
