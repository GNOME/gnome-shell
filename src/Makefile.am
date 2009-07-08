lib_LTLIBRARIES = libmutter-private.la

SUBDIRS=wm-tester tools compositor/plugins

INCLUDES=@MUTTER_CFLAGS@ -I $(srcdir)/include -I$(srcdir)/compositor -DMUTTER_LIBEXECDIR=\"$(libexecdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" -DMUTTER_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\" -DMUTTER_PKGDATADIR=\"$(pkgdatadir)\" -DMUTTER_DATADIR=\"$(datadir)\" -DG_LOG_DOMAIN=\"mutter\" -DSN_API_NOT_YET_FROZEN=1 -DMUTTER_MAJOR_VERSION=$(MUTTER_MAJOR_VERSION) -DMUTTER_MINOR_VERSION=$(MUTTER_MINOR_VERSION) -DMUTTER_MICRO_VERSION=$(MUTTER_MICRO_VERSION) -DMUTTER_PLUGIN_API_VERSION=$(MUTTER_PLUGIN_API_VERSION) -DMUTTER_PKGLIBDIR=\"$(pkglibdir)\" -DMUTTER_PLUGIN_DIR=\"@MUTTER_PLUGIN_DIR@\"

mutter_built_sources = \
	mutter-marshal.h \
	mutter-marshal.c \
	mutter-enum-types.h \
	mutter-enum-types.c

mutter_SOURCES= 				\
	core/async-getprop.c			\
	core/async-getprop.h			\
	core/alttabhandler.c			\
	include/alttabhandler.h			\
	core/alttabhandlerdefault.c		\
	include/alttabhandlerdefault.h		\
	core/bell.c				\
	core/bell.h				\
	core/boxes.c				\
	include/boxes.h				\
	compositor/compositor.c			\
	compositor/compositor-private.h		\
	compositor/mutter-module.c		\
	compositor/mutter-module.h		\
	compositor/mutter-plugin.c		\
	compositor/mutter-plugin-manager.c	\
	compositor/mutter-plugin-manager.h	\
	compositor/mutter-shaped-texture.c	\
	compositor/mutter-window.c		\
	compositor/mutter-window-private.h	\
	compositor/shadow.c			\
	compositor/shadow.h			\
	compositor/mutter-shaped-texture.h	\
	compositor/tidy/tidy-texture-frame.c	\
	compositor/tidy/tidy-texture-frame.h	\
	include/compositor.h			\
	include/mutter-plugin.h			\
	include/mutter-window.h			\
	include/compositor-mutter.h 		\
	core/constraints.c			\
	core/constraints.h			\
	core/core.c				\
	core/delete.c				\
	core/display.c				\
	core/display-private.h			\
	include/display.h			\
	ui/draw-workspace.c			\
	ui/draw-workspace.h			\
	core/edge-resistance.c			\
	core/edge-resistance.h			\
	core/errors.c				\
	include/errors.h			\
	core/eventqueue.c			\
	core/eventqueue.h			\
	core/frame.c				\
	core/frame-private.h			\
	include/frame.h				\
	ui/gradient.c				\
	ui/gradient.h				\
	core/group-private.h			\
	core/group-props.c			\
	core/group-props.h			\
	core/group.c				\
	include/group.h				\
	core/iconcache.c			\
	core/iconcache.h			\
	core/keybindings.c			\
	core/keybindings-private.h		\
	core/main.c				\
	core/mutter-Xatomtype.h		\
	core/place.c				\
	core/place.h				\
	core/prefs.c				\
	include/prefs.h				\
	core/screen.c				\
	core/screen-private.h			\
	include/screen.h			\
	include/types.h                         \
	core/session.c				\
	core/session.h				\
	core/stack.c				\
	core/stack.h				\
	core/stack-tracker.c			\
	core/stack-tracker.h			\
	core/util.c				\
	include/util.h				\
	core/window-props.c			\
	core/window-props.h			\
	core/window.c				\
	core/window-private.h			\
	include/window.h			\
	core/workspace.c			\
	core/workspace-private.h		\
	core/xprops.c				\
	include/xprops.h			\
	include/common.h			\
	include/core.h				\
	include/ui.h				\
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
	include/resizepopup.h			\
	ui/tabpopup.c				\
	include/tabpopup.h				\
	ui/theme-parser.c			\
	ui/theme-parser.h			\
	ui/theme.c				\
	ui/theme.h				\
	ui/themewidget.c			\
	ui/themewidget.h			\
	ui/ui.c					\
	include/all-keybindings.h               \
	$(mutter_built_sources)

# by setting libmutter_private_la_CFLAGS, the files shared with
# mutter proper will be compiled with different names.
libmutter_private_la_CFLAGS =
libmutter_private_la_SOURCES=			\
	core/boxes.c				\
	include/boxes.h				\
	ui/gradient.c				\
	ui/gradient.h				\
	core/util.c				\
	include/util.h				\
	include/common.h			\
	ui/preview-widget.c			\
	ui/preview-widget.h			\
	ui/theme-parser.c			\
	ui/theme-parser.h			\
	ui/theme.c				\
	ui/theme.h

libmutter_private_la_LDFLAGS = -no-undefined
libmutter_private_la_LIBADD  = @MUTTER_LIBS@

libmutterincludedir = $(includedir)/mutter/mutter-private

libmutterinclude_base_headers =		\
	include/alttabhandler.h			\
	include/boxes.h				\
	ui/gradient.h				\
	include/main.h				\
	include/util.h				\
	include/common.h			\
	ui/preview-widget.h			\
	ui/theme-parser.h			\
	ui/theme.h				\
	include/prefs.h				\
	include/window.h			\
	include/workspace.h			\
	include/compositor.h			\
	include/compositor-mutter.h		\
	include/types.h				\
	include/screen.h			\
	include/display.h			\
	include/group.h				\
	include/keybindings.h			\
	include/mutter-plugin.h			\
	include/mutter-window.h

# Excluded from scanning for introspection but installed
libmutterinclude_extra_headers =		\
	include/atomnames.h

libmutterinclude_HEADERS =			\
	$(libmutterinclude_base_headers)	\
	$(libmutterinclude_extra_headers)

mutter_theme_viewer_SOURCES=  \
	ui/theme-viewer.c

bin_PROGRAMS=mutter mutter-theme-viewer

api_version = $(MUTTER_MAJOR_VERSION).$(MUTTER_MINOR_VERSION)

if WITH_INTROSPECTION
# These files are in package-private directories, even though they may be used
# by plugins.  If you're writing a plugin, use g-ir-compiler --add-include-path
# and g-ir-compiler --includedir.
girdir = $(pkglibdir)
gir_DATA = Meta-$(api_version).gir

typelibdir = $(pkglibdir)
typelib_DATA = Meta-$(api_version).typelib

# We need to strip out the attribute that would point back to libmutter-introspect
# so that libgirepository looks for symbols in the executable instead
Meta-$(api_version).gir: $(G_IR_SCANNER) mutter $(libmutterinclude_HEADERS) $(mutter_SOURCES)
	$(G_IR_SCANNER)					\
		--namespace=Meta			\
		--nsversion=$(api_version)		\
		--include=GObject-2.0			\
		--include=Gdk-2.0			\
		--include=Gtk-2.0			\
		--include=Clutter-0.9			\
		--pkg=clutter-0.9			\
		--pkg=gtk+-2.0				\
		--include=xfixes-4.0			\
		--program=./mutter			\
		$(filter %.c,$(mutter_SOURCES))	\
		$(libmutterinclude_base_headers)	\
		$(INCLUDES)				\
		-o $@

Meta-$(api_version).typelib: $(G_IR_COMPILER) Meta-$(api_version).gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. $(G_IR_COMPILER) Meta-$(api_version).gir -o $@
endif

EFENCE=
mutter_LDADD=@MUTTER_LIBS@ libmutter-private.la $(EFENCE)
mutter_LDFLAGS=-export-dynamic

mutter_theme_viewer_LDADD= @MUTTER_LIBS@ libmutter-private.la

testboxes_SOURCES=include/util.h core/util.c include/boxes.h core/boxes.c core/testboxes.c
testgradient_SOURCES=ui/gradient.h ui/gradient.c ui/testgradient.c
testasyncgetprop_SOURCES=core/async-getprop.h core/async-getprop.c core/testasyncgetprop.c

noinst_PROGRAMS=testboxes testgradient testasyncgetprop

testboxes_LDADD= @MUTTER_LIBS@ libmutter-private.la
testgradient_LDADD= @MUTTER_LIBS@ libmutter-private.la
testasyncgetprop_LDADD= @MUTTER_LIBS@ libmutter-private.la

@INTLTOOL_DESKTOP_RULE@

desktopfilesdir=$(datadir)/applications
desktopfiles_in_files=mutter.desktop.in
desktopfiles_files=$(desktopfiles_in_files:.desktop.in=.desktop)
desktopfiles_DATA = $(desktopfiles_files)

wmpropertiesdir=$(datadir)/gnome/wm-properties
wmproperties_in_files=mutter-wm.desktop.in
wmproperties_files=$(wmproperties_in_files:.desktop.in=.desktop)
wmproperties_DATA = $(wmproperties_files)

schemadir   = @GCONF_SCHEMA_FILE_DIR@
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
CLEANFILES = inlinepixbufs.h mutter.desktop mutter-wm.desktop mutter.schemas

inlinepixbufs.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) >$(srcdir)/inlinepixbufs.h

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libmutter-private.pc mutter-plugins.pc

EXTRA_DIST=$(desktopfiles_files) 	\
	$(wmproperties_files)		\
	$(IMAGES) $(schema_DATA) 	\
	$(desktopfiles_in_files)	\
	$(wmproperties_in_files)	\
	$(schema_in_files)		\
	libmutter-private.pc.in	\
	mutter-plugins.pc.in  \
	mutter-enum-types.h.in \
	mutter-enum-types.c.in \
	mutter-marshal.list

BUILT_SOURCES += $(mutter_built_sources)
MUTTER_STAMP_FILES = stamp-mutter-marshal.h
CLEANFILES += $(MUTTER_STAMP_FILES)

mutter-marshal.h: stamp-mutter-marshal.h
	@true
stamp-mutter-marshal.h: Makefile mutter-marshal.list
	$(GLIB_GENMARSHAL) \
		--prefix=_mutter_marshal \
		--header \
	$(srcdir)/mutter-marshal.list > xgen-tmh && \
	(cmp -s xgen-tmh mutter-marshal.h || cp -f xgen-tmh mutter-marshal.h) && \
	rm -f xgen-tmh && \
	echo timestamp > $(@F)

mutter-marshal.c: Makefile mutter-marshal.list
	(echo "#include \"mutter-marshal.h\"" ; \
	 $(GLIB_GENMARSHAL) \
		--prefix=_mutter_marshal \
		--body \
	 $(srcdir)/mutter-marshal.list ) > xgen-tmc && \
	cp -f xgen-tmc mutter-marshal.c && \
	rm -f xgen-tmc

mutter-enum-types.h: stamp-mutter-enum-types.h Makefile
	@true
stamp-mutter-enum-types.h: $(mutter_source_h) mutter-enum-types.h.in
	( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template $(srcdir)/mutter-enum-types.h.in \
	  $(libmutterinclude_base_headers) ) >> xgen-teth && \
	(cmp xgen-teth mutter-enum-types.h || cp xgen-teth mutter-enum-types.h) && \
	rm -f xgen-teth && \
	echo timestamp > $(@F)

mutter-enum-types.c: stamp-mutter-enum-types.h mutter-enum-types.c.in
	( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template $(srcdir)/mutter-enum-types.c.in \
	  $(libmutterinclude_base_headers) ) >> xgen-tetc && \
	cp xgen-tetc mutter-enum-types.c && \
	rm -f xgen-tetc

