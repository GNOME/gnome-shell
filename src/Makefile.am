# Flag build for parallelism; see https://savannah.gnu.org/patch/?6905
.AUTOPARALLEL:

lib_LTLIBRARIES = libmutter-wayland.la

SUBDIRS=compositor/plugins

INCLUDES=								\
	-DCLUTTER_ENABLE_EXPERIMENTAL_API				\
	-DCOGL_ENABLE_EXPERIMENTAL_API					\
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API                              \
	$(MUTTER_CFLAGS)						\
	-I$(top_builddir)						\
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

INCLUDES += \
	-I$(srcdir)/wayland						\
	-I$(builddir)/wayland						\
	-DXWAYLAND_PATH='"@XWAYLAND_PATH@"'

mutter_built_sources = \
	$(dbus_idle_built_sources)		\
	$(dbus_xrandr_built_sources)		\
	mutter-enum-types.h 			\
	mutter-enum-types.c			\
	wayland/gtk-shell-protocol.c		\
	wayland/gtk-shell-server-protocol.h	\
	wayland/gtk-shell-client-protocol.h	\
	wayland/xserver-protocol.c		\
	wayland/xserver-server-protocol.h	\
	wayland/xserver-client-protocol.h

libmutter_wayland_la_SOURCES =			\
	core/async-getprop.c			\
	core/async-getprop.h			\
	core/barrier.c				\
	meta/barrier.h				\
	core/bell.c				\
	core/bell.h				\
	core/boxes.c				\
	core/boxes-private.h			\
	meta/boxes.h				\
	compositor/clutter-utils.c		\
	compositor/clutter-utils.h		\
	compositor/cogl-utils.c			\
	compositor/cogl-utils.h			\
	compositor/compositor.c			\
	compositor/compositor-private.h		\
	compositor/meta-background.c		\
	compositor/meta-background-actor.c	\
	compositor/meta-background-actor-private.h	\
	compositor/meta-background-group.c	\
	compositor/meta-background-group-private.h	\
	compositor/meta-module.c		\
	compositor/meta-module.h		\
	compositor/meta-plugin.c		\
	compositor/meta-plugin-manager.c	\
	compositor/meta-plugin-manager.h	\
	compositor/meta-shadow-factory.c	\
	compositor/meta-shadow-factory-private.h	\
	compositor/meta-shaped-texture.c	\
	compositor/meta-shaped-texture-private.h 	\
	compositor/meta-texture-rectangle.c	\
	compositor/meta-texture-rectangle.h	\
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
	meta/meta-background.h			\
	meta/meta-background-actor.h		\
	meta/meta-background-group.h		\
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
	core/edid-parse.c			\
	core/edid.h				\
	core/errors.c				\
	meta/errors.h				\
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
	core/meta-cursor-tracker.c		\
	core/meta-cursor-tracker-private.h	\
	core/meta-idle-monitor.c		\
	core/meta-idle-monitor-private.h	\
	core/meta-xrandr-shared.h		\
	core/monitor.c				\
	core/monitor-config.c			\
	core/monitor-private.h			\
	core/monitor-xrandr.c			\
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
	core/util-private.h			\
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
	$(mutter_built_sources)

libmutter_wayland_la_SOURCES +=			\
	wayland/meta-wayland.c			\
	wayland/meta-wayland-private.h		\
	wayland/meta-xwayland-private.h		\
	wayland/meta-xwayland.c			\
	wayland/meta-wayland-data-device.c      \
	wayland/meta-wayland-data-device.h      \
	wayland/meta-wayland-keyboard.c		\
	wayland/meta-wayland-keyboard.h		\
	wayland/meta-wayland-pointer.c		\
	wayland/meta-wayland-pointer.h		\
	wayland/meta-wayland-seat.c		\
	wayland/meta-wayland-seat.h		\
	wayland/meta-wayland-stage.h		\
	wayland/meta-wayland-stage.c		\
	wayland/meta-wayland-surface.c		\
	wayland/meta-wayland-surface.h		\
	wayland/meta-wayland-types.h		\
	wayland/meta-wayland-versions.h		\
	wayland/meta-weston-launch.c		\
	wayland/meta-weston-launch.h

libmutter_wayland_la_LDFLAGS = -no-undefined
libmutter_wayland_la_LIBADD  = $(MUTTER_LIBS)

# Headers installed for plugins; introspected information will
# be extracted into Mutter-<version>.gir
libmutterinclude_base_headers =		\
	meta/barrier.h				\
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
	meta/meta-background-actor.h		\
	meta/meta-background-group.h		\
	meta/meta-background.h			\
	meta/meta-cursor-tracker.h		\
	meta/meta-idle-monitor.h		\
	meta/meta-plugin.h			\
	meta/meta-shaped-texture.h		\
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
# atomnames.h: macros cause problems for scanning process
libmutterinclude_extra_headers =		\
	meta/atomnames.h

libmutterincludedir = $(includedir)/mutter-wayland/meta

libmutterinclude_HEADERS =			\
	$(libmutterinclude_base_headers)	\
	$(libmutterinclude_extra_headers)

bin_PROGRAMS=mutter-wayland

mutter_wayland_SOURCES = core/mutter.c
mutter_wayland_LDADD = $(MUTTER_LIBS) libmutter-wayland.la

bin_PROGRAMS+=mutter-launch

mutter_launch_SOURCES = wayland/weston-launch.c wayland/weston-launch.h

mutter_launch_CFLAGS = $(MUTTER_LAUNCH_CFLAGS) -DLIBDIR=\"$(libdir)\"
mutter_launch_LDFLAGS = $(MUTTER_LAUNCH_LIBS) -lpam

install-exec-hook:
	-chown root $(DESTDIR)$(bindir)/mutter-launch
	-chmod u+s $(DESTDIR)$(bindir)/mutter-launch

if HAVE_INTROSPECTION
include $(INTROSPECTION_MAKEFILE)

# Since we don't make any guarantees about stability and we don't support
# parallel install, there's no real reason to change directories, filenames,
# etc. as we change the Mutter tarball version.
#api_version = $(MUTTER_MAJOR_VERSION).$(MUTTER_MINOR_VERSION)
api_version = 3.0

# These files are in package-private directories, even though they may be used
# by plugins.  If you're writing a plugin, use g-ir-compiler --add-include-path
# and g-ir-compiler --includedir.
girdir = $(pkglibdir)
gir_DATA = Meta-$(api_version).gir

typelibdir = $(pkglibdir)
typelib_DATA = Meta-$(api_version).typelib

INTROSPECTION_GIRS = Meta-$(api_version).gir

Meta-$(api_version).gir: libmutter-wayland.la
@META_GIR@_INCLUDES = GObject-2.0 GDesktopEnums-3.0 Gdk-3.0 Gtk-3.0 Clutter-1.0 xlib-2.0 xfixes-4.0 Cogl-1.0
@META_GIR@_EXPORT_PACKAGES = libmutter-wayland
@META_GIR@_CFLAGS = $(INCLUDES)
@META_GIR@_LIBS = libmutter-wayland.la
@META_GIR@_FILES =				\
	mutter-enum-types.h			\
	$(libmutterinclude_base_headers)	\
	$(filter %.c,$(libmutter_wayland_la_SOURCES))
@META_GIR@_SCANNERFLAGS = --warn-all --warn-error

endif

testboxes_SOURCES = core/testboxes.c
testgradient_SOURCES = ui/testgradient.c
testasyncgetprop_SOURCES = core/testasyncgetprop.c

noinst_PROGRAMS=testboxes testgradient testasyncgetprop

testboxes_LDADD = $(MUTTER_LIBS) libmutter-wayland.la
testgradient_LDADD = $(MUTTER_LIBS) libmutter-wayland.la
testasyncgetprop_LDADD = $(MUTTER_LIBS) libmutter-wayland.la

@INTLTOOL_DESKTOP_RULE@

desktopfilesdir=$(datadir)/applications
desktopfiles_in_files=mutter-wayland.desktop.in
desktopfiles_files=$(desktopfiles_in_files:.desktop.in=.desktop)
desktopfiles_DATA = $(desktopfiles_files)

xmldir       = @GNOME_KEYBINDINGS_KEYSDIR@
xml_in_files = \
        50-mutter-navigation.xml.in	\
        50-mutter-system.xml.in		\
        50-mutter-windows.xml.in
xml_DATA     = $(xml_in_files:.xml.in=.xml)

dbus_idle_built_sources = meta-dbus-idle-monitor.c meta-dbus-idle-monitor.h

gsettings_SCHEMAS = org.gnome.mutter.gschema.xml org.gnome.mutter.wayland.gschema.xml
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = mutter-schemas.convert

CLEANFILES =					\
	mutter-wayland.desktop			\
	mutter-wm.desktop			\
	org.gnome.mutter.gschema.xml		\
	org.gnome.mutter.wayland.gschema.xml	\
	$(xml_DATA)				\
	$(mutter_built_sources)			\
	$(typelib_DATA)				\
	$(gir_DATA)

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libmutter-wayland.pc

EXTRA_DIST=$(desktopfiles_files) 	\
	$(wmproperties_files)		\
	$(IMAGES) 			\
	$(desktopfiles_in_files)	\
	$(wmproperties_in_files)	\
	$(xml_in_files)			\
	org.gnome.mutter.gschema.xml.in \
	org.gnome.mutter.wayland.gschema.xml.in \
	mutter-schemas.convert \
	libmutter-wayland.pc.in \
	mutter-enum-types.h.in \
	mutter-enum-types.c.in \
	xrandr.xml idle-monitor.xml

BUILT_SOURCES = $(mutter_built_sources)
MUTTER_STAMP_FILES = stamp-mutter-enum-types.h
CLEANFILES += $(MUTTER_STAMP_FILES)

mutter-enum-types.h: stamp-mutter-enum-types.h Makefile
	@true
stamp-mutter-enum-types.h: $(libmutterinclude_base_headers) mutter-enum-types.h.in
	$(AM_V_GEN) ( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template mutter-enum-types.h.in \
	  $(libmutterinclude_base_headers) ) >> xgen-teth && \
	(cmp -s xgen-teth mutter-enum-types.h || cp xgen-teth mutter-enum-types.h) && \
	rm -f xgen-teth && \
	echo timestamp > $(@F)

mutter-enum-types.c: stamp-mutter-enum-types.h mutter-enum-types.c.in
	  $(AM_V_GEN) ( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template mutter-enum-types.c.in \
	  $(libmutterinclude_base_headers) ) >> xgen-tetc && \
	cp xgen-tetc mutter-enum-types.c && \
	rm -f xgen-tetc

dbus_xrandr_built_sources = meta-dbus-xrandr.c meta-dbus-xrandr.h

$(dbus_xrandr_built_sources) : Makefile.am xrandr.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.gnome.Mutter					\
		--c-namespace MetaDBus							\
		--generate-c-code meta-dbus-xrandr					\
		$(srcdir)/xrandr.xml

dbus_idle_built_sources = meta-dbus-idle-monitor.c meta-dbus-idle-monitor.h

$(dbus_idle_built_sources) : Makefile.am idle-monitor.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.gnome.Mutter					\
		--c-namespace MetaDBus							\
		--generate-c-code meta-dbus-idle-monitor				\
		--c-generate-object-manager						\
		$(srcdir)/idle-monitor.xml

wayland/%-protocol.c : $(top_builddir)/protocol/%.xml
	mkdir -p wayland
	$(AM_V_GEN)$(WAYLAND_SCANNER) code < $< > $@
wayland/%-server-protocol.h : $(top_builddir)/protocol/%.xml
	mkdir -p wayland
	$(AM_V_GEN)$(WAYLAND_SCANNER) server-header < $< > $@
wayland/%-client-protocol.h : $(top_builddir)/protocol/%.xml
	mkdir -p wayland
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < $< > $@
