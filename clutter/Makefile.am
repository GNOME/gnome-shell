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
	$(srcdir)/clutter-action.h		\
	$(srcdir)/clutter-actor-meta.h		\
	$(srcdir)/clutter-actor.h		\
	$(srcdir)/clutter-align-constraint.h	\
	$(srcdir)/clutter-animatable.h          \
	$(srcdir)/clutter-backend.h		\
	$(srcdir)/clutter-bind-constraint.h	\
	$(srcdir)/clutter-binding-pool.h 	\
	$(srcdir)/clutter-bin-layout.h		\
	$(srcdir)/clutter-blur-effect.h		\
	$(srcdir)/clutter-box-layout.h		\
	$(srcdir)/clutter-brightness-contrast-effect.h	\
	$(srcdir)/clutter-cairo.h		\
	$(srcdir)/clutter-canvas.h		\
	$(srcdir)/clutter-child-meta.h		\
	$(srcdir)/clutter-click-action.h	\
	$(srcdir)/clutter-cogl-compat.h 	\
	$(srcdir)/clutter-clone.h		\
	$(srcdir)/clutter-color-static.h	\
	$(srcdir)/clutter-color.h		\
	$(srcdir)/clutter-colorize-effect.h	\
	$(srcdir)/clutter-constraint.h		\
	$(srcdir)/clutter-container.h		\
	$(srcdir)/clutter-content.h		\
	$(srcdir)/clutter-deform-effect.h	\
	$(srcdir)/clutter-deprecated.h		\
	$(srcdir)/clutter-desaturate-effect.h	\
	$(srcdir)/clutter-device-manager.h	\
	$(srcdir)/clutter-drag-action.h		\
	$(srcdir)/clutter-drop-action.h		\
	$(srcdir)/clutter-effect.h		\
	$(srcdir)/clutter-enums.h		\
	$(srcdir)/clutter-event.h 		\
	$(srcdir)/clutter-feature.h 		\
	$(srcdir)/clutter-fixed-layout.h	\
	$(srcdir)/clutter-flow-layout.h		\
	$(srcdir)/clutter-gesture-action.h 	\
	$(srcdir)/clutter-grid-layout.h 	\
	$(srcdir)/clutter-group.h 		\
	$(srcdir)/clutter-image.h		\
	$(srcdir)/clutter-input-device.h	\
        $(srcdir)/clutter-interval.h            \
	$(srcdir)/clutter-keyframe-transition.h	\
	$(srcdir)/clutter-keysyms.h 		\
	$(srcdir)/clutter-layout-manager.h	\
	$(srcdir)/clutter-layout-meta.h		\
	$(srcdir)/clutter-list-model.h		\
	$(srcdir)/clutter-macros.h		\
	$(srcdir)/clutter-main.h		\
	$(srcdir)/clutter-model.h		\
	$(srcdir)/clutter-offscreen-effect.h	\
	$(srcdir)/clutter-page-turn-effect.h	\
	$(srcdir)/clutter-paint-nodes.h		\
	$(srcdir)/clutter-paint-node.h		\
	$(srcdir)/clutter-pan-action.h		\
	$(srcdir)/clutter-path-constraint.h	\
	$(srcdir)/clutter-path.h		\
	$(srcdir)/clutter-property-transition.h	\
	$(srcdir)/clutter-rotate-action.h	\
	$(srcdir)/clutter-script.h		\
	$(srcdir)/clutter-scriptable.h		\
	$(srcdir)/clutter-scroll-actor.h	\
	$(srcdir)/clutter-settings.h		\
	$(srcdir)/clutter-shader-effect.h	\
	$(srcdir)/clutter-shader-types.h	\
	$(srcdir)/clutter-swipe-action.h	\
	$(srcdir)/clutter-snap-constraint.h	\
	$(srcdir)/clutter-stage.h 		\
	$(srcdir)/clutter-stage-manager.h	\
	$(srcdir)/clutter-tap-action.h		\
	$(srcdir)/clutter-texture.h 		\
	$(srcdir)/clutter-text.h		\
	$(srcdir)/clutter-text-buffer.h		\
	$(srcdir)/clutter-timeline.h 		\
	$(srcdir)/clutter-transition-group.h	\
	$(srcdir)/clutter-transition.h		\
	$(srcdir)/clutter-types.h		\
	$(srcdir)/clutter-units.h 		\
	$(srcdir)/clutter-zoom-action.h		\
	$(NULL)

source_c = \
	$(srcdir)/clutter-action.c		\
	$(srcdir)/clutter-actor-box.c		\
	$(srcdir)/clutter-actor-meta.c		\
	$(srcdir)/clutter-actor.c		\
	$(srcdir)/clutter-align-constraint.c	\
	$(srcdir)/clutter-animatable.c		\
	$(srcdir)/clutter-backend.c		\
	$(srcdir)/clutter-base-types.c		\
	$(srcdir)/clutter-bezier.c		\
	$(srcdir)/clutter-bind-constraint.c	\
	$(srcdir)/clutter-binding-pool.c	\
	$(srcdir)/clutter-bin-layout.c		\
	$(srcdir)/clutter-blur-effect.c		\
	$(srcdir)/clutter-box-layout.c		\
	$(srcdir)/clutter-brightness-contrast-effect.c	\
	$(srcdir)/clutter-cairo.c		\
	$(srcdir)/clutter-canvas.c		\
	$(srcdir)/clutter-child-meta.c		\
	$(srcdir)/clutter-click-action.c	\
	$(srcdir)/clutter-clone.c		\
	$(srcdir)/clutter-color.c 		\
	$(srcdir)/clutter-colorize-effect.c	\
	$(srcdir)/clutter-constraint.c		\
	$(srcdir)/clutter-container.c		\
	$(srcdir)/clutter-content.c		\
	$(srcdir)/clutter-deform-effect.c	\
	$(srcdir)/clutter-desaturate-effect.c	\
	$(srcdir)/clutter-device-manager.c	\
	$(srcdir)/clutter-drag-action.c		\
	$(srcdir)/clutter-drop-action.c		\
	$(srcdir)/clutter-effect.c		\
	$(srcdir)/clutter-event.c 		\
	$(srcdir)/clutter-feature.c 		\
	$(srcdir)/clutter-fixed-layout.c	\
	$(srcdir)/clutter-flatten-effect.c	\
	$(srcdir)/clutter-flow-layout.c		\
	$(srcdir)/clutter-gesture-action.c 	\
	$(srcdir)/clutter-grid-layout.c 	\
	$(srcdir)/clutter-image.c		\
	$(srcdir)/clutter-input-device.c	\
	$(srcdir)/clutter-interval.c            \
	$(srcdir)/clutter-keyframe-transition.c	\
	$(srcdir)/clutter-keysyms-table.c	\
	$(srcdir)/clutter-layout-manager.c	\
	$(srcdir)/clutter-layout-meta.c		\
	$(srcdir)/clutter-list-model.c		\
	$(srcdir)/clutter-main.c 		\
	$(srcdir)/clutter-master-clock.c	\
	$(srcdir)/clutter-model.c		\
	$(srcdir)/clutter-offscreen-effect.c	\
	$(srcdir)/clutter-page-turn-effect.c	\
	$(srcdir)/clutter-paint-nodes.c		\
	$(srcdir)/clutter-paint-node.c		\
	$(srcdir)/clutter-pan-action.c		\
	$(srcdir)/clutter-path-constraint.c	\
	$(srcdir)/clutter-path.c		\
	$(srcdir)/clutter-property-transition.c	\
	$(srcdir)/clutter-rotate-action.c	\
	$(srcdir)/clutter-script.c		\
	$(srcdir)/clutter-script-parser.c	\
	$(srcdir)/clutter-scriptable.c		\
	$(srcdir)/clutter-scroll-actor.c	\
	$(srcdir)/clutter-settings.c		\
	$(srcdir)/clutter-shader-effect.c	\
	$(srcdir)/clutter-shader-types.c	\
	$(srcdir)/clutter-swipe-action.c	\
	$(srcdir)/clutter-snap-constraint.c	\
	$(srcdir)/clutter-stage.c		\
	$(srcdir)/clutter-stage-manager.c	\
	$(srcdir)/clutter-stage-window.c	\
	$(srcdir)/clutter-tap-action.c		\
	$(srcdir)/clutter-text.c		\
	$(srcdir)/clutter-text-buffer.c		\
	$(srcdir)/clutter-transition-group.c	\
	$(srcdir)/clutter-transition.c		\
	$(srcdir)/clutter-timeline.c 		\
	$(srcdir)/clutter-units.c		\
	$(srcdir)/clutter-util.c 		\
	$(srcdir)/clutter-paint-volume.c 	\
	$(srcdir)/clutter-zoom-action.c 	\
	$(NULL)

# private headers; these should not be distributed or introspected
source_h_priv = \
	$(srcdir)/clutter-actor-meta-private.h		\
	$(srcdir)/clutter-actor-private.h		\
	$(srcdir)/clutter-backend-private.h		\
	$(srcdir)/clutter-bezier.h			\
	$(srcdir)/clutter-content-private.h		\
	$(srcdir)/clutter-debug.h 			\
	$(srcdir)/clutter-device-manager-private.h	\
	$(srcdir)/clutter-easing.h			\
	$(srcdir)/clutter-effect-private.h		\
	$(srcdir)/clutter-event-translator.h		\
	$(srcdir)/clutter-event-private.h		\
	$(srcdir)/clutter-flatten-effect.h		\
	$(srcdir)/clutter-gesture-action-private.h	\
	$(srcdir)/clutter-id-pool.h 			\
	$(srcdir)/clutter-master-clock.h		\
	$(srcdir)/clutter-model-private.h		\
	$(srcdir)/clutter-offscreen-effect-private.h	\
	$(srcdir)/clutter-paint-node-private.h		\
	$(srcdir)/clutter-paint-volume-private.h	\
	$(srcdir)/clutter-private.h 			\
	$(srcdir)/clutter-profile.h			\
	$(srcdir)/clutter-script-private.h		\
	$(srcdir)/clutter-settings-private.h		\
	$(srcdir)/clutter-stage-manager-private.h	\
	$(srcdir)/clutter-stage-private.h		\
	$(srcdir)/clutter-stage-window.h		\
	$(NULL)

# private source code; these should not be introspected
source_c_priv = \
	$(srcdir)/clutter-easing.c		\
	$(srcdir)/clutter-event-translator.c	\
	$(srcdir)/clutter-id-pool.c 		\
	$(srcdir)/clutter-profile.c		\
	$(NULL)

# deprecated installed headers
deprecated_h = \
	$(srcdir)/deprecated/clutter-actor.h			\
	$(srcdir)/deprecated/clutter-alpha.h 			\
	$(srcdir)/deprecated/clutter-animatable.h		\
	$(srcdir)/deprecated/clutter-animation.h		\
	$(srcdir)/deprecated/clutter-animator.h			\
	$(srcdir)/deprecated/clutter-backend.h			\
	$(srcdir)/deprecated/clutter-behaviour.h 		\
	$(srcdir)/deprecated/clutter-behaviour-depth.h 		\
	$(srcdir)/deprecated/clutter-behaviour-ellipse.h	\
	$(srcdir)/deprecated/clutter-behaviour-opacity.h 	\
	$(srcdir)/deprecated/clutter-behaviour-path.h 		\
	$(srcdir)/deprecated/clutter-behaviour-rotate.h 	\
	$(srcdir)/deprecated/clutter-behaviour-scale.h 		\
	$(srcdir)/deprecated/clutter-bin-layout.h		\
	$(srcdir)/deprecated/clutter-box.h			\
	$(srcdir)/deprecated/clutter-cairo-texture.h		\
	$(srcdir)/deprecated/clutter-container.h		\
	$(srcdir)/deprecated/clutter-fixed.h			\
	$(srcdir)/deprecated/clutter-frame-source.h		\
	$(srcdir)/deprecated/clutter-group.h			\
	$(srcdir)/deprecated/clutter-input-device.h		\
	$(srcdir)/deprecated/clutter-keysyms.h			\
	$(srcdir)/deprecated/clutter-main.h			\
	$(srcdir)/deprecated/clutter-media.h			\
	$(srcdir)/deprecated/clutter-rectangle.h		\
	$(srcdir)/deprecated/clutter-score.h 			\
	$(srcdir)/deprecated/clutter-shader.h 			\
	$(srcdir)/deprecated/clutter-stage-manager.h		\
	$(srcdir)/deprecated/clutter-stage.h			\
	$(srcdir)/deprecated/clutter-state.h			\
	$(srcdir)/deprecated/clutter-table-layout.h		\
	$(srcdir)/deprecated/clutter-texture.h			\
	$(srcdir)/deprecated/clutter-timeline.h 		\
	$(srcdir)/deprecated/clutter-timeout-pool.h 		\
	$(srcdir)/deprecated/clutter-util.h 			\
	$(NULL)

# deprecated source code
deprecated_c = \
	$(srcdir)/deprecated/clutter-actor-deprecated.c		\
	$(srcdir)/deprecated/clutter-alpha.c 			\
	$(srcdir)/deprecated/clutter-animation.c		\
	$(srcdir)/deprecated/clutter-animator.c			\
	$(srcdir)/deprecated/clutter-behaviour.c 		\
	$(srcdir)/deprecated/clutter-behaviour-depth.c		\
	$(srcdir)/deprecated/clutter-behaviour-ellipse.c	\
	$(srcdir)/deprecated/clutter-behaviour-opacity.c	\
	$(srcdir)/deprecated/clutter-behaviour-path.c		\
	$(srcdir)/deprecated/clutter-behaviour-rotate.c		\
	$(srcdir)/deprecated/clutter-behaviour-scale.c		\
	$(srcdir)/deprecated/clutter-box.c			\
	$(srcdir)/deprecated/clutter-cairo-texture.c		\
	$(srcdir)/deprecated/clutter-fixed.c			\
	$(srcdir)/deprecated/clutter-frame-source.c		\
	$(srcdir)/deprecated/clutter-group.c 			\
	$(srcdir)/deprecated/clutter-input-device-deprecated.c	\
	$(srcdir)/deprecated/clutter-layout-manager-deprecated.c \
	$(srcdir)/deprecated/clutter-media.c			\
	$(srcdir)/deprecated/clutter-rectangle.c 		\
	$(srcdir)/deprecated/clutter-score.c 			\
	$(srcdir)/deprecated/clutter-shader.c 			\
	$(srcdir)/deprecated/clutter-state.c			\
	$(srcdir)/deprecated/clutter-table-layout.c		\
	$(srcdir)/deprecated/clutter-texture.c 			\
	$(srcdir)/deprecated/clutter-timeout-pool.c		\
	$(NULL)

# deprecated private headers; these should not be installed
deprecated_h_priv = \
	$(srcdir)/deprecated/clutter-timeout-interval.h	\
	$(NULL)

# deprecated private source code; these should not be introspected
deprecated_c_priv = \
	$(srcdir)/deprecated/clutter-timeout-interval.c	\
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
	$(srcdir)/x11/clutter-backend-x11.c		\
	$(srcdir)/x11/clutter-device-manager-core-x11.c	\
	$(srcdir)/x11/clutter-event-x11.c		\
	$(srcdir)/x11/clutter-input-device-core-x11.c	\
	$(srcdir)/x11/clutter-keymap-x11.c		\
	$(srcdir)/x11/clutter-stage-x11.c		\
	$(srcdir)/x11/clutter-x11-texture-pixmap.c	\
	$(NULL)

x11_source_h = \
	$(srcdir)/x11/clutter-x11.h			\
	$(srcdir)/x11/clutter-x11-texture-pixmap.h	\
	$(NULL)

x11_source_h_priv = \
	$(srcdir)/x11/clutter-backend-x11.h		\
	$(srcdir)/x11/clutter-device-manager-core-x11.h	\
	$(srcdir)/x11/clutter-input-device-core-x11.h	\
	$(srcdir)/x11/clutter-keymap-x11.h		\
	$(srcdir)/x11/clutter-settings-x11.h		\
	$(srcdir)/x11/clutter-stage-x11.h		\
	$(NULL)

x11_source_c_priv = \
	$(srcdir)/x11/xsettings/xsettings-client.c	\
	$(srcdir)/x11/xsettings/xsettings-client.h	\
	$(srcdir)/x11/xsettings/xsettings-common.c	\
	$(srcdir)/x11/xsettings/xsettings-common.h	\
	$(NULL)

if BUILD_XI2
x11_source_c += \
	$(srcdir)/x11/clutter-device-manager-xi2.c	\
	$(srcdir)/x11/clutter-input-device-xi2.c	\
	$(NULL)

x11_source_h_priv += \
	$(srcdir)/x11/clutter-device-manager-xi2.h	\
	$(srcdir)/x11/clutter-input-device-xi2.h	\
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
	$(srcdir)/cogl/clutter-stage-cogl.c	\
	$(NULL)

cogl_source_h_priv = \
	$(srcdir)/cogl/clutter-stage-cogl.h	\
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
glx_source_c = $(srcdir)/x11/clutter-glx-texture-pixmap.c
glx_source_h = $(srcdir)/x11/clutter-glx-texture-pixmap.h \
	       $(srcdir)/x11/clutter-glx.h

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
	$(srcdir)/gdk/clutter-backend-gdk.c 		\
	$(srcdir)/gdk/clutter-device-manager-gdk.c 	\
	$(srcdir)/gdk/clutter-input-device-gdk.c 	\
	$(srcdir)/gdk/clutter-event-gdk.c 		\
	$(srcdir)/gdk/clutter-stage-gdk.c		\
	$(NULL)

gdk_source_h = \
	$(srcdir)/gdk/clutter-gdk.h			\
	$(NULL)

gdk_source_h_priv = \
	$(srcdir)/gdk/clutter-settings-gdk.h		\
	$(srcdir)/gdk/clutter-backend-gdk.h 		\
	$(srcdir)/gdk/clutter-device-manager-gdk.h 	\
	$(srcdir)/gdk/clutter-input-device-gdk.h 	\
	$(srcdir)/gdk/clutter-stage-gdk.h		\
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
	$(srcdir)/win32/clutter-backend-win32.c		\
	$(srcdir)/win32/clutter-device-manager-win32.c	\
	$(srcdir)/win32/clutter-event-win32.c		\
	$(srcdir)/win32/clutter-stage-win32.c		\
	$(NULL)

win32_source_h = \
	$(srcdir)/win32/clutter-win32.h			\
	$(NULL)

win32_source_h_priv = \
	$(srcdir)/win32/clutter-backend-win32.h		\
	$(srcdir)/win32/clutter-device-manager-win32.h	\
	$(srcdir)/win32/clutter-stage-win32.h		\
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

win32/resources.o : $(srcdir)/win32/invisible-cursor.cur

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
	$(srcdir)/win32/invisible-cursor.cur 	\
	$(srcdir)/win32/resources.rc		\
	$(NULL)

egl_tslib_h = $(srcdir)/tslib/clutter-event-tslib.h
egl_tslib_c = $(srcdir)/tslib/clutter-event-tslib.c

if USE_TSLIB
backend_source_c_priv += $(egl_tslib_c)
backend_source_h_priv += $(egl_tslib_h)
endif # SUPPORT_TSLIB

evdev_c_priv = \
	$(srcdir)/evdev/clutter-device-manager-evdev.c	\
	$(srcdir)/evdev/clutter-input-device-evdev.c	\
	$(NULL)
evdev_h_priv = \
	$(srcdir)/evdev/clutter-device-manager-evdev.h	\
	$(srcdir)/evdev/clutter-input-device-evdev.h	\
	$(NULL)
evdev_h = $(srcdir)/evdev/clutter-evdev.h

if USE_EVDEV
backend_source_c_priv += $(evdev_c_priv)
backend_source_h_priv += $(evdev_h_priv)
backend_source_h += $(evdev_h)

clutterevdev_includedir = $(clutter_includedir)/evdev
clutterevdev_include_HEADERS = $(evdev_h)
endif # SUPPORT_EVDEV

if NEED_XKB_UTILS
backend_source_c += $(srcdir)/evdev/clutter-xkb-utils.c
backend_source_h_priv += $(srcdir)/evdev/clutter-xkb-utils.h
endif

cex_source_h_priv = $(srcdir)/cex100/clutter-backend-cex100.h
cex_source_c = $(srcdir)/cex100/clutter-backend-cex100.c
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
	$(srcdir)/egl/clutter-egl-headers.h	\
	$(srcdir)/egl/clutter-egl.h		\
	$(NULL)

egl_source_h_priv = $(srcdir)/egl/clutter-backend-eglnative.h
egl_source_c = $(srcdir)/egl/clutter-backend-eglnative.c

# Wayland backend rules
if SUPPORT_WAYLAND
backend_source_h_priv += \
       $(srcdir)/wayland/clutter-backend-wayland.h             \
       $(srcdir)/wayland/clutter-backend-wayland-priv.h        \
       $(srcdir)/wayland/clutter-stage-wayland.h               \
       $(srcdir)/wayland/clutter-event-wayland.h               \
       $(srcdir)/wayland/clutter-input-device-wayland.h        \
       $(srcdir)/wayland/clutter-device-manager-wayland.h

backend_source_c += \
       $(srcdir)/wayland/clutter-backend-wayland.c             \
       $(srcdir)/wayland/clutter-stage-wayland.c               \
       $(srcdir)/wayland/clutter-event-wayland.c               \
       $(srcdir)/wayland/clutter-input-device-wayland.c        \
       $(srcdir)/wayland/clutter-device-manager-wayland.c


clutterwayland_includedir = $(clutter_includedir)/wayland
clutterwayland_include_HEADERS = $(srcdir)/wayland/clutter-wayland.h

clutter-wayland-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-wayland-$(CLUTTER_API_VERSION).pc
endif # SUPPORT_WAYLAND

if SUPPORT_WAYLAND_COMPOSITOR
wayland_compositor_source_h = \
	$(srcdir)/wayland/clutter-wayland-compositor.h		\
	$(srcdir)/wayland/clutter-wayland-surface.h
backend_source_h += $(wayland_compositor_source_h)
backend_source_c += \
	$(srcdir)/wayland/clutter-wayland-surface.c

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
	$(srcdir)/osx/clutter-backend-osx.c	\
	$(srcdir)/osx/clutter-stage-osx.c	\
	$(NULL)

osx_source_h = $(srcdir)/osx/clutter-osx.h

osx_source_h_priv = \
	$(srcdir)/osx/clutter-backend-osx.h	\
	$(srcdir)/osx/clutter-event-loop-osx.h	\
	$(srcdir)/osx/clutter-stage-osx.h	\
	$(srcdir)/osx/clutter-device-manager-osx.h \
	$(NULL)

osx_source_c_priv = \
	$(srcdir)/osx/clutter-event-loop-osx.c	\
	$(srcdir)/osx/clutter-event-osx.c	\
	$(srcdir)/osx/clutter-device-manager-osx.c \
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
	$(srcdir)/cally/cally-actor.h		\
	$(srcdir)/cally/cally-clone.h		\
	$(srcdir)/cally/cally-factory.h		\
	$(srcdir)/cally/cally-group.h		\
	$(srcdir)/cally/cally.h			\
	$(srcdir)/cally/cally-main.h		\
	$(srcdir)/cally/cally-rectangle.h	\
	$(srcdir)/cally/cally-root.h		\
	$(srcdir)/cally/cally-stage.h		\
	$(srcdir)/cally/cally-text.h		\
	$(srcdir)/cally/cally-texture.h		\
	$(srcdir)/cally/cally-util.h		\
	$(NULL)

cally_sources_c = \
	$(srcdir)/cally/cally-actor.c		\
	$(srcdir)/cally/cally.c			\
	$(srcdir)/cally/cally-clone.c		\
	$(srcdir)/cally/cally-group.c		\
	$(srcdir)/cally/cally-rectangle.c	\
	$(srcdir)/cally/cally-root.c		\
	$(srcdir)/cally/cally-stage.c		\
	$(srcdir)/cally/cally-text.c		\
	$(srcdir)/cally/cally-texture.c		\
	$(srcdir)/cally/cally-util.c		\
	$(NULL)

cally_sources_private = \
	$(srcdir)/cally/cally-actor-private.h	\
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
glib_enum_headers = $(source_h) $(backend_source_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)
DISTCLEANFILES += $(pc_files)

clutter_include_HEADERS = \
	$(source_h) \
	$(top_srcdir)/clutter/clutter.h \
	$(top_builddir)/clutter/clutter-version.h

nodist_clutter_include_HEADERS = \
	$(top_builddir)/clutter/clutter-config.h \
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
	-export-symbols-regex "^(clutter|cally).*" \
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
	clutter.symbols	\
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

# Test
clutter_all_c_sources = \
	$(backend_source_c) \
	$(backend_source_c_priv) \
	$(source_c) \
	$(source_c_priv) \
	$(deprecated_c) \
	$(deprecated_c_priv) \
	$(cally_sources_c) \
	$(built_source_c)

TESTS_ENVIRONMENT = srcdir="$(srcdir)" CLUTTER_BACKENDS="$(CLUTTER_BACKENDS)"
if OS_LINUX
TESTS = abicheck.sh
endif

EXTRA_DIST += abicheck.sh
