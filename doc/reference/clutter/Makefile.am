DOC_MODULE = clutter
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.xml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR = ../../../clutter

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS = --type-init-func="clutter_base_init()"

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED"
SCAN_OPTIONS = --deprecated-guards="CLUTTER_DISABLE_DEPRECATED"

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS = --output-format=xml --name-space=clutter

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS =

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS = \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo \
	--extra-dir=$(PANGO_PREFIX)/share/gtk-doc/html/pango \
	--extra-dir=$(COGL_PREFIX)/share/gtk-doc/html/cogl

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB = \
	$(top_srcdir)/clutter/*.h \
	$(top_builddir)/clutter/*.h \
	$(top_srcdir)/clutter/x11/clutter-x11.h \
	$(top_srcdir)/clutter/x11/clutter-x11-texture-pixmap.h \
	$(top_srcdir)/clutter/x11/clutter-glx-texture-pixmap.h \
	$(top_srcdir)/clutter/egl/clutter-egl.h \
	$(top_srcdir)/clutter/cex100/clutter-cex100.h \
	$(top_srcdir)/clutter/win32/clutter-win32.h \
	$(top_srcdir)/clutter/gdk/clutter-gdk.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland-compositor.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland-surface.h

CFILE_GLOB = \
	$(top_srcdir)/clutter/*.c \
	$(top_srcdir)/clutter/cogl/*.c \
	$(top_srcdir)/clutter/x11/*.c \
	$(top_srcdir)/clutter/win32/*.c \
	$(top_srcdir)/clutter/gdk/*.c \
	$(top_srcdir)/clutter/cex100/*.c \
	$(top_srcdir)/clutter/egl/*.c \
	$(top_srcdir)/clutter/wayland/*.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES = \
	config.h			\
	clutter.h 			\
	clutter-actor-meta-private.h	\
	clutter-actor-private.h		\
	clutter-backend-private.h	\
        clutter-bezier.h 		\
	clutter-cogl-compat.h		\
	clutter-color-static.h		\
	clutter-config.h		\
	clutter-debug.h 		\
	clutter-deprecated.h 		\
	clutter-device-manager-private.h	\
	clutter-easing.h		\
	clutter-enum-types.h 		\
	clutter-event-translator.h	\
	clutter-flatten-effect.h	\
	clutter-gesture-action-private.h	\
	clutter-id-pool.h 		\
	clutter-keysyms.h 		\
	clutter-keysyms-compat.h	\
	clutter-keysyms-table.h 	\
	clutter-marshal.h 		\
	clutter-master-clock.h 		\
	clutter-model-private.h 	\
	clutter-paint-node-private.h	\
	clutter-paint-volume-private.h	\
	clutter-private.h 		\
	clutter-profile.h		\
	clutter-script-private.h 	\
	clutter-stage-manager-private.h	\
	clutter-stage-private.h		\
	clutter-stage-window.h 		\
	clutter-timeout-interval.h 	\
	clutter-test-utils.h		\
	cally				\
	cex100				\
	cogl 				\
	egl				\
	evdev				\
	gdk				\
	osx 				\
	tslib				\
	x11 				\
	wayland				\
	win32

EXTRA_HFILES = \
        $(top_srcdir)/clutter/x11/clutter-x11.h \
	$(top_srcdir)/clutter/x11/clutter-x11-texture-pixmap.h \
	$(top_srcdir)/clutter/x11/clutter-glx-texture-pixmap.h \
	$(top_srcdir)/clutter/egl/clutter-egl.h \
	$(top_srcdir)/clutter/cex100/clutter-cex100.h \
	$(top_srcdir)/clutter/win32/clutter-win32.h \
	$(top_srcdir)/clutter/gdk/clutter-gdk.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland-compositor.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland-surface.h

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES = \
	actor-box.png \
	actor-example.png \
	animator-key-frames.png \
	bin-layout.png \
	box-layout.png \
	constraints-example.png \
        easing-modes.png \
	event-flow.png \
	flow-layout.png \
	path-alpha-func.png \
	offscreen-redirect.png \
	table-layout.png

# SVG used to generate the images above
SVG_IMAGES = \
	animator-key-frames.svg \
        easing-modes.svg

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files = \
	glossary.xml \
	clutter-overview.xml \
	building-clutter.xml \
	running-clutter.xml \
	migrating-ClutterAnimation.xml \
	migrating-ClutterBehaviour.xml \
	migrating-ClutterEffect.xml \
	migrating-ClutterPath.xml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files = \
	glossary.xml \
	clutter-overview.xml \
	building-clutter.xml \
	running-clutter.xml \
	migrating-ClutterAnimation.xml \
	migrating-ClutterBehaviour.xml \
	migrating-ClutterEffect.xml \
	migrating-ClutterPath.xml

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir) \
	-I$(top_builddir)/clutter \
	-DCLUTTER_DISABLE_DEPRECATION_WARNINGS
AM_CFLAGS = $(CLUTTER_CFLAGS)
GTKDOC_LIBS = $(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la $(CLUTTER_LIBS)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += $(HTML_IMAGES) $(SVG_IMAGES)
