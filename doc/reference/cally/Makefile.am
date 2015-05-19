DOC_MODULE = cally
DOC_MAIN_SGML_FILE =$(DOC_MODULE)-docs.xml
DOC_SOURCE_DIR = $(top_srcdir)/clutter/cally $(top_builddir)/clutter/cally

SCANGOBJ_OPTIONS = --type-init-func="clutter_base_init()"
SCAN_OPTIONS =
MKDB_OPTIONS = --output-format=xml --name-space=cally
MKTMPL_OPTIONS =
FIXXREF_OPTIONS = \
	--extra-dir=../clutter/html \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo \
	--extra-dir=$(PANGO_PREFIX)/share/gtk-doc/html/pango \
	--extra-dir=$(COGL_PREFIX)/share/gtk-doc/html/cogl

HFILE_GLOB = $(top_srcdir)/clutter/cally/*.h
CFILE_GLOB = $(top_srcdir)/clutter/cally/*.c

IGNORE_HFILES = \
	cally-actor-private.h \
	cally.h

EXTRA_HFILES =

HTML_IMAGES=

content_files= \
	cally-overview.xml

expand_content_files= \
	cally-overview.xml

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir) \
	-I$(top_builddir)/clutter

AM_CFLAGS = $(CLUTTER_CFLAGS)

GTKDOC_LIBS = $(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la $(CLUTTER_LIBS)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
#EXTRA_DIST +=
