# Flag build for parallelism; see https://savannah.gnu.org/patch/?6905
.AUTOPARALLEL:

lib_LTLIBRARIES = libmutter-@LIBMUTTER_API_VERSION@.la

SUBDIRS=compositor/plugins

EXTRA_DIST =
NULL =

AM_CPPFLAGS = \
	-DCLUTTER_ENABLE_COMPOSITOR_API					\
	-DCLUTTER_ENABLE_EXPERIMENTAL_API				\
	-DCOGL_ENABLE_EXPERIMENTAL_API					\
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API                              \
	-DCOGL_ENABLE_MUTTER_API					\
	-DCLUTTER_DISABLE_DEPRECATION_WARNINGS				\
	-DCOGL_DISABLE_DEPRECATION_WARNINGS				\
	$(MUTTER_CFLAGS)						\
	$(MUTTER_NATIVE_BACKEND_CFLAGS)					\
	-I$(builddir)							\
	-I$(srcdir)							\
	-I$(srcdir)/backends						\
	-I$(srcdir)/core						\
	-I$(srcdir)/ui							\
	-I$(srcdir)/compositor						\
	-I$(top_srcdir)/cogl						\
	-I$(top_srcdir)/cogl/cogl					\
	-I$(top_srcdir)/cogl/cogl/winsys				\
	-I$(top_builddir)/cogl/cogl					\
	-I$(top_builddir)/cogl						\
	-I$(top_srcdir)/clutter						\
	-I$(top_srcdir)/clutter/clutter					\
	-I$(top_builddir)/clutter					\
	-I$(top_builddir)/clutter/clutter				\
	-DMUTTER_LIBEXECDIR=\"$(libexecdir)\"				\
	-DMUTTER_LOCALEDIR=\"$(localedir)\"				\
	-DMUTTER_PKGDATADIR=\"$(pkgdatadir)\"				\
	-DMUTTER_DATADIR=\"$(datadir)\"					\
	-DG_LOG_DOMAIN=\"mutter\"					\
	-DSN_API_NOT_YET_FROZEN=1					\
	-DMUTTER_PKGLIBDIR=\"$(pkglibdir)\"				\
	-DMUTTER_PLUGIN_DIR=\"$(MUTTER_PLUGIN_DIR)\"			\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\"			\
	-DXWAYLAND_PATH=\"$(XWAYLAND_PATH)\"				\
	$(NULL)

mutter_built_sources = \
	$(dbus_idle_built_sources)		\
	$(dbus_display_config_built_sources)	\
	$(dbus_login1_built_sources)		\
	meta/meta-enum-types.h			\
	meta-enum-types.c			\
	$(NULL)

if HAVE_WAYLAND
mutter_built_sources += \
	pointer-gestures-unstable-v1-protocol.c				\
	pointer-gestures-unstable-v1-server-protocol.h			\
	gtk-shell-protocol.c			\
	gtk-shell-server-protocol.h		\
	gtk-primary-selection-protocol.c				\
	gtk-primary-selection-server-protocol.h				\
	xdg-shell-unstable-v6-protocol.c				\
	xdg-shell-unstable-v6-server-protocol.h				\
	relative-pointer-unstable-v1-protocol.c				\
	relative-pointer-unstable-v1-server-protocol.h			\
	pointer-constraints-unstable-v1-protocol.c			\
	pointer-constraints-unstable-v1-server-protocol.h		\
	tablet-unstable-v2-protocol.c					\
	tablet-unstable-v2-server-protocol.h				\
	xdg-foreign-unstable-v1-protocol.c				\
	xdg-foreign-unstable-v1-server-protocol.h			\
	$(NULL)
endif

wayland_protocols =				\
	wayland/protocol/gtk-shell.xml		\
	wayland/protocol/gtk-primary-selection.xml	\
	$(NULL)

libmutter_@LIBMUTTER_API_VERSION@_la_SOURCES =	\
	backends/meta-backend.c			\
	meta/meta-backend.h			\
	meta/meta-settings.h			\
	backends/meta-backend-private.h		\
	backends/meta-barrier.c			\
	backends/meta-barrier-private.h		\
	backends/meta-cursor.c			\
	backends/meta-cursor.h			\
	backends/meta-cursor-tracker.c		\
	backends/meta-cursor-tracker-private.h	\
	backends/meta-cursor-renderer.c		\
	backends/meta-cursor-renderer.h		\
	backends/meta-dnd-private.h		\
	backends/meta-egl.c			\
	backends/meta-egl.h			\
	backends/meta-egl-ext.h			\
	backends/meta-display-config-shared.h	\
	backends/meta-idle-monitor.c		\
	backends/meta-idle-monitor-private.h	\
	backends/meta-idle-monitor-dbus.c	\
	backends/meta-idle-monitor-dbus.h	\
	backends/meta-input-settings.c		\
	backends/meta-input-settings-private.h	\
	backends/meta-logical-monitor.c		\
	backends/meta-logical-monitor.h		\
	backends/meta-monitor-config.c		\
	backends/meta-monitor-config.h		\
	backends/meta-monitor-config-manager.c	\
	backends/meta-monitor-config-manager.h	\
	backends/meta-monitor-config-store.c	\
	backends/meta-monitor-config-store.h	\
	backends/meta-monitor.c			\
	backends/meta-monitor.h			\
	backends/meta-monitor-manager.c		\
	meta/meta-monitor-manager.h		\
	backends/meta-monitor-manager-private.h	\
	backends/meta-monitor-manager-dummy.c	\
	backends/meta-monitor-manager-dummy.h	\
	backends/meta-pointer-constraint.c	\
	backends/meta-pointer-constraint.h	\
	backends/meta-settings.c		\
	backends/meta-settings-private.h	\
	backends/meta-stage.h			\
	backends/meta-stage.c			\
	backends/meta-renderer.c		\
	backends/meta-renderer.h		\
	backends/meta-renderer-view.c		\
	backends/meta-renderer-view.h		\
	backends/edid-parse.c			\
	backends/edid.h				\
	backends/x11/meta-backend-x11.c			\
	backends/x11/meta-backend-x11.h			\
	backends/x11/meta-barrier-x11.c			\
	backends/x11/meta-barrier-x11.h			\
	backends/x11/meta-clutter-backend-x11.c		\
	backends/x11/meta-clutter-backend-x11.h		\
	backends/x11/meta-cursor-renderer-x11.c		\
	backends/x11/meta-cursor-renderer-x11.h		\
	backends/x11/cm/meta-backend-x11-cm.c		\
	backends/x11/cm/meta-backend-x11-cm.h		\
	backends/x11/cm/meta-renderer-x11-cm.c		\
	backends/x11/cm/meta-renderer-x11-cm.h		\
	backends/x11/nested/meta-backend-x11-nested.c	\
	backends/x11/nested/meta-backend-x11-nested.h	\
	backends/x11/nested/meta-cursor-renderer-x11-nested.c		\
	backends/x11/nested/meta-cursor-renderer-x11-nested.h		\
	backends/x11/nested/meta-renderer-x11-nested.c	\
	backends/x11/nested/meta-renderer-x11-nested.h	\
	backends/x11/meta-idle-monitor-xsync.c		\
	backends/x11/meta-idle-monitor-xsync.h		\
	backends/x11/meta-input-settings-x11.c		\
	backends/x11/meta-input-settings-x11.h		\
	backends/x11/meta-monitor-manager-xrandr.c	\
	backends/x11/meta-monitor-manager-xrandr.h	\
	backends/x11/meta-renderer-x11.c		\
	backends/x11/meta-renderer-x11.h		\
	backends/x11/meta-stage-x11-nested.c		\
	backends/x11/meta-stage-x11-nested.h		\
	core/meta-accel-parse.c			\
	core/meta-accel-parse.h			\
	meta/barrier.h				\
	core/bell.c				\
	core/bell.h				\
	core/boxes.c				\
	core/boxes-private.h			\
	meta/boxes.h				\
	core/meta-border.c			\
	core/meta-border.h			\
	compositor/clutter-utils.c		\
	compositor/clutter-utils.h		\
	compositor/cogl-utils.c			\
	compositor/cogl-utils.h			\
	compositor/compositor.c			\
	compositor/compositor-private.h		\
	compositor/meta-background.c		\
	compositor/meta-background-private.h	\
	compositor/meta-background-actor.c	\
	compositor/meta-background-actor-private.h	\
	compositor/meta-background-image.c	\
	compositor/meta-background-group.c	\
	compositor/meta-cullable.c		\
	compositor/meta-cullable.h		\
	compositor/meta-dnd.c		\
	compositor/meta-dnd-actor.c		\
	compositor/meta-dnd-actor-private.h	\
	compositor/meta-feedback-actor.c	\
	compositor/meta-feedback-actor-private.h	\
	compositor/meta-module.c		\
	compositor/meta-module.h		\
	compositor/meta-plugin.c		\
	compositor/meta-plugin-manager.c	\
	compositor/meta-plugin-manager.h	\
	compositor/meta-shadow-factory.c	\
	compositor/meta-shaped-texture.c	\
	compositor/meta-shaped-texture-private.h 	\
	compositor/meta-surface-actor.c		\
	compositor/meta-surface-actor.h		\
	compositor/meta-surface-actor-x11.c	\
	compositor/meta-surface-actor-x11.h	\
	compositor/meta-sync-ring.c		\
	compositor/meta-sync-ring.h		\
	compositor/meta-texture-rectangle.c	\
	compositor/meta-texture-rectangle.h	\
	compositor/meta-texture-tower.c		\
	compositor/meta-texture-tower.h		\
	compositor/meta-window-actor.c		\
	compositor/meta-window-actor-private.h	\
	compositor/meta-window-group.c		\
	compositor/meta-window-group.h		\
	compositor/meta-window-shape.c		\
	compositor/region-utils.c		\
	compositor/region-utils.h		\
	meta/compositor.h			\
	meta/meta-background.h			\
	meta/meta-background-actor.h		\
	meta/meta-background-image.h		\
	meta/meta-background-group.h		\
	meta/meta-plugin.h			\
	meta/meta-shadow-factory.h		\
	meta/meta-window-actor.h		\
	meta/meta-window-shape.h		\
	meta/compositor-mutter.h 		\
	core/constraints.c			\
	core/constraints.h			\
	core/core.c				\
	core/meta-close-dialog.c		\
	meta/meta-close-dialog.h		\
	core/meta-close-dialog-default.c	\
	core/meta-close-dialog-default-private.h	\
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
	core/meta-gesture-tracker.c		\
	core/meta-gesture-tracker-private.h	\
	core/keybindings.c			\
	core/keybindings-private.h		\
	core/main-private.h			\
	core/main.c				\
	core/place.c				\
	core/place.h				\
	core/prefs.c				\
	meta/prefs.h				\
	core/screen.c				\
	core/screen-private.h			\
	meta/screen.h				\
	core/startup-notification.c		\
	core/startup-notification-private.h	\
	meta/types.h				\
	core/restart.c				\
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
	ui/theme.c				\
	meta/theme.h				\
	ui/theme-private.h			\
	ui/ui.c					\
	x11/atomnames.h				\
	x11/events.c				\
	x11/events.h				\
	x11/group-private.h			\
	x11/group-props.c			\
	x11/group-props.h			\
	x11/group.c				\
	meta/group.h				\
	x11/iconcache.c				\
	x11/iconcache.h				\
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
	$(NULL)

if HAVE_WAYLAND
libmutter_@LIBMUTTER_API_VERSION@_la_SOURCES +=	\
	compositor/meta-surface-actor-wayland.c	\
	compositor/meta-surface-actor-wayland.h	\
	wayland/meta-wayland.c			\
	wayland/meta-wayland.h			\
	wayland/meta-wayland-private.h		\
	wayland/meta-xwayland.c			\
	wayland/meta-xwayland.h			\
	wayland/meta-xwayland-selection.c	\
	wayland/meta-xwayland-selection-private.h	\
	wayland/meta-xwayland-private.h		\
	wayland/meta-wayland-buffer.c      	\
	wayland/meta-wayland-buffer.h      	\
	wayland/meta-wayland-region.c      	\
	wayland/meta-wayland-region.h      	\
	wayland/meta-wayland-data-device.c      \
	wayland/meta-wayland-data-device.h      \
	wayland/meta-wayland-data-device-private.h	\
	wayland/meta-wayland-egl-stream.c	\
	wayland/meta-wayland-egl-stream.h	\
	wayland/meta-wayland-input-device.c	\
	wayland/meta-wayland-input-device.h	\
	wayland/meta-wayland-pointer-gestures.c	\
	wayland/meta-wayland-pointer-gestures.h	\
	wayland/meta-wayland-pointer-gesture-swipe.c	\
	wayland/meta-wayland-pointer-gesture-swipe.h	\
	wayland/meta-wayland-pointer-gesture-pinch.c	\
	wayland/meta-wayland-pointer-gesture-pinch.h	\
	wayland/meta-wayland-keyboard.c		\
	wayland/meta-wayland-keyboard.h		\
	wayland/meta-wayland-pointer.c		\
	wayland/meta-wayland-pointer.h		\
	wayland/meta-wayland-pointer-constraints.c	\
	wayland/meta-wayland-pointer-constraints.h	\
	wayland/meta-pointer-lock-wayland.c		\
	wayland/meta-pointer-lock-wayland.h		\
	wayland/meta-pointer-confinement-wayland.c	\
	wayland/meta-pointer-confinement-wayland.h	\
	wayland/meta-wayland-popup.c		\
	wayland/meta-wayland-popup.h		\
	wayland/meta-wayland-seat.c		\
	wayland/meta-wayland-seat.h		\
	wayland/meta-wayland-tablet.c		\
	wayland/meta-wayland-tablet.h		\
	wayland/meta-wayland-tablet-manager.c	\
	wayland/meta-wayland-tablet-manager.h	\
	wayland/meta-wayland-tablet-seat.c	\
	wayland/meta-wayland-tablet-seat.h	\
	wayland/meta-wayland-tablet-tool.c	\
	wayland/meta-wayland-tablet-tool.h	\
	wayland/meta-wayland-tablet-pad.c	\
	wayland/meta-wayland-tablet-pad.h	\
	wayland/meta-wayland-tablet-pad-group.c	\
	wayland/meta-wayland-tablet-pad-group.h	\
	wayland/meta-wayland-tablet-pad-ring.c	\
	wayland/meta-wayland-tablet-pad-ring.h	\
	wayland/meta-wayland-tablet-pad-strip.c	\
	wayland/meta-wayland-tablet-pad-strip.h	\
	wayland/meta-wayland-touch.c		\
	wayland/meta-wayland-touch.h		\
	wayland/meta-wayland-surface.c		\
	wayland/meta-wayland-surface.h		\
	wayland/meta-wayland-surface-role-cursor.c	\
	wayland/meta-wayland-surface-role-cursor.h	\
	wayland/meta-wayland-surface-role-tablet-cursor.c	\
	wayland/meta-wayland-surface-role-tablet-cursor.h	\
	wayland/meta-wayland-types.h		\
	wayland/meta-wayland-versions.h		\
	wayland/meta-wayland-outputs.c		\
	wayland/meta-wayland-outputs.h		\
	wayland/meta-wayland-xdg-foreign.c     	\
	wayland/meta-wayland-xdg-foreign.h     	\
	wayland/meta-window-wayland.c		\
	wayland/meta-window-wayland.h		\
	wayland/meta-wayland-xdg-shell.c	\
	wayland/meta-wayland-xdg-shell.h	\
	wayland/meta-wayland-wl-shell.c		\
	wayland/meta-wayland-wl-shell.h		\
	wayland/meta-wayland-gtk-shell.c	\
	wayland/meta-wayland-gtk-shell.h	\
	$(NULL)
endif

if HAVE_NATIVE_BACKEND
libmutter_@LIBMUTTER_API_VERSION@_la_SOURCES +=		\
	backends/native/meta-backend-native.c		\
	backends/native/meta-backend-native.h		\
	backends/native/meta-backend-native-private.h	\
	backends/native/meta-barrier-native.c		\
	backends/native/meta-barrier-native.h		\
	backends/native/meta-clutter-backend-native.c	\
	backends/native/meta-clutter-backend-native.h	\
	backends/native/meta-cursor-renderer-native.c	\
	backends/native/meta-cursor-renderer-native.h	\
	backends/native/meta-default-modes.h		\
	backends/native/meta-idle-monitor-native.c	\
	backends/native/meta-idle-monitor-native.h	\
	backends/native/meta-input-settings-native.c	\
	backends/native/meta-input-settings-native.h	\
	backends/native/meta-monitor-manager-kms.c	\
	backends/native/meta-monitor-manager-kms.h	\
	backends/native/meta-launcher.c			\
	backends/native/meta-launcher.h			\
	backends/native/meta-renderer-native.c		\
	backends/native/meta-renderer-native.h		\
	backends/native/meta-stage-native.c		\
	backends/native/meta-stage-native.h		\
	backends/native/dbus-utils.c			\
	backends/native/dbus-utils.h			\
	$(NULL)
endif

nodist_libmutter_@LIBMUTTER_API_VERSION@_la_SOURCES = $(mutter_built_sources)

libmutter_@LIBMUTTER_API_VERSION@_la_LDFLAGS = \
	-no-undefined -export-symbols-regex "^(meta|ag)_.*"
libmutter_@LIBMUTTER_API_VERSION@_la_LIBADD  = \
	$(MUTTER_LIBS) \
	$(MUTTER_NATIVE_BACKEND_LIBS)	\
	$(top_builddir)/clutter/clutter/libmutter-clutter-$(LIBMUTTER_API_VERSION).la \
	$(NULL)

# Headers installed for plugins; introspected information will
# be extracted into Mutter-<version>.gir
libmutterinclude_headers =			\
	meta/barrier.h				\
	meta/boxes.h				\
	meta/common.h				\
	meta/compositor-mutter.h		\
	meta/compositor.h			\
	meta/display.h				\
	meta/errors.h				\
	meta/group.h				\
	meta/keybindings.h			\
	meta/main.h				\
	meta/meta-backend.h			\
	meta/meta-background.h			\
	meta/meta-background-actor.h		\
	meta/meta-background-image.h		\
	meta/meta-background-group.h		\
	meta/meta-close-dialog.h		\
	meta/meta-cursor-tracker.h		\
	meta/meta-dnd.h			\
	meta/meta-idle-monitor.h		\
	meta/meta-plugin.h			\
	meta/meta-monitor-manager.h		\
	meta/meta-settings.h			\
	meta/meta-shaped-texture.h		\
	meta/meta-shadow-factory.h		\
	meta/meta-window-actor.h		\
	meta/meta-window-shape.h		\
	meta/prefs.h				\
	meta/screen.h				\
	meta/theme.h				\
	meta/types.h				\
	meta/util.h				\
	meta/window.h				\
	meta/workspace.h			\
	$(NULL)

libmutterinclude_built_headers =		\
	meta/meta-version.h			\
	meta/meta-enum-types.h			\
	$(NULL)

libmutterinclude_base_headers =			\
	$(libmutterinclude_headers)		\
	$(libmutterinclude_built_headers)

libmutterincludedir = $(includedir)/mutter/meta

libmutterinclude_HEADERS =			\
	$(libmutterinclude_headers)

nodist_libmutterinclude_HEADERS =		\
	$(libmutterinclude_built_headers)

bin_PROGRAMS=mutter
noinst_PROGRAMS=

mutter_SOURCES = core/mutter.c
mutter_LDADD = $(MUTTER_LIBS) libmutter-@LIBMUTTER_API_VERSION@.la

libexec_PROGRAMS = mutter-restart-helper
mutter_restart_helper_SOURCES = core/restart-helper.c
mutter_restart_helper_LDADD = $(MUTTER_LIBS)

include Makefile-tests.am

if HAVE_INTROSPECTION

include $(INTROSPECTION_MAKEFILE)

# These files are in package-private directories, even though they may be used
# by plugins.  If you're writing a plugin, use g-ir-compiler --add-include-path
# and g-ir-compiler --includedir.
girdir = $(pkglibdir)
gir_DATA = Meta-$(LIBMUTTER_API_VERSION).gir

typelibdir = $(pkglibdir)
typelib_DATA = Meta-$(LIBMUTTER_API_VERSION).typelib

INTROSPECTION_GIRS = Meta-$(LIBMUTTER_API_VERSION).gir
INTROSPECTION_SCANNER_ARGS =					\
	--add-include-path=$(top_builddir)/clutter/clutter	\
	--add-include-path=$(top_builddir)/cogl/cogl		\
	--add-include-path=$(top_builddir)/cogl/cogl-pango	\
	$(NULL)
INTROSPECTION_COMPILER_ARGS =					\
	--includedir=$(top_builddir)/clutter/clutter		\
	--includedir=$(top_builddir)/cogl/cogl			\
	--includedir=$(top_builddir)/cogl/cogl-pango		\
	$(NULL)
INTROSPECTION_SCANNER_ENV = \
	PKG_CONFIG_PATH=$(top_builddir)/clutter/clutter/:$(top_builddir)/cogl/cogl/:$(top_builddir)/cogl/cogl-pango/:$${PKG_CONFIG_PATH}

Meta-@LIBMUTTER_API_VERSION@.gir: libmutter-$(LIBMUTTER_API_VERSION).la
Meta_@LIBMUTTER_API_VERSION@_gir_VERSION = $(LIBMUTTER_API_VERSION)
Meta_@LIBMUTTER_API_VERSION@_gir_INCLUDES =	\
	GObject-2.0				\
	GDesktopEnums-3.0			\
	Gdk-3.0					\
	Gtk-3.0					\
	Cogl-$(LIBMUTTER_API_VERSION)		\
	Clutter-$(LIBMUTTER_API_VERSION)	\
	xlib-2.0				\
	xfixes-4.0				\
	$(NULL)
Meta_@LIBMUTTER_API_VERSION@_gir_EXPORT_PACKAGES = libmutter-$(LIBMUTTER_API_VERSION)
Meta_@LIBMUTTER_API_VERSION@_gir_CFLAGS = $(AM_CPPFLAGS)
Meta_@LIBMUTTER_API_VERSION@_gir_LIBS = libmutter-$(LIBMUTTER_API_VERSION).la
Meta_@LIBMUTTER_API_VERSION@_gir_FILES =			\
	$(libmutterinclude_base_headers)			\
	$(filter %.c,$(libmutter_@LIBMUTTER_API_VERSION@_la_SOURCES) $(nodist_libmutter_@LIBMUTTER_API_VERSION@_la_SOURCES))
Meta_@LIBMUTTER_API_VERSION@_SCANNERFLAGS = --warn-all --warn-error

endif

dbus_idle_built_sources = meta-dbus-idle-monitor.c meta-dbus-idle-monitor.h

CLEANFILES =					\
	$(mutter_built_sources)			\
	$(typelib_DATA)				\
	$(gir_DATA)

DISTCLEANFILES = 				\
	$(libmutterinclude_built_headers)	\
	$(pkgconfig_DATA)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmutter-$(LIBMUTTER_API_VERSION).pc

EXTRA_DIST +=					\
	$(wayland_protocols)			\
	libmutter.pc.in				\
	meta-enum-types.h.in			\
	meta-enum-types.c.in			\
	org.freedesktop.login1.xml		\
	org.gnome.Mutter.DisplayConfig.xml	\
	org.gnome.Mutter.IdleMonitor.xml	\
	backends/native/gen-default-modes.py	\
	$(NULL)

BUILT_SOURCES =					\
	$(mutter_built_sources)			\
	$(libmutterinclude_built_headers)

MUTTER_STAMP_FILES = stamp-meta-enum-types.h
CLEANFILES += $(MUTTER_STAMP_FILES)

meta/meta-enum-types.h: stamp-meta-enum-types.h Makefile
	@true
stamp-meta-enum-types.h: $(libmutterinclude_headers) meta-enum-types.h.in
	$(AM_V_GEN) ( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template meta-enum-types.h.in \
	  $(libmutterinclude_base_headers) ) >> xgen-teth && \
	(cmp -s xgen-teth meta/meta-enum-types.h || cp xgen-teth meta/meta-enum-types.h) && \
	rm -f xgen-teth && \
	echo timestamp > $(@F)

meta-enum-types.c: stamp-meta-enum-types.h meta-enum-types.c.in
	  $(AM_V_GEN) ( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template meta-enum-types.c.in \
	  $(libmutterinclude_base_headers) ) >> xgen-tetc && \
	cp xgen-tetc meta-enum-types.c && \
	rm -f xgen-tetc

dbus_display_config_built_sources = meta-dbus-display-config.c meta-dbus-display-config.h

$(dbus_display_config_built_sources) : Makefile.am org.gnome.Mutter.DisplayConfig.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.gnome.Mutter					\
		--c-namespace MetaDBus							\
		--generate-c-code meta-dbus-display-config				\
		--c-generate-autocleanup all						\
		$(srcdir)/org.gnome.Mutter.DisplayConfig.xml

$(dbus_idle_built_sources) : Makefile.am org.gnome.Mutter.IdleMonitor.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.gnome.Mutter					\
		--c-namespace MetaDBus							\
		--generate-c-code meta-dbus-idle-monitor				\
		--c-generate-object-manager						\
		--c-generate-autocleanup all						\
		$(srcdir)/org.gnome.Mutter.IdleMonitor.xml

dbus_login1_built_sources = meta-dbus-login1.c meta-dbus-login1.h

$(dbus_login1_built_sources) : Makefile.am org.freedesktop.login1.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.freedesktop.login1				\
		--c-namespace Login1							\
		--generate-c-code meta-dbus-login1					\
		--c-generate-autocleanup all						\
		$(srcdir)/org.freedesktop.login1.xml

backends/native/meta-default-modes.h: backends/native/gen-default-modes.py Makefile.am
	@if test -n "$(CVT)"; then \
		if $(AM_V_P); then PS4= set -x; else echo "  GEN      $@"; fi; \
		python $< > $@; \
	fi

.SECONDEXPANSION:

define protostability
$(shell echo $1 | sed 's/.*\(\<unstable\>\|\<stable\>\).*/\1/')
endef

define protoname
$(shell echo $1 | sed 's/\([a-z\-]\+\)-[a-z]\+-v[0-9]\+/\1/')
endef

%-protocol.c : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) code $< $@
%-server-protocol.h : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) server-header $< $@
%-protocol.c : $(srcdir)/wayland/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) code $< $@
%-server-protocol.h : $(srcdir)/wayland/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) server-header $< $@
