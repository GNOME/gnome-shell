DOC_MODULE = clutter
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.xml
DOC_SOURCE_DIR = $(top_srcdir)/clutter $(top_builddir)/clutter

SCANGOBJ_OPTIONS =
SCAN_OPTIONS =
MKDB_OPTIONS = --xml-mode --output-format=xml --name-space=clutter
MKTMPL_OPTIONS =
FIXXREF_OPTIONS = \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo \
	--extra-dir=$(PANGO_PREFIX)/share/gtk-doc/html/pango \
	--extra-dir=$(COGL_PREFIX)/share/gtk-doc/html/cogl \
	--extra-dir=$(GDK_PREFIX)/share/gtk-doc/html/gdk \
	--extra-dir=$(ATK_PREFIX)/share/gtk-doc/html/atk

BUILT_HFILES = \
	clutter-enum-types.h \
	clutter-version.h \
	clutter-marshal.h

HFILE_GLOB = \
	$(top_srcdir)/clutter/*.h \
	$(top_builddir)/clutter/*.h \
	$(top_srcdir)/clutter/deprecated/*.h \
	$(top_srcdir)/clutter/x11/clutter-x11.h \
	$(top_srcdir)/clutter/x11/clutter-x11-texture-pixmap.h \
	$(top_srcdir)/clutter/x11/clutter-glx-texture-pixmap.h \
	$(top_srcdir)/clutter/egl/clutter-egl.h \
	$(top_builddir)/clutter/cex100/clutter-cex100.h \
	$(top_srcdir)/clutter/win32/clutter-win32.h \
	$(top_srcdir)/clutter/gdk/clutter-gdk.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland-compositor.h \
	$(top_srcdir)/clutter/wayland/clutter-wayland-surface.h \
	$(top_srcdir)/clutter/mir/clutter-mir.h \
	$(top_srcdir)/clutter/cally/*.h

CFILE_GLOB = \
	$(top_srcdir)/clutter/*.c \
	$(top_srcdir)/clutter/cally/*.c \
	$(top_srcdir)/clutter/cogl/*.c \
	$(top_srcdir)/clutter/x11/*.c \
	$(top_srcdir)/clutter/win32/*.c \
	$(top_srcdir)/clutter/gdk/*.c \
	$(top_srcdir)/clutter/cex100/*.c \
	$(top_srcdir)/clutter/egl/*.c \
	$(top_srcdir)/clutter/wayland/*.c \
	$(top_srcdir)/clutter/mir/*.c \
	$(top_srcdir)/clutter/deprecated/*.c

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
	clutter-constraint-private.h	\
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
	clutter-master-clock-default.h	\
	clutter-master-clock.h 		\
	clutter-model-private.h 	\
	clutter-paint-node-private.h	\
	clutter-paint-volume-private.h	\
	clutter-private.h 		\
	clutter-script-private.h 	\
	clutter-settings-private.h	\
	clutter-stage-manager-private.h	\
	clutter-stage-private.h		\
	clutter-stage-window.h 		\
	clutter-timeout-interval.h 	\
	cally-actor-private.h		\
	cex100				\
	cogl 				\
	egl				\
	evdev				\
	gdk				\
	mir				\
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
	$(top_srcdir)/clutter/wayland/clutter-wayland-surface.h \
	$(top_srcdir)/clutter/mir/clutter-mir.h

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

SVG_IMAGES = \
	animator-key-frames.svg \
        easing-modes.svg

content_files = \
	glossary.xml \
	clutter-overview.xml \
	building-clutter.xml \
	running-clutter.xml \
	migrating-ClutterAnimation.xml \
	migrating-ClutterBehaviour.xml \
	migrating-ClutterEffect.xml \
	migrating-ClutterPath.xml

expand_content_files = \
	glossary.xml \
	clutter-overview.xml \
	building-clutter.xml \
	running-clutter.xml \
	migrating-ClutterAnimation.xml \
	migrating-ClutterBehaviour.xml \
	migrating-ClutterEffect.xml \
	migrating-ClutterPath.xml

GTKDOC_CFLAGS = -I$(top_srcdir) -I$(top_builddir) -DCLUTTER_DISABLE_DEPRECATION_WARNINGS $(CLUTTER_CFLAGS)
GTKDOC_LIBS = $(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la $(CLUTTER_LIBS)

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST += clutter.types
EXTRA_DIST += $(HTML_IMAGES) $(SVG_IMAGES)

if ENABLE_GTK_DOC
TESTS_ENVIRONMENT = \
	DOC_MODULE=$(DOC_MODULE) \
	DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE) \
	SRCDIR=$(abs_srcdir) \
	BUILDDIR=$(abs_builddir)

TESTS = $(GTKDOC_CHECK)
endif
