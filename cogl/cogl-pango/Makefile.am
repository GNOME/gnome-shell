NULL =

CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =

source_c = \
	cogl-pango-display-list.c   \
	cogl-pango-fontmap.c        \
	cogl-pango-render.c         \
	cogl-pango-glyph-cache.c    \
	cogl-pango-pipeline-cache.c \
	$(NULL)

source_h = cogl-pango.h

source_h_priv = \
	cogl-pango-display-list.h   \
	cogl-pango-private.h        \
	cogl-pango-glyph-cache.h    \
	cogl-pango-pipeline-cache.h \
	$(NULL)

mutterlibdir = $(libdir)/mutter
mutterlib_LTLIBRARIES = libmutter-cogl-pango.la

libmutter_cogl_pango_la_SOURCES = $(source_c) $(source_h) $(source_h_priv)
libmutter_cogl_pango_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_PANGO_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libmutter_cogl_pango_la_LIBADD = $(top_builddir)/cogl/libmutter-cogl.la
libmutter_cogl_pango_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_PANGO_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libmutter_cogl_pango_la_LDFLAGS = \
	-export-dynamic \
	-rpath $(mutterlibdir) \
	-export-symbols-regex "^cogl_pango_.*" \
	-no-undefined \
	-avoid-version

AM_CPPFLAGS = \
	-DCOGL_COMPILATION		\
	-DG_LOG_DOMAIN=\"CoglPango\"	\
	-I$(top_srcdir)/cogl		\
	-I$(top_builddir)/cogl		\
	-I$(top_srcdir)/cogl/winsys	\
	-I$(top_srcdir)			\
	-I$(top_builddir)

cogl_base_includedir = $(includedir)/mutter
cogl_pangoheadersdir = $(cogl_base_includedir)/cogl/cogl-pango
cogl_pangoheaders_HEADERS = $(source_h)

pc_files = mutter-cogl-pango-1.0.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

DISTCLEANFILES += $(pc_files)

EXTRA_DIST += cogl-pango.symbols

-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =

if HAVE_INTROSPECTION
INTROSPECTION_COMPILER_ARGS=--includedir=$(top_builddir)/cogl

CoglPango-1.0.gir: libmutter-cogl-pango.la Makefile

CoglPango_1_0_gir_NAMESPACE = CoglPango
CoglPango_1_0_gir_VERSION = 1.0
CoglPango_1_0_gir_LIBS = $(top_builddir)/cogl/libmutter-cogl.la libmutter-cogl-pango.la
CoglPango_1_0_gir_FILES = $(source_h) $(source_c)
CoglPango_1_0_gir_CFLAGS = $(AM_CPPFLAGS) $(COGL_DEP_CFLAGS) $(COGL_PANGO_DEP_CFLAGS)
CoglPango_1_0_gir_INCLUDES = Pango-1.0 PangoCairo-1.0
CoglPango_1_0_gir_EXPORT_PACKAGES = mutter-cogl-pango-1.0
CoglPango_1_0_gir_SCANNERFLAGS = \
	--warn-all \
	--identifier-prefix=CoglPango \
	--symbol-prefix=cogl_pango \
	--c-include='cogl-pango/cogl-pango.h' \
	--include-uninstalled=$(top_builddir)/cogl/Cogl-1.0.gir

INTROSPECTION_GIRS += CoglPango-1.0.gir

girdir = $(mutterlibdir)
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(mutterlibdir)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
