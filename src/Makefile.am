# Flag build for parallelism; see https://savannah.gnu.org/patch/?6905
.AUTOPARALLEL:

lib_LTLIBRARIES = libmutter.la

SUBDIRS=compositor/plugins

INCLUDES=								\
	-DCLUTTER_ENABLE_COMPOSITOR_API					\
	-DCLUTTER_ENABLE_EXPERIMENTAL_API				\
	-DCOGL_ENABLE_EXPERIMENTAL_API					\
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API                              \
	$(MUTTER_CFLAGS)						\
	-I$(srcdir)							\
	-I$(srcdir)/backends						\
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
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\"			\
	-DXWAYLAND_PATH='"@XWAYLAND_PATH@"'

mutter_built_sources = \
	$(dbus_idle_built_sources)		\
	$(dbus_display_config_built_sources)	\
	$(dbus_login1_built_sources)		\
	mutter-enum-types.h 			\
	mutter-enum-types.c			\
	gtk-shell-protocol.c			\
	gtk-shell-server-protocol.h		\
	xdg-shell-protocol.c			\
	xdg-shell-server-protocol.h

wayland_protocols = \
	wayland/protocol/gtk-shell.xml		\
	wayland/protocol/xdg-shell.xml

libmutter_la_SOURCES =				\
	backends/meta-backend.c			\
	backends/meta-backend.h			\
	backends/meta-backend-private.h		\
	backends/meta-cursor.c			\
	backends/meta-cursor.h			\
	backends/meta-cursor-private.h		\
	backends/meta-cursor-tracker.c		\
	backends/meta-cursor-tracker-private.h	\
	backends/meta-cursor-renderer.c		\
	backends/meta-cursor-renderer.h		\
	backends/meta-display-config-shared.h	\
	backends/meta-idle-monitor.c		\
	backends/meta-idle-monitor-private.h	\
	backends/meta-idle-monitor-dbus.c	\
	backends/meta-idle-monitor-dbus.h	\
	backends/meta-monitor-config.c		\
	backends/meta-monitor-config.h		\
	backends/meta-monitor-manager.c		\
	backends/meta-monitor-manager.h		\
	backends/meta-monitor-manager-dummy.c	\
	backends/meta-monitor-manager-dummy.h	\
	backends/edid-parse.c			\
	backends/edid.h				\
	backends/native/meta-backend-native.c		\
	backends/native/meta-backend-native.h		\
	backends/native/meta-cursor-renderer-native.c	\
	backends/native/meta-cursor-renderer-native.h	\
	backends/native/meta-idle-monitor-native.c	\
	backends/native/meta-idle-monitor-native.h	\
	backends/native/meta-monitor-manager-kms.c	\
	backends/native/meta-monitor-manager-kms.h	\
	backends/native/meta-launcher.c			\
	backends/native/meta-launcher.h			\
	backends/native/dbus-utils.c			\
	backends/native/dbus-utils.h			\
	backends/x11/meta-backend-x11.c			\
	backends/x11/meta-backend-x11.h			\
	backends/x11/meta-cursor-renderer-x11.c		\
	backends/x11/meta-cursor-renderer-x11.h		\
	backends/x11/meta-idle-monitor-xsync.c		\
	backends/x11/meta-idle-monitor-xsync.h		\
	backends/x11/meta-monitor-manager-xrandr.c	\
	backends/x11/meta-monitor-manager-xrandr.h	\
	core/meta-accel-parse.c			\
	core/meta-accel-parse.h			\
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
	compositor/meta-cullable.c		\
	compositor/meta-cullable.h		\
	compositor/meta-module.c		\
	compositor/meta-module.h		\
	compositor/meta-plugin.c		\
	compositor/meta-plugin-manager.c	\
	compositor/meta-plugin-manager.h	\
	compositor/meta-shadow-factory.c	\
	compositor/meta-shadow-factory-private.h	\
	compositor/meta-shaped-texture.c	\
	compositor/meta-shaped-texture-private.h 	\
	compositor/meta-surface-actor.c		\
	compositor/meta-surface-actor.h		\
	compositor/meta-surface-actor-x11.c	\
	compositor/meta-surface-actor-x11.h	\
	compositor/meta-surface-actor-wayland.c	\
	compositor/meta-surface-actor-wayland.h	\
	compositor/meta-stage.h			\
	compositor/meta-stage.c			\
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
	core/constraints.c			\
	core/constraints.h			\
	core/core.c				\
	core/delete.c				\
	core/display.c				\
	core/display-private.h			\
	meta/display.h				\
	core/edge-resistance.c			\
	core/edge-resistance.h			\
	core/events.c				\
	core/events.h				\
	core/errors.c				\
	meta/errors.h				\
	core/frame.c				\
	core/frame.h				\
	ui/gradient.c				\
	meta/gradient.h				\
	core/keybindings.c			\
	core/keybindings-private.h		\
	core/main.c				\
	core/place.c				\
	core/place.h				\
	core/prefs.c				\
	meta/prefs.h				\
	core/screen.c				\
	core/screen-private.h			\
	meta/screen.h				\
	meta/types.h				\
	core/stack.c				\
	core/stack.h				\
	core/stack-tracker.c			\
	core/stack-tracker.h			\
	core/util.c				\
	meta/util.h				\
	core/util-private.h			\
	core/window.c				\
	core/window-private.h			\
	meta/window.h				\
	core/workspace.c			\
	core/workspace-private.h		\
	meta/common.h				\
	core/core.h				\
	ui/ui.h					\
	ui/frames.c				\
	ui/frames.h				\
	ui/resizepopup.c			\
	ui/resizepopup.h			\
	ui/theme-parser.c			\
	ui/theme.c				\
	meta/theme.h				\
	ui/theme-private.h			\
	ui/ui.c					\
	x11/iconcache.c				\
	x11/iconcache.h				\
	x11/async-getprop.c			\
	x11/async-getprop.h			\
	x11/group-private.h			\
	x11/group-props.c			\
	x11/group-props.h			\
	x11/group.c				\
	meta/group.h				\
	x11/session.c				\
	x11/session.h				\
	x11/window-props.c			\
	x11/window-props.h			\
	x11/window-x11.c			\
	x11/window-x11.h			\
	x11/window-x11-private.h		\
	x11/xprops.c				\
	x11/xprops.h				\
	x11/mutter-Xatomtype.h			\
	wayland/meta-wayland.c			\
	wayland/meta-wayland.h			\
	wayland/meta-wayland-private.h		\
	wayland/meta-xwayland.c			\
	wayland/meta-xwayland.h			\
	wayland/meta-xwayland-private.h		\
	wayland/meta-wayland-data-device.c      \
	wayland/meta-wayland-data-device.h      \
	wayland/meta-wayland-keyboard.c		\
	wayland/meta-wayland-keyboard.h		\
	wayland/meta-wayland-pointer.c		\
	wayland/meta-wayland-pointer.h		\
	wayland/meta-wayland-seat.c		\
	wayland/meta-wayland-seat.h		\
	wayland/meta-wayland-surface.c		\
	wayland/meta-wayland-surface.h		\
	wayland/meta-wayland-types.h		\
	wayland/meta-wayland-versions.h		\
	wayland/meta-wayland-outputs.c		\
	wayland/meta-wayland-outputs.h		\
	wayland/window-wayland.c		\
	wayland/window-wayland.h

nodist_libmutter_la_SOURCES =			\
	$(mutter_built_sources)

libmutter_la_LDFLAGS = -no-undefined
libmutter_la_LIBADD  = $(MUTTER_LIBS)

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

libmutterincludedir = $(includedir)/mutter/meta

libmutterinclude_HEADERS =			\
	$(libmutterinclude_base_headers)	\
	$(libmutterinclude_extra_headers)

bin_PROGRAMS=mutter

mutter_SOURCES = core/mutter.c
mutter_LDADD = $(MUTTER_LIBS) libmutter.la

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

Meta-$(api_version).gir: libmutter.la
@META_GIR@_INCLUDES = GObject-2.0 GDesktopEnums-3.0 Gdk-3.0 Gtk-3.0 Clutter-1.0 xlib-2.0 xfixes-4.0 Cogl-1.0
@META_GIR@_EXPORT_PACKAGES = libmutter
@META_GIR@_CFLAGS = $(INCLUDES)
@META_GIR@_LIBS = libmutter.la
@META_GIR@_FILES =				\
	mutter-enum-types.h			\
	$(libmutterinclude_base_headers)	\
	$(filter %.c,$(libmutter_la_SOURCES) $(nodist_libmutter_la_SOURCES))
@META_GIR@_SCANNERFLAGS = --warn-all --warn-error

endif

testboxes_SOURCES = core/testboxes.c
testgradient_SOURCES = ui/testgradient.c
testasyncgetprop_SOURCES = x11/testasyncgetprop.c

noinst_PROGRAMS=testboxes testgradient testasyncgetprop

testboxes_LDADD = $(MUTTER_LIBS) libmutter.la
testgradient_LDADD = $(MUTTER_LIBS) libmutter.la
testasyncgetprop_LDADD = $(MUTTER_LIBS) libmutter.la

@INTLTOOL_DESKTOP_RULE@

desktopfilesdir=$(datadir)/applications
desktopfiles_in_files=mutter.desktop.in
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
	mutter.desktop				\
	mutter-wm.desktop			\
	org.gnome.mutter.gschema.xml		\
	org.gnome.mutter.wayland.gschema.xml	\
	$(xml_DATA)				\
	$(mutter_built_sources)			\
	$(typelib_DATA)				\
	$(gir_DATA)

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libmutter.pc

EXTRA_DIST=$(desktopfiles_files) 	\
	$(wmproperties_files)		\
	$(IMAGES) 			\
	$(desktopfiles_in_files)	\
	$(wmproperties_in_files)	\
	$(xml_in_files)			\
	$(wayland_protocols)		\
	org.gnome.mutter.gschema.xml.in \
	org.gnome.mutter.wayland.gschema.xml.in \
	mutter-schemas.convert \
	libmutter.pc.in \
	mutter-enum-types.h.in \
	mutter-enum-types.c.in \
	org.freedesktop.login1.xml	\
	org.gnome.Mutter.DisplayConfig.xml	\
	org.gnome.Mutter.IdleMonitor.xml

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

dbus_display_config_built_sources = meta-dbus-display-config.c meta-dbus-display-config.h

$(dbus_display_config_built_sources) : Makefile.am org.gnome.Mutter.DisplayConfig.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.gnome.Mutter					\
		--c-namespace MetaDBus							\
		--generate-c-code meta-dbus-display-config				\
		$(srcdir)/org.gnome.Mutter.DisplayConfig.xml

$(dbus_idle_built_sources) : Makefile.am org.gnome.Mutter.IdleMonitor.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.gnome.Mutter					\
		--c-namespace MetaDBus							\
		--generate-c-code meta-dbus-idle-monitor				\
		--c-generate-object-manager						\
		$(srcdir)/org.gnome.Mutter.IdleMonitor.xml

dbus_login1_built_sources = meta-dbus-login1.c meta-dbus-login1.h

$(dbus_login1_built_sources) : Makefile.am org.freedesktop.login1.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.freedesktop.login1				\
		--c-namespace Login1							\
		--generate-c-code meta-dbus-login1					\
		$(srcdir)/org.freedesktop.login1.xml

%-protocol.c : $(srcdir)/wayland/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) code < $< > $@
%-server-protocol.h : $(srcdir)/wayland/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) server-header < $< > $@
