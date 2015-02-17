AUTOMAKE_OPTIONS = subdir-objects

include $(top_srcdir)/build/autotools/Makefile.am.silent

# preamble
NULL =

# common definitions
CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

lib_LTLIBRARIES =

AM_CPPFLAGS = \
	-DCLUTTER_PREFIX=\""$(prefix)"\"	\
	-DCLUTTER_LIBDIR=\""$(libdir)"\"	\
	-DCLUTTER_DATADIR=\""$(datadir)"\"	\
	-DCLUTTER_LOCALEDIR=\""$(localedir)"\"	\
	-DCLUTTER_SYSCONFDIR=\""$(sysconfdir)"\" \
	-DCLUTTER_COMPILATION=1			\
	-DCOGL_ENABLE_EXPERIMENTAL_API		\
	-DCOGL_DISABLE_DEPRECATION_WARNINGS	\
	-DG_LOG_DOMAIN=\"Clutter\" 		\
	-I$(top_srcdir) 			\
	-I$(top_srcdir)/clutter			\
	-I$(top_srcdir)/clutter/cally		\
	-I$(top_builddir)			\
	-I$(top_builddir)/clutter		\
	$(CLUTTER_DEPRECATED_CFLAGS)		\
	$(CLUTTER_DEBUG_CFLAGS)			\
	$(CLUTTER_PROFILE_CFLAGS)		\
	$(CLUTTER_HIDDEN_VISIBILITY_CFLAGS)	\
	$(NULL)

AM_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)

# these are the gir files we generate using g-ir-scanner
INTROSPECTION_GIRS =

# the base include path for headers
clutter_base_includedir = $(includedir)/clutter-$(CLUTTER_API_VERSION)
clutter_includedir = $(clutter_base_includedir)/clutter
clutter_deprecateddir = $(clutter_base_includedir)/clutter/deprecated

# pkg-config files
pc_files =

# common sources - please, keep these sorted alphabetically
source_h =					\
	clutter-action.h		\
	clutter-actor-meta.h		\
	clutter-actor.h		\
	clutter-align-constraint.h	\
	clutter-animatable.h          \
	clutter-backend.h		\
	clutter-bind-constraint.h	\
	clutter-binding-pool.h 	\
	clutter-bin-layout.h		\
	clutter-blur-effect.h		\
	clutter-box-layout.h		\
	clutter-brightness-contrast-effect.h	\
	clutter-cairo.h		\
	clutter-canvas.h		\
	clutter-child-meta.h		\
	clutter-click-action.h	\
	clutter-cogl-compat.h 	\
	clutter-clone.h		\
	clutter-color-static.h	\
	clutter-color.h		\
	clutter-colorize-effect.h	\
	clutter-constraint.h		\
	clutter-container.h		\
	clutter-content.h		\
	clutter-deform-effect.h	\
	clutter-deprecated.h		\
	clutter-desaturate-effect.h	\
	clutter-device-manager.h	\
	clutter-drag-action.h		\
	clutter-drop-action.h		\
	clutter-effect.h		\
	clutter-enums.h		\
	clutter-event.h 		\
	clutter-feature.h 		\
	clutter-fixed-layout.h	\
	clutter-flow-layout.h		\
	clutter-gesture-action.h 	\
	clutter-grid-layout.h 	\
	clutter-group.h 		\
	clutter-image.h		\
	clutter-input-device.h	\
        clutter-interval.h            \
	clutter-keyframe-transition.h	\
	clutter-keysyms.h 		\
	clutter-layout-manager.h	\
	clutter-layout-meta.h		\
	clutter-list-model.h		\
	clutter-macros.h		\
	clutter-main.h		\
	clutter-model.h		\
	clutter-offscreen-effect.h	\
	clutter-page-turn-effect.h	\
	clutter-paint-nodes.h		\
	clutter-paint-node.h		\
	clutter-pan-action.h		\
	clutter-path-constraint.h	\
	clutter-path.h		\
	clutter-property-transition.h	\
	clutter-rotate-action.h	\
	clutter-script.h		\
	clutter-scriptable.h		\
	clutter-scroll-actor.h	\
	clutter-settings.h		\
	clutter-shader-effect.h	\
	clutter-shader-types.h	\
	clutter-swipe-action.h	\
	clutter-snap-constraint.h	\
	clutter-stage.h 		\
	clutter-stage-manager.h	\
	clutter-tap-action.h		\
	clutter-test-utils.h		\
	clutter-texture.h 		\
	clutter-text.h		\
	clutter-text-buffer.h		\
	clutter-timeline.h 		\
	clutter-transition-group.h	\
	clutter-transition.h		\
	clutter-types.h		\
	clutter-units.h 		\
	clutter-zoom-action.h		\
	$(NULL)

source_c = \
	clutter-action.c		\
	clutter-actor-box.c		\
	clutter-actor-meta.c		\
	clutter-actor.c		\
	clutter-align-constraint.c	\
	clutter-animatable.c		\
	clutter-backend.c		\
	clutter-base-types.c		\
	clutter-bezier.c		\
	clutter-bind-constraint.c	\
	clutter-binding-pool.c	\
	clutter-bin-layout.c		\
	clutter-blur-effect.c		\
	clutter-box-layout.c		\
	clutter-brightness-contrast-effect.c	\
	clutter-cairo.c		\
	clutter-canvas.c		\
	clutter-child-meta.c		\
	clutter-click-action.c	\
	clutter-clone.c		\
	clutter-color.c 		\
	clutter-colorize-effect.c	\
	clutter-constraint.c		\
	clutter-container.c		\
	clutter-content.c		\
	clutter-deform-effect.c	\
	clutter-desaturate-effect.c	\
	clutter-device-manager.c	\
	clutter-drag-action.c		\
	clutter-drop-action.c		\
	clutter-effect.c		\
	clutter-event.c 		\
	clutter-feature.c 		\
	clutter-fixed-layout.c	\
	clutter-flatten-effect.c	\
	clutter-flow-layout.c		\
	clutter-gesture-action.c 	\
	clutter-grid-layout.c 	\
	clutter-image.c		\
	clutter-input-device.c	\
	clutter-interval.c            \
	clutter-keyframe-transition.c	\
	clutter-keysyms-table.c	\
	clutter-layout-manager.c	\
	clutter-layout-meta.c		\
	clutter-list-model.c		\
	clutter-main.c 		\
	clutter-master-clock.c	\
	clutter-master-clock-default.c	\
	clutter-model.c		\
	clutter-offscreen-effect.c	\
	clutter-page-turn-effect.c	\
	clutter-paint-nodes.c		\
	clutter-paint-node.c		\
	clutter-pan-action.c		\
	clutter-path-constraint.c	\
	clutter-path.c		\
	clutter-property-transition.c	\
	clutter-rotate-action.c	\
	clutter-script.c		\
	clutter-script-parser.c	\
	clutter-scriptable.c		\
	clutter-scroll-actor.c	\
	clutter-settings.c		\
	clutter-shader-effect.c	\
	clutter-shader-types.c	\
	clutter-swipe-action.c	\
	clutter-snap-constraint.c	\
	clutter-stage.c		\
	clutter-stage-manager.c	\
	clutter-stage-window.c	\
	clutter-tap-action.c		\
	clutter-test-utils.c		\
	clutter-text.c		\
	clutter-text-buffer.c		\
	clutter-transition-group.c	\
	clutter-transition.c		\
	clutter-timeline.c 		\
	clutter-units.c		\
	clutter-util.c 		\
	clutter-paint-volume.c 	\
	clutter-zoom-action.c 	\
	$(NULL)

# private headers; these should not be distributed or introspected
source_h_priv = \
	clutter-actor-meta-private.h		\
	clutter-actor-private.h			\
	clutter-backend-private.h		\
	clutter-bezier.h			\
	clutter-constraint-private.h		\
	clutter-content-private.h		\
	clutter-debug.h 			\
	clutter-device-manager-private.h	\
	clutter-easing.h			\
	clutter-effect-private.h		\
	clutter-event-translator.h		\
	clutter-event-private.h			\
	clutter-flatten-effect.h		\
	clutter-gesture-action-private.h	\
	clutter-id-pool.h 			\
	clutter-master-clock.h			\
	clutter-master-clock-default.h		\
	clutter-model-private.h			\
	clutter-offscreen-effect-private.h	\
	clutter-paint-node-private.h		\
	clutter-paint-volume-private.h		\
	clutter-private.h 			\
	clutter-profile.h			\
	clutter-script-private.h		\
	clutter-settings-private.h		\
	clutter-stage-manager-private.h		\
	clutter-stage-private.h			\
	clutter-stage-window.h			\
	$(NULL)

# private source code; these should not be introspected
source_c_priv = \
	clutter-easing.c		\
	clutter-event-translator.c	\
	clutter-id-pool.c 		\
	clutter-profile.c		\
	$(NULL)

# deprecated installed headers
deprecated_h = \
	deprecated/clutter-actor.h			\
	deprecated/clutter-alpha.h 			\
	deprecated/clutter-animatable.h		\
	deprecated/clutter-animation.h		\
	deprecated/clutter-animator.h			\
	deprecated/clutter-backend.h			\
	deprecated/clutter-behaviour.h 		\
	deprecated/clutter-behaviour-depth.h 		\
	deprecated/clutter-behaviour-ellipse.h	\
	deprecated/clutter-behaviour-opacity.h 	\
	deprecated/clutter-behaviour-path.h 		\
	deprecated/clutter-behaviour-rotate.h 	\
	deprecated/clutter-behaviour-scale.h 		\
	deprecated/clutter-bin-layout.h		\
	deprecated/clutter-box.h			\
	deprecated/clutter-cairo-texture.h		\
	deprecated/clutter-container.h		\
	deprecated/clutter-fixed.h			\
	deprecated/clutter-frame-source.h		\
	deprecated/clutter-group.h			\
	deprecated/clutter-input-device.h		\
	deprecated/clutter-keysyms.h			\
	deprecated/clutter-main.h			\
	deprecated/clutter-media.h			\
	deprecated/clutter-rectangle.h		\
	deprecated/clutter-score.h 			\
	deprecated/clutter-shader.h 			\
	deprecated/clutter-stage-manager.h		\
	deprecated/clutter-stage.h			\
	deprecated/clutter-state.h			\
	deprecated/clutter-table-layout.h		\
	deprecated/clutter-texture.h			\
	deprecated/clutter-timeline.h 		\
	deprecated/clutter-timeout-pool.h 		\
	deprecated/clutter-util.h 			\
	$(NULL)

# deprecated source code
deprecated_c = \
	deprecated/clutter-actor-deprecated.c		\
	deprecated/clutter-alpha.c 			\
	deprecated/clutter-animation.c		\
	deprecated/clutter-animator.c			\
	deprecated/clutter-behaviour.c 		\
	deprecated/clutter-behaviour-depth.c		\
	deprecated/clutter-behaviour-ellipse.c	\
	deprecated/clutter-behaviour-opacity.c	\
	deprecated/clutter-behaviour-path.c		\
	deprecated/clutter-behaviour-rotate.c		\
	deprecated/clutter-behaviour-scale.c		\
	deprecated/clutter-box.c			\
	deprecated/clutter-cairo-texture.c		\
	deprecated/clutter-fixed.c			\
	deprecated/clutter-frame-source.c		\
	deprecated/clutter-group.c 			\
	deprecated/clutter-input-device-deprecated.c	\
	deprecated/clutter-layout-manager-deprecated.c \
	deprecated/clutter-media.c			\
	deprecated/clutter-rectangle.c 		\
	deprecated/clutter-score.c 			\
	deprecated/clutter-shader.c 			\
	deprecated/clutter-state.c			\
	deprecated/clutter-table-layout.c		\
	deprecated/clutter-texture.c 			\
	deprecated/clutter-timeout-pool.c		\
	$(NULL)

# deprecated private headers; these should not be installed
deprecated_h_priv = \
	deprecated/clutter-timeout-interval.h	\
	$(NULL)

# deprecated private source code; these should not be introspected
deprecated_c_priv = \
	deprecated/clutter-timeout-interval.c	\
	$(NULL)

# built sources
built_source_c = \
	clutter-enum-types.c			\
	clutter-marshal.c			\
	$(NULL)

# built headers
built_source_h = \
	clutter-enum-types.h			\
	clutter-marshal.h			\
	$(NULL)

# config header
DISTCLEANFILES += clutter-config.h
EXTRA_DIST += clutter-config.h.in

# win32 config header
EXTRA_DIST += \
	config.h.win32.in	\
	config.h.win32		\
	$(NULL)

# version header
DISTCLEANFILES += clutter-version.h
EXTRA_DIST += clutter-version.h.in clutter-version.h

# key symbol update script
EXTRA_DIST += clutter-keysyms-update.pl

pc_files += clutter-$(CLUTTER_API_VERSION).pc

# in order to be compatible with Clutter < 1.10, when we shipped a single
# shared library whose name was determined by the single backend it
# supported, we need to install symbolic links so that existing applications
# using Clutter won't break in the Brave New World of multi-backend support
# in the same shared object.
compat_libs =

# backends source listings
#
#   backend_source_c := source code
#   backend_source_h := installed public headers
#   backend_source_c_priv := source that should not be scanned by g-i
#   backend_source_h_priv := private headers
#   backend_source_built := built sources
#
backend_source_c =
backend_source_h =
backend_source_c_priv =
backend_source_h_priv =
backend_source_built =

# X11 backend rules
x11_source_c = \
	x11/clutter-backend-x11.c		\
	x11/clutter-device-manager-core-x11.c	\
	x11/clutter-event-x11.c		\
	x11/clutter-input-device-core-x11.c	\
	x11/clutter-keymap-x11.c		\
	x11/clutter-stage-x11.c		\
	x11/clutter-x11-texture-pixmap.c	\
	$(NULL)

x11_source_h = \
	x11/clutter-x11.h			\
	x11/clutter-x11-texture-pixmap.h	\
	$(NULL)

x11_source_h_priv = \
	x11/clutter-backend-x11.h		\
	x11/clutter-device-manager-core-x11.h	\
	x11/clutter-input-device-core-x11.h	\
	x11/clutter-keymap-x11.h		\
	x11/clutter-settings-x11.h		\
	x11/clutter-stage-x11.h		\
	$(NULL)

x11_source_c_priv = \
	x11/xsettings/xsettings-client.c	\
	x11/xsettings/xsettings-client.h	\
	x11/xsettings/xsettings-common.c	\
	x11/xsettings/xsettings-common.h	\
	$(NULL)

if BUILD_XI2
x11_source_c += \
	x11/clutter-device-manager-xi2.c	\
	x11/clutter-input-device-xi2.c	\
	$(NULL)

x11_source_h_priv += \
	x11/clutter-device-manager-xi2.h	\
	x11/clutter-input-device-xi2.h	\
	$(NULL)
endif # BUILD_XI2

if SUPPORT_X11
backend_source_h += $(x11_source_h)
backend_source_c += $(x11_source_c)
backend_source_h_priv += $(x11_source_h_priv)
backend_source_c_priv += $(x11_source_c_priv)

# the list of files we want to introspect on X11
x11_introspection = $(x11_source_c) $(x11_source_h)

clutterx11_includedir = $(clutter_includedir)/x11
clutterx11_include_HEADERS = $(x11_source_h)

clutter-x11-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-x11-$(CLUTTER_API_VERSION).pc
endif # SUPPORT_X11

# Shared cogl backend files
cogl_source_h =

cogl_source_c = \
	cogl/clutter-stage-cogl.c	\
	$(NULL)

cogl_source_h_priv = \
	cogl/clutter-stage-cogl.h	\
	$(NULL)

cogl_source_c_priv =

if USE_COGL
backend_source_h += $(cogl_source_h)
backend_source_c += $(cogl_source_c)
backend_source_h_priv += $(cogl_source_h_priv)
backend_source_c_priv += $(cogl_source_c_priv)

# pkg-config file for the cogl meta-backend
clutter-cogl-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-cogl-$(CLUTTER_API_VERSION).pc
endif

# For compatibility with the old GLX backend
#
# Note: there wasn't actually anything GLX specific so we can add
# the compatibility if clutter supports x11
glx_source_c = x11/clutter-glx-texture-pixmap.c
glx_source_h = x11/clutter-glx-texture-pixmap.h \
	       x11/clutter-glx.h

if SUPPORT_X11
backend_source_h += $(glx_source_h)
backend_source_c += $(glx_source_c)

clutterglx_includedir = $(clutter_includedir)/glx
clutterglx_include_HEADERS = $(glx_source_h)

clutter-glx-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-glx-$(CLUTTER_API_VERSION).pc

compat_libs += \
	libclutter-glx-$(CLUTTER_API_VERSION).so \
	$(NULL)
endif

# GDK backend rules
gdk_source_c = \
	gdk/clutter-backend-gdk.c 		\
	gdk/clutter-device-manager-gdk.c 	\
	gdk/clutter-event-gdk.c 		\
	gdk/clutter-input-device-gdk.c 	\
	gdk/clutter-stage-gdk.c		\
	$(NULL)

gdk_source_h = \
	gdk/clutter-gdk.h			\
	$(NULL)

gdk_source_h_priv = \
	gdk/clutter-settings-gdk.h		\
	gdk/clutter-backend-gdk.h 		\
	gdk/clutter-device-manager-gdk.h 	\
	gdk/clutter-input-device-gdk.h 	\
	gdk/clutter-stage-gdk.h		\
	$(NULL)

if SUPPORT_GDK
backend_source_h += $(gdk_source_h)
backend_source_c += $(gdk_source_c)
backend_source_h_priv += $(gdk_source_h_priv)

cluttergdk_includedir = $(clutter_includedir)/gdk
cluttergdk_include_HEADERS = $(gdk_source_h)

clutter-gdk-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-gdk-$(CLUTTER_API_VERSION).pc

gdk_introspection = $(gdk_source_c) $(gdk_source_h)
endif # SUPPORT_GDK

# Windows backend rules
win32_source_c = \
	win32/clutter-backend-win32.c		\
	win32/clutter-device-manager-win32.c	\
	win32/clutter-event-win32.c		\
	win32/clutter-stage-win32.c		\
	$(NULL)

win32_source_h = \
	win32/clutter-win32.h			\
	$(NULL)

win32_source_h_priv = \
	win32/clutter-backend-win32.h		\
	win32/clutter-device-manager-win32.h	\
	win32/clutter-stage-win32.h		\
	$(NULL)

if SUPPORT_WIN32
# Ideally this resources stuff would go in win32/ but libtool doesn't
# seem to pass on the -Wl argument when linking a convenience library
# so we need to do it here as part of linking the dll. libtool also
# won't let you link against the .o directly because it wants you to
# link against libtool objects for dynamic libraries.
.rc.o :
	mkdir -p win32
	$(WINDRES) -I$(srcdir)/win32 $< $@

win32/resources.o : win32/invisible-cursor.cur

win32_resources = win32/resources.o
win32_resources_ldflag = -Wl,win32/resources.o

backend_source_h += $(win32_source_h)
backend_source_c += $(win32_source_c)
backend_source_h_priv += $(win32_source_h_priv)

clutterwin_includedir = $(clutter_includedir)/win32
clutterwin_include_HEADERS = $(win32_source_h)

clutter-win32-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-win32-$(CLUTTER_API_VERSION).pc
endif # SUPPORT_WIN32

EXTRA_DIST += \
	win32/invisible-cursor.cur 	\
	win32/resources.rc		\
	$(NULL)

egl_tslib_h = tslib/clutter-event-tslib.h
egl_tslib_c = tslib/clutter-event-tslib.c

if USE_TSLIB
backend_source_c_priv += $(egl_tslib_c)
backend_source_h_priv += $(egl_tslib_h)
endif # SUPPORT_TSLIB

evdev_c_priv = \
	evdev/clutter-device-manager-evdev.c	\
	evdev/clutter-input-device-evdev.c	\
	$(NULL)
evdev_h_priv = \
	evdev/clutter-device-manager-evdev.h	\
	evdev/clutter-input-device-evdev.h	\
	$(NULL)
evdev_h = evdev/clutter-evdev.h

if USE_EVDEV
backend_source_c_priv += $(evdev_c_priv)
backend_source_h_priv += $(evdev_h_priv)
backend_source_h += $(evdev_h)

clutterevdev_includedir = $(clutter_includedir)/evdev
clutterevdev_include_HEADERS = $(evdev_h)
endif # SUPPORT_EVDEV

if NEED_XKB_UTILS
backend_source_c += evdev/clutter-xkb-utils.c
backend_source_h_priv += evdev/clutter-xkb-utils.h
endif

cex_source_h_priv = cex100/clutter-backend-cex100.h
cex_source_c = cex100/clutter-backend-cex100.c
cex_h = cex100/clutter-cex100.h
BUILT_SOURCES += $(cex_h)
EXTRA_DIST += $(srcdir)/$(cex_h).in

if SUPPORT_CEX100
backend_source_h += $(cex_h)
backend_source_c += $(cex_source_c)
backend_source_h_priv += $(cex_source_h_priv)

cluttercex100_includedir = $(clutter_includedir)/cex100
cluttercex100_include_HEADERS = $(cex_h)

clutter-cex100-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-cex100-$(CLUTTER_API_VERSION).pc

compat_libs += \
	libclutter-cex100-$(CLUTTER_API_VERSION).so \
	$(NULL)
endif # SUPPORT_CEX100

# EGL backend rules
egl_source_h = \
	egl/clutter-egl-headers.h	\
	egl/clutter-egl.h		\
	$(NULL)

egl_source_h_priv = egl/clutter-backend-eglnative.h egl/clutter-stage-eglnative.h
egl_source_c = egl/clutter-backend-eglnative.c egl/clutter-stage-eglnative.c

# Wayland backend rules
if SUPPORT_WAYLAND
backend_source_h_priv += \
       wayland/clutter-backend-wayland.h             \
       wayland/clutter-backend-wayland-priv.h        \
       wayland/clutter-stage-wayland.h               \
       wayland/clutter-event-wayland.h               \
       wayland/clutter-input-device-wayland.h        \
       wayland/clutter-device-manager-wayland.h

backend_source_c += \
       wayland/clutter-backend-wayland.c             \
       wayland/clutter-stage-wayland.c               \
       wayland/clutter-event-wayland.c               \
       wayland/clutter-input-device-wayland.c        \
       wayland/clutter-device-manager-wayland.c


clutterwayland_includedir = $(clutter_includedir)/wayland
clutterwayland_include_HEADERS = wayland/clutter-wayland.h

clutter-wayland-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-wayland-$(CLUTTER_API_VERSION).pc
endif # SUPPORT_WAYLAND

if SUPPORT_WAYLAND_COMPOSITOR
wayland_compositor_source_h = \
	wayland/clutter-wayland-compositor.h		\
	wayland/clutter-wayland-surface.h
backend_source_h += $(wayland_compositor_source_h)
backend_source_c += \
	wayland/clutter-wayland-surface.c

wayland_compositor_includedir = $(clutter_includedir)/wayland
wayland_compositor_include_HEADERS = $(wayland_compositor_source_h)

clutter-wayland-compositor-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-wayland-compositor-$(CLUTTER_API_VERSION).pc
endif

if SUPPORT_EGL
backend_source_h += $(egl_source_h)
backend_source_c += $(egl_source_c)
backend_source_h_priv += $(egl_source_h_priv)

clutteregl_includedir = $(clutter_includedir)/egl
clutteregl_include_HEADERS = $(egl_source_h)

clutter-egl-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-egl-$(CLUTTER_API_VERSION).pc
endif # SUPPORT_EGL

# OSX backend rules
osx_source_c = \
	osx/clutter-backend-osx.c	\
	osx/clutter-stage-osx.c	\
	$(NULL)

osx_source_h = osx/clutter-osx.h

osx_source_h_priv = \
	osx/clutter-backend-osx.h	\
	osx/clutter-event-loop-osx.h	\
	osx/clutter-stage-osx.h	\
	osx/clutter-device-manager-osx.h \
	$(NULL)

osx_source_c_priv = \
	osx/clutter-event-loop-osx.c	\
	osx/clutter-event-osx.c	\
	osx/clutter-device-manager-osx.c \
	$(NULL)

if SUPPORT_OSX
# we need to tell the compiler that part of our code base is
# in Objective C
AM_CFLAGS += -xobjective-c

backend_source_h += $(osx_source_h)
backend_source_c += $(osx_source_c)
backend_source_h_priv += $(osx_source_h_priv)
backend_source_c_priv += $(osx_source_c_priv)

clutterosx_includedir = $(clutter_includedir)/osx
clutterosx_include_HEADERS = $(osx_source_h)

clutter-osx-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-osx-$(CLUTTER_API_VERSION).pc
endif # SUPPORT_OSX

# cally
cally_sources_h = \
	cally/cally-actor.h		\
	cally/cally-clone.h		\
	cally/cally-factory.h		\
	cally/cally-group.h		\
	cally/cally.h			\
	cally/cally-main.h		\
	cally/cally-rectangle.h	\
	cally/cally-root.h		\
	cally/cally-stage.h		\
	cally/cally-text.h		\
	cally/cally-texture.h		\
	cally/cally-util.h		\
	$(NULL)

cally_sources_c = \
	cally/cally-actor.c		\
	cally/cally.c			\
	cally/cally-clone.c		\
	cally/cally-group.c		\
	cally/cally-rectangle.c	\
	cally/cally-root.c		\
	cally/cally-stage.c		\
	cally/cally-text.c		\
	cally/cally-texture.c		\
	cally/cally-util.c		\
	$(NULL)

cally_sources_private = \
	cally/cally-actor-private.h	\
	$(NULL)

cally_includedir = $(clutter_base_includedir)/cally
cally_include_HEADERS = $(cally_sources_h)

pc_files += cally/cally-$(CLUTTER_API_VERSION).pc
EXTRA_DIST += cally/cally.pc.in

# general build rules:
# you should not need to modify anything below this point

# glib-genmarshal rules
glib_marshal_list = clutter-marshal.list
glib_marshal_prefix = _clutter_marshal
include $(top_srcdir)/build/autotools/Makefile.am.marshal

# glib-mkenums rules
glib_enum_h = clutter-enum-types.h
glib_enum_c = clutter-enum-types.c
glib_enum_headers = $(source_h) $(deprecated_h) $(backend_source_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)
DISTCLEANFILES += $(pc_files)

clutter_include_HEADERS = \
	$(source_h) \
	clutter.h \
	clutter-version.h

nodist_clutter_include_HEADERS = \
	clutter-config.h \
	$(built_source_h)

clutter_deprecated_HEADERS = $(deprecated_h)

lib_LTLIBRARIES += libclutter-@CLUTTER_API_VERSION@.la

libclutter_@CLUTTER_API_VERSION@_la_LIBADD = \
	-lm \
	$(CLUTTER_LIBS) \
	$(CLUTTER_PROFILE_LIBS)

libclutter_@CLUTTER_API_VERSION@_la_DEPENDENCIES = \
	$(win32_resources)

libclutter_@CLUTTER_API_VERSION@_la_SOURCES = \
	$(backend_source_c) \
	$(backend_source_h) \
	$(backend_source_c_priv) \
	$(backend_source_h_priv) \
	$(source_c) \
	$(source_h) \
	$(source_c_priv) \
	$(source_h_priv) \
	$(deprecated_c) \
	$(deprecated_h) \
	$(deprecated_c_priv) \
	$(deprecated_h_priv) \
	$(cally_sources_c) \
	$(cally_sources_h) \
	$(cally_sources_private) \
	$(NULL)

nodist_libclutter_@CLUTTER_API_VERSION@_la_SOURCES = \
	$(backend_source_built) \
	$(built_source_c) \
	$(built_source_h)

libclutter_@CLUTTER_API_VERSION@_la_LDFLAGS = \
	$(CLUTTER_LINK_FLAGS) \
	$(CLUTTER_LT_LDFLAGS) \
	-export-dynamic \
	-rpath $(libdir) \
	$(win32_resources_ldflag) \
	$(NULL)

dist-hook: ../build/win32/vs9/clutter.vcproj ../build/win32/vs10/clutter.vcxproj ../build/win32/vs10/clutter.vcxproj.filters ../build/win32/gen-enums.bat

../build/win32/vs9/clutter.vcproj: $(top_srcdir)/build/win32/vs9/clutter.vcprojin
	for F in `echo $(win32_source_c) $(cally_sources_c) $(source_c) $(source_c_priv) $(deprecated_c) $(deprecated_c_priv) $(built_source_c) | tr '/' '\\'`; do \
		case $$F in \
		*.c)	echo '   <File RelativePath="..\..\..\clutter\'$$F'" />' \
			;; \
		esac; \
	done >clutter.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/clutter.vcprojin >$@
	rm clutter.sourcefiles

../build/win32/vs10/clutter.vcxproj: $(top_srcdir)/build/win32/vs10/clutter.vcxprojin
	for F in `echo $(win32_source_c) $(cally_sources_c) $(source_c) $(source_c_priv) $(deprecated_c) $(deprecated_c_priv) $(built_source_c) | tr '/' '\\'`; do \
		case $$F in \
		*.c)	echo '    <ClCompile Include="..\..\..\clutter\'$$F'" />' \
			;; \
		esac; \
	done >clutter.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/clutter.vcxprojin >$@
	rm clutter.vs10.sourcefiles

../build/win32/vs10/clutter.vcxproj.filters: $(top_srcdir)/build/win32/vs10/clutter.vcxproj.filtersin
	for F in `echo $(win32_source_c) $(cally_sources_c) $(source_c) $(source_c_priv) $(deprecated_c) $(deprecated_c_priv) $(built_source_c) | tr '/' '\\'`; do \
		case $$F in \
		*.c)	echo '    <ClCompile Include="..\..\..\clutter\'$$F'"><Filter>Sources</Filter></ClCompile>' \
			;; \
		esac; \
	done >clutter.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/clutter.vcxproj.filtersin >$@
	rm clutter.vs10.sourcefiles.filters

clutter.vsenums_h:
	echo 'perl %1\bin\glib-mkenums --template ../../clutter/clutter-enum-types.h.in ' >vsenums_h.temp1
	for F in `echo $(source_h) $(backend_source_h) $(srcdir)/win32/clutter-win32.h`; do \
		case $$F in \
		*-x11*.h|*-wayland*.h|*-gdk*.h|*-glx*.h|*-cex*.h|*-egl*.h|*-osx*.h) ;; \
		*.h) echo '../../clutter'$$F' '	\
			;;	\
		esac;	\
	done >>vsenums_h.temp1
	cat vsenums_h.temp1 | sed 's_/clutter./_/clutter/_' >vsenums_h.temp
	cat vsenums_h.temp | tr -d '\n' >>$@
	echo '> ..\..\clutter\clutter-enum-types.h' >>$@
	rm vsenums_h.temp1
	rm vsenums_h.temp

clutter.vsenums_c:
	echo 'perl %1\bin\glib-mkenums --template ../../clutter/clutter-enum-types.c.in ' >vsenums_c.temp1
	for F in `echo $(source_h) $(backend_source_h) $(srcdir)/win32/clutter-win32.h`; do \
		case $$F in \
		*-x11*.h|*-wayland*.h|*-gdk*.h|*-glx*.h|*-cex*.h|*-egl*.h|*-osx*.h) ;; \
		*.h) echo '../../clutter'$$F' '	\
			;;	\
		esac;	\
	done >>vsenums_c.temp1
	cat vsenums_c.temp1 | sed 's_/clutter./_/clutter/_' >vsenums_c.temp
	cat vsenums_c.temp | tr -d '\n' >>$@
	echo '> ..\..\clutter\clutter-enum-types.c' >>$@
	rm vsenums_c.temp1
	rm vsenums_c.temp

../build/win32/gen-enums.bat: clutter.vsenums_h clutter.vsenums_c
	echo 'if exist ..\..\clutter\clutter-enum-types.h goto DONE_COGL_ENUMS_H' >$@
	cat clutter.vsenums_h >>$@
	echo ':DONE_COGL_ENUMS_H' >>$@
	echo 'if exist ..\..\clutter\clutter-enum-types.c goto DONE_COGL_ENUMS_C' >>$@
	cat clutter.vsenums_c >>$@
	echo ':DONE_COGL_ENUMS_C' >>$@
	rm clutter.vsenums_h
	rm clutter.vsenums_c

EXTRA_DIST += \
	clutter-config.h.win32	\
	clutter-config.h.win32_GDK	\
	$(NULL)

# Let the VS9/VS10 Project files be cleared out before they are re-expanded...
DISTCLEANFILES += ../build/win32/vs9/clutter.vcproj ../build/win32/vs10/clutter.vcxproj ../build/win32/vs10/clutter.vcxproj.filters ../build/win32/gen-enums.bat

install-exec-local:
	test -z "$(libdir)" || $(MKDIR_P) "$(DESTDIR)$(libdir)"
	for lib in `echo $(compat_libs)`; do \
	  (cd $(DESTDIR)$(libdir) && \
	    rm -f $$lib.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION); \
	  ) ; \
	  (cd $(DESTDIR)$(libdir) && \
	    { ln -s -f libclutter-$(CLUTTER_API_VERSION).so.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION) $$lib.0 || \
	      { rm -f $$lib.0 && ln -s libclutter-1.0.so.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION) $$lib.0; }; \
	    } \
	  ) ; \
	  (cd $(DESTDIR)$(libdir) && \
	    { ln -s -f libclutter-$(CLUTTER_API_VERSION).so.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION) $$lib || \
	      { rm -f $$lib && ln -s libclutter-1.0.so.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION) $$lib; }; \
	    } \
	  ) ; \
	done

# gobject-introspection rules
-include $(INTROSPECTION_MAKEFILE)

if HAVE_INTROSPECTION
Clutter-@CLUTTER_API_VERSION@.gir: libclutter-@CLUTTER_API_VERSION@.la Makefile

Clutter_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = Clutter
Clutter_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
Clutter_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_API_VERSION@.la
Clutter_@CLUTTER_API_VERSION_AM@_gir_FILES = \
	$(clutter_include_HEADERS) \
	$(clutter_deprecated_HEADERS) \
	$(nodist_clutter_include_HEADERS) \
	$(source_c) \
	$(deprecated_c) \
	$(built_source_c)
Clutter_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(AM_CPPFLAGS) $(CLUTTER_CFLAGS)
Clutter_@CLUTTER_API_VERSION_AM@_gir_INCLUDES = GL-1.0 GObject-2.0 cairo-1.0 Cogl-1.0 CoglPango-1.0 Atk-1.0 Json-1.0
Clutter_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--c-include='clutter/clutter.h' \
	--pkg-export=clutter-@CLUTTER_API_VERSION@

INTROSPECTION_GIRS += Clutter-@CLUTTER_API_VERSION@.gir

Cally-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir

Cally_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = Cally
Cally_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
Cally_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_API_VERSION@.la
Cally_@CLUTTER_API_VERSION_AM@_gir_FILES = $(cally_sources_h) $(cally_sources_c)
Cally_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(AM_CPPFLAGS) $(CLUTTER_CFLAGS)
Cally_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--identifier-prefix=Cally \
	--symbol-prefix=cally \
	--c-include='cally/cally.h' \
	--pkg-export=cally-@CLUTTER_API_VERSION@ \
	--include-uninstalled=$(top_builddir)/clutter/Clutter-@CLUTTER_API_VERSION@.gir

INTROSPECTION_GIRS += Cally-@CLUTTER_API_VERSION@.gir

if SUPPORT_X11
ClutterX11-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir

ClutterX11_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--identifier-prefix=ClutterX11 \
	--symbol-prefix=clutter_x11 \
	--c-include='clutter/x11/clutter-x11.h' \
	--pkg-export=clutter-x11-@CLUTTER_API_VERSION@ \
	--include-uninstalled=$(top_builddir)/clutter/Clutter-@CLUTTER_API_VERSION@.gir
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_INCLUDES = xlib-2.0
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_API_VERSION@.la
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_FILES = $(x11_introspection)
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(AM_CPPFLAGS) $(CLUTTER_CFLAGS)

INTROSPECTION_GIRS += ClutterX11-@CLUTTER_API_VERSION@.gir
endif # SUPPORT_X11

if SUPPORT_GDK
ClutterGdk-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir

ClutterGdk_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--identifier-prefix=ClutterGdk \
	--symbol-prefix=clutter_gdk \
	--c-include='clutter/gdk/clutter-gdk.h' \
	--pkg-export=clutter-gdk-@CLUTTER_API_VERSION@ \
	--include-uninstalled=$(top_builddir)/clutter/Clutter-@CLUTTER_API_VERSION@.gir
ClutterGdk_@CLUTTER_API_VERSION_AM@_gir_INCLUDES = Gdk-3.0
ClutterGdk_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_API_VERSION@.la
ClutterGdk_@CLUTTER_API_VERSION_AM@_gir_FILES = $(gdk_introspection)
ClutterGdk_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(AM_CPPFLAGS) $(CLUTTER_CFLAGS)

INTROSPECTION_GIRS += ClutterGdk-@CLUTTER_API_VERSION@.gir
endif # SUPPORT_GDK

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to
# install anything - we need to install inside our prefix.
girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif # HAVE_INTROSPECTION
