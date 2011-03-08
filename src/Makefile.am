# Flag build for parallelism; see https://savannah.gnu.org/patch/?6905
.AUTOPARALLEL:

lib_LTLIBRARIES = libmutter-private.la libmutter-wm.la

SUBDIRS=wm-tester tools compositor/plugins

INCLUDES=								\
	$(MUTTER_CFLAGS)						\
	-I$(srcdir)							\
	-I$(srcdir)/core						\
	-I$(srcdir)/ui							\
	-I$(srcdir)/compositor						\
	-DMUTTER_LIBEXECDIR=\"$(libexecdir)\"				\
	-DHOST_ALIAS=\"@HOST_ALIAS@\"					\
	-DMUTTER_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\"		\
	-DMUTTER_PKGDATADIR=\"$(pkgdatadir)\"				\
	-DMUTTER_DATADIR=\"$(datadir)\"					\
	-DG_LOG_DOMAIN=\"mutter\"					\
	-DSN_API_NOT_YET_FROZEN=1					\
	-DMUTTER_MAJOR_VERSION=$(MUTTER_MAJOR_VERSION)			\
	-DMUTTER_MINOR_VERSION=$(MUTTER_MINOR_VERSION)			\
	-DMUTTER_MICRO_VERSION=$(MUTTER_MICRO_VERSION)			\
	-DMUTTER_PLUGIN_API_VERSION=$(MUTTER_PLUGIN_API_VERSION)	\
	-DMUTTER_PKGLIBDIR=\"$(pkglibdir)\"				\
	-DMUTTER_PLUGIN_DIR=\"@MUTTER_PLUGIN_DIR@\"			\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\"

mutter_built_sources = \
	mutter-marshal.h \
	mutter-marshal.c \
	mutter-enum-types.h \
	mutter-enum-types.c

libmutter_wm_la_SOURCES =			\
	core/async-getprop.c			\
	core/async-getprop.h			\
	core/bell.c				\
	core/bell.h				\
	core/boxes.c				\
	core/boxes-private.h			\
	meta/boxes.h				\
	compositor/cogl-utils.c			\
	compositor/cogl-utils.h			\
	compositor/compositor.c			\
	compositor/compositor-private.h		\
	compositor/meta-background-actor.c	\
	compositor/meta-background-actor.h	\
	compositor/meta-module.c		\
	compositor/meta-module.h		\
	compositor/meta-plugin.c		\
	compositor/meta-plugin-manager.c	\
	compositor/meta-plugin-manager.h	\
	compositor/meta-shadow-factory.c	\
	compositor/meta-shadow-factory-private.h	\
	compositor/meta-shaped-texture.c	\
	compositor/meta-shaped-texture.h	\
	compositor/meta-texture-tower.c		\
	compositor/meta-texture-tower.h		\
	compositor/meta-window-actor.c		\
	compositor/meta-window-actor-private.h	\
	compositor/meta-window-group.c		\
	compositor/meta-window-group.h		\
	compositor/meta-window-shape.c		\
	compositor/meta-window-shape.h		\
	compositor/region-utils.c		\
	compositor/region-utils.h		\
	meta/compositor.h			\
	meta/meta-plugin.h			\
	meta/meta-shadow-factory.h		\
	meta/meta-window-actor.h		\
	meta/compositor-mutter.h 		\
	core/above-tab-keycode.c		\
	core/constraints.c			\
	core/constraints.h			\
	core/core.c				\
	core/delete.c				\
	core/display.c				\
	core/display-private.h			\
	meta/display.h				\
	ui/draw-workspace.c			\
	ui/draw-workspace.h			\
	core/edge-resistance.c			\
	core/edge-resistance.h			\
	core/errors.c				\
	meta/errors.h				\
	core/eventqueue.c			\
	core/eventqueue.h			\
	core/frame.c				\
	core/frame.h				\
	ui/gradient.c				\
	meta/gradient.h				\
	core/group-private.h			\
	core/group-props.c			\
	core/group-props.h			\
	core/group.c				\
	meta/group.h				\
	core/iconcache.c			\
	core/iconcache.h			\
	core/keybindings.c			\
	core/keybindings-private.h		\
	core/main.c				\
	core/mutter-Xatomtype.h			\
	core/place.c				\
	core/place.h				\
	core/prefs.c				\
	meta/prefs.h				\
	core/screen.c				\
	core/screen-private.h			\
	meta/screen.h				\
	meta/types.h				\
	core/session.c				\
	core/session.h				\
	core/stack.c				\
	core/stack.h				\
	core/stack-tracker.c			\
	core/stack-tracker.h			\
	core/util.c				\
	meta/util.h				\
	core/window-props.c			\
	core/window-props.h			\
	core/window.c				\
	core/window-private.h			\
	meta/window.h				\
	core/workspace.c			\
	core/workspace-private.h		\
	core/xprops.c				\
	core/xprops.h				\
	meta/common.h				\
	core/core.h				\
	ui/ui.h					\
	inlinepixbufs.h				\
	ui/fixedtip.c				\
	ui/fixedtip.h				\
	ui/frames.c				\
	ui/frames.h				\
	ui/menu.c				\
	ui/menu.h				\
	ui/metaaccellabel.c			\
	ui/metaaccellabel.h			\
	ui/resizepopup.c			\
	ui/resizepopup.h			\
	ui/tabpopup.c				\
	ui/tabpopup.h				\
	ui/tile-preview.c			\
	ui/tile-preview.h			\
	ui/theme-parser.c			\
	ui/theme.c				\
	meta/theme.h				\
	ui/theme-private.h			\
	ui/ui.c					\
	core/all-keybindings.h			\
	$(mutter_built_sources)

libmutter_wm_la_LDFLAGS = -no-undefined
libmutter_wm_la_LIBADD  = $(MUTTER_LIBS)

# by setting libmutter_private_la_CFLAGS, the files shared with
# mutter proper will be compiled with different names.
libmutter_private_la_CFLAGS =
libmutter_private_la_SOURCES=			\
	core/boxes.c				\
	meta/boxes.h				\
	ui/gradient.c				\
	meta/gradient.h				\
	core/util.c				\
	meta/util.h				\
	meta/common.h				\
	ui/preview-widget.c			\
	meta/preview-widget.h			\
	ui/theme-parser.c			\
	ui/theme.c				\
	meta/theme.h

libmutter_private_la_LDFLAGS = -no-undefined
libmutter_private_la_LIBADD  = $(MUTTER_LIBS)

# Headers installed for plugins; introspected information will
# be extracted into Mutter-<version>.gir
libmutterinclude_base_headers =		\
	meta/boxes.h				\
	meta/common.h				\
	meta/compositor-mutter.h		\
	meta/compositor.h			\
	meta/display.h				\
	meta/errors.h				\
	meta/gradient.h				\
	meta/group.h				\
	meta/keybindings.h			\
	meta/main.h				\
	meta/meta-plugin.h			\
	meta/meta-shadow-factory.h		\
	meta/meta-window-actor.h		\
	meta/prefs.h				\
	meta/screen.h				\
	meta/theme.h				\
	meta/types.h				\
	meta/util.h				\
	meta/window.h				\
	meta/workspace.h

# Excluded from scanning for introspection but installed
# preview-widget.h: only part of libmutter-private
# atomnames.h: macros cause problems for scanning process
libmutterinclude_extra_headers =		\
	meta/preview-widget.h			\
	meta/atomnames.h

libmutterincludedir = $(includedir)/mutter/meta

libmutterinclude_HEADERS =			\
	$(libmutterinclude_base_headers)	\
	$(libmutterinclude_extra_headers)

mutter_theme_viewer_SOURCES=  \
	ui/theme-viewer.c

bin_PROGRAMS=mutter mutter-theme-viewer

mutter_SOURCES = core/mutter.c
mutter_LDADD = $(MUTTER_LIBS) libmutter-wm.la

if HAVE_INTROSPECTION
include $(INTROSPECTION_MAKEFILE)

api_version = $(MUTTER_MAJOR_VERSION).$(MUTTER_MINOR_VERSION)

# These files are in package-private directories, even though they may be used
# by plugins.  If you're writing a plugin, use g-ir-compiler --add-include-path
# and g-ir-compiler --includedir.
girdir = $(pkglibdir)
gir_DATA = Meta-$(api_version).gir

typelibdir = $(pkglibdir)
typelib_DATA = Meta-$(api_version).typelib

INTROSPECTION_GIRS = Meta-$(api_version).gir

Meta-$(api_version).gir: libmutter-wm.la
@META_GIR@_INCLUDES = GObject-2.0 Gdk-3.0 Gtk-3.0 Clutter-1.0 xlib-2.0 xfixes-4.0
@META_GIR@_PACKAGES = clutter-1.0 gtk+-3.0
@META_GIR@_CFLAGS = $(INCLUDES)
@META_GIR@_LIBS = libmutter-wm.la
@META_GIR@_FILES =				\
	mutter-enum-types.h			\
	$(libmutterinclude_base_headers)	\
	$(filter %.c,$(libmutter_wm_la_SOURCES))
@META_GIR@_SCANNERFLAGS = --warn-all --warn-error

endif

mutter_theme_viewer_LDADD= $(MUTTER_LIBS) libmutter-private.la

testboxes_SOURCES = core/testboxes.c
testgradient_SOURCES = ui/testgradient.c
testasyncgetprop_SOURCES = core/testasyncgetprop.c

noinst_PROGRAMS=testboxes testgradient testasyncgetprop

testboxes_LDADD = $(MUTTER_LIBS) libmutter-wm.la
testgradient_LDADD = $(MUTTER_LIBS) libmutter-wm.la
testasyncgetprop_LDADD = $(MUTTER_LIBS) libmutter-wm.la

@INTLTOOL_DESKTOP_RULE@

desktopfilesdir=$(datadir)/applications
desktopfiles_in_files=mutter.desktop.in
desktopfiles_files=$(desktopfiles_in_files:.desktop.in=.desktop)
desktopfiles_DATA = $(desktopfiles_files)

wmpropertiesdir=$(datadir)/gnome/wm-properties
wmproperties_in_files=mutter-wm.desktop.in
wmproperties_files=$(wmproperties_in_files:.desktop.in=.desktop)
wmproperties_DATA = $(wmproperties_files)

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = mutter.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_XML_NOMERGE_RULE@

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA)
else
install-data-local:
endif

IMAGES=stock_maximize.png stock_minimize.png stock_delete.png
VARIABLES=stock_maximize_data $(srcdir)/stock_maximize.png \
          stock_minimize_data $(srcdir)/stock_minimize.png \
          stock_delete_data $(srcdir)/stock_delete.png

BUILT_SOURCES = inlinepixbufs.h
CLEANFILES =					\
	inlinepixbufs.h				\
	mutter.desktop				\
	mutter-wm.desktop			\
	mutter.schemas				\
	$(mutter_built_sources)			\
	$(typelib_DATA)				\
	$(gir_DATA)

inlinepixbufs.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) >$(srcdir)/inlinepixbufs.h

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libmutter-private.pc libmutter-wm.pc mutter-plugins.pc

EXTRA_DIST=$(desktopfiles_files) 	\
	$(wmproperties_files)		\
	$(IMAGES) 			\
	$(desktopfiles_in_files)	\
	$(wmproperties_in_files)	\
	$(schema_in_files)		\
	libmutter-private.pc.in	\
	libmutter-wm.pc.in \
	mutter-plugins.pc.in  \
	mutter-enum-types.h.in \
	mutter-enum-types.c.in \
	mutter-marshal.list

BUILT_SOURCES += $(mutter_built_sources)
MUTTER_STAMP_FILES = stamp-mutter-marshal.h stamp-mutter-enum-types.h
CLEANFILES += $(MUTTER_STAMP_FILES)

mutter-marshal.h: stamp-mutter-marshal.h
	@true
stamp-mutter-marshal.h: Makefile mutter-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--prefix=_mutter_marshal \
		--header \
	$(srcdir)/mutter-marshal.list > xgen-tmh && \
	(cmp -s xgen-tmh mutter-marshal.h || cp -f xgen-tmh mutter-marshal.h) && \
	rm -f xgen-tmh && \
	echo timestamp > $(@F)

mutter-marshal.c: Makefile mutter-marshal.list
	 $(AM_V_GEN) (echo "#include \"mutter-marshal.h\"" ; \
	 $(GLIB_GENMARSHAL) \
		--prefix=_mutter_marshal \
		--body \
	 $(srcdir)/mutter-marshal.list ) > xgen-tmc && \
	cp -f xgen-tmc mutter-marshal.c && \
	rm -f xgen-tmc

mutter-enum-types.h: stamp-mutter-enum-types.h Makefile
	@true
stamp-mutter-enum-types.h: $(libmutterinclude_base_headers) mutter-enum-types.h.in
	$(AM_V_GEN) ( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template $(srcdir)/mutter-enum-types.h.in \
	  $(libmutterinclude_base_headers) ) >> xgen-teth && \
	(cmp -s xgen-teth mutter-enum-types.h || cp xgen-teth mutter-enum-types.h) && \
	rm -f xgen-teth && \
	echo timestamp > $(@F)

mutter-enum-types.c: stamp-mutter-enum-types.h mutter-enum-types.c.in
	  $(AM_V_GEN) ( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template $(srcdir)/mutter-enum-types.c.in \
	  $(libmutterinclude_base_headers) ) >> xgen-tetc && \
	cp xgen-tetc mutter-enum-types.c && \
	rm -f xgen-tetc
