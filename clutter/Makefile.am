include $(top_srcdir)/build/autotools/Makefile.am.silent

# preamble
NULL =

# common definitions
CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

lib_LTLIBRARIES =

INCLUDES = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/clutter				\
	-I$(top_srcdir)/clutter/$(CLUTTER_WINSYS)	\
	-I$(top_srcdir)/clutter/$(CLUTTER_WINSYS_BASE)	\
	-I$(top_srcdir)/clutter/cally			\
	-I$(top_srcdir)/clutter/evdev			\
	-I$(top_builddir)				\
	-I$(top_builddir)/clutter			\
	$(NULL)

AM_CPPFLAGS = \
	-DCLUTTER_PREFIX=\""$(prefix)"\"	\
	-DCLUTTER_LIBDIR=\""$(libdir)"\"	\
	-DCLUTTER_DATADIR=\""$(datadir)"\"	\
	-DCLUTTER_LOCALEDIR=\""$(localedir)"\"	\
	-DCLUTTER_COMPILATION=1			\
	-DCOGL_ENABLE_EXPERIMENTAL_API		\
	-DG_LOG_DOMAIN=\"Clutter\" 		\
	$(CLUTTER_DEPRECATED_CFLAGS)		\
	$(CLUTTER_DEBUG_CFLAGS)			\
	$(CLUTTER_PROFILE_CFLAGS)		\
	$(NULL)

AM_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS) $(GCOV_CFLAGS)

# these are the gir files we generate using g-ir-scanner
INTROSPECTION_GIRS =

# the base include path for headers
clutter_base_includedir = $(includedir)/clutter-$(CLUTTER_API_VERSION)
clutter_includedir = $(clutter_base_includedir)/clutter

# pkg-config files
pc_files =

# common sources - please, keep these sorted alphabetically
source_h =					\
	$(srcdir)/clutter-action.h		\
	$(srcdir)/clutter-actor-meta.h		\
	$(srcdir)/clutter-actor.h		\
	$(srcdir)/clutter-align-constraint.h	\
	$(srcdir)/clutter-alpha.h		\
	$(srcdir)/clutter-animatable.h          \
	$(srcdir)/clutter-animation.h           \
	$(srcdir)/clutter-animator.h		\
	$(srcdir)/clutter-backend.h		\
	$(srcdir)/clutter-behaviour.h     	\
	$(srcdir)/clutter-behaviour-depth.h 	\
	$(srcdir)/clutter-behaviour-ellipse.h 	\
	$(srcdir)/clutter-behaviour-opacity.h 	\
	$(srcdir)/clutter-behaviour-path.h 	\
	$(srcdir)/clutter-behaviour-rotate.h 	\
	$(srcdir)/clutter-behaviour-scale.h 	\
	$(srcdir)/clutter-bind-constraint.h	\
	$(srcdir)/clutter-binding-pool.h 	\
	$(srcdir)/clutter-bin-layout.h		\
	$(srcdir)/clutter-blur-effect.h		\
	$(srcdir)/clutter-box.h			\
	$(srcdir)/clutter-box-layout.h		\
	$(srcdir)/clutter-cairo-texture.h	\
	$(srcdir)/clutter-child-meta.h		\
	$(srcdir)/clutter-click-action.h	\
	$(srcdir)/clutter-clone.h		\
	$(srcdir)/clutter-color-static.h	\
	$(srcdir)/clutter-color.h		\
	$(srcdir)/clutter-colorize-effect.h	\
	$(srcdir)/clutter-constraint.h		\
	$(srcdir)/clutter-container.h		\
	$(srcdir)/clutter-deform-effect.h	\
	$(srcdir)/clutter-deprecated.h		\
	$(srcdir)/clutter-desaturate-effect.h	\
	$(srcdir)/clutter-device-manager.h	\
	$(srcdir)/clutter-drag-action.h		\
	$(srcdir)/clutter-drop-action.h		\
	$(srcdir)/clutter-effect.h		\
	$(srcdir)/clutter-event.h 		\
	$(srcdir)/clutter-feature.h 		\
	$(srcdir)/clutter-fixed.h 		\
	$(srcdir)/clutter-fixed-layout.h	\
	$(srcdir)/clutter-flow-layout.h		\
	$(srcdir)/clutter-frame-source.h        \
	$(srcdir)/clutter-gesture-action.h 		\
	$(srcdir)/clutter-group.h 		\
	$(srcdir)/clutter-input-device.h	\
        $(srcdir)/clutter-interval.h            \
	$(srcdir)/clutter-keysyms.h 		\
	$(srcdir)/clutter-keysyms-compat.h	\
	$(srcdir)/clutter-layout-manager.h	\
	$(srcdir)/clutter-layout-meta.h		\
	$(srcdir)/clutter-list-model.h		\
	$(srcdir)/clutter-main.h		\
	$(srcdir)/clutter-media.h		\
	$(srcdir)/clutter-model.h		\
	$(srcdir)/clutter-offscreen-effect.h	\
	$(srcdir)/clutter-page-turn-effect.h	\
	$(srcdir)/clutter-path-constraint.h	\
	$(srcdir)/clutter-path.h		\
	$(srcdir)/clutter-rectangle.h		\
	$(srcdir)/clutter-score.h 		\
	$(srcdir)/clutter-script.h		\
	$(srcdir)/clutter-scriptable.h		\
	$(srcdir)/clutter-settings.h		\
	$(srcdir)/clutter-shader.h		\
	$(srcdir)/clutter-shader-effect.h	\
	$(srcdir)/clutter-shader-types.h	\
	$(srcdir)/clutter-swipe-action.h	\
	$(srcdir)/clutter-snap-constraint.h	\
	$(srcdir)/clutter-stage.h 		\
	$(srcdir)/clutter-stage-manager.h	\
	$(srcdir)/clutter-stage-window.h	\
	$(srcdir)/clutter-state.h		\
	$(srcdir)/clutter-table-layout.h	\
	$(srcdir)/clutter-texture.h 		\
        $(srcdir)/clutter-text.h                \
	$(srcdir)/clutter-timeline.h 		\
	$(srcdir)/clutter-timeout-pool.h 	\
	$(srcdir)/clutter-types.h		\
	$(srcdir)/clutter-units.h 		\
	$(srcdir)/clutter-util.h 		\
	$(NULL)

source_c = \
	$(srcdir)/clutter-action.c		\
	$(srcdir)/clutter-actor-box.c		\
	$(srcdir)/clutter-actor-meta.c		\
	$(srcdir)/clutter-actor.c		\
	$(srcdir)/clutter-align-constraint.c	\
	$(srcdir)/clutter-alpha.c 		\
	$(srcdir)/clutter-animatable.c		\
	$(srcdir)/clutter-animation.c		\
	$(srcdir)/clutter-animator.c		\
	$(srcdir)/clutter-backend.c		\
	$(srcdir)/clutter-behaviour.c 		\
	$(srcdir)/clutter-behaviour-depth.c	\
	$(srcdir)/clutter-behaviour-ellipse.c	\
	$(srcdir)/clutter-behaviour-opacity.c	\
	$(srcdir)/clutter-behaviour-path.c	\
	$(srcdir)/clutter-behaviour-rotate.c	\
	$(srcdir)/clutter-behaviour-scale.c	\
	$(srcdir)/clutter-bezier.c		\
	$(srcdir)/clutter-bind-constraint.c	\
	$(srcdir)/clutter-binding-pool.c	\
	$(srcdir)/clutter-bin-layout.c		\
	$(srcdir)/clutter-blur-effect.c		\
	$(srcdir)/clutter-box.c			\
	$(srcdir)/clutter-box-layout.c		\
	$(srcdir)/clutter-cairo-texture.c       \
	$(srcdir)/clutter-child-meta.c		\
	$(srcdir)/clutter-click-action.c	\
	$(srcdir)/clutter-clone.c		\
	$(srcdir)/clutter-color.c 		\
	$(srcdir)/clutter-colorize-effect.c	\
	$(srcdir)/clutter-constraint.c		\
	$(srcdir)/clutter-container.c		\
	$(srcdir)/clutter-deform-effect.c	\
	$(srcdir)/clutter-desaturate-effect.c	\
	$(srcdir)/clutter-device-manager.c	\
	$(srcdir)/clutter-drag-action.c		\
	$(srcdir)/clutter-drop-action.c		\
	$(srcdir)/clutter-effect.c		\
	$(srcdir)/clutter-event.c 		\
	$(srcdir)/clutter-feature.c 		\
	$(srcdir)/clutter-fixed.c		\
	$(srcdir)/clutter-fixed-layout.c	\
	$(srcdir)/clutter-flatten-effect.c	\
	$(srcdir)/clutter-flow-layout.c		\
	$(srcdir)/clutter-frame-source.c	\
	$(srcdir)/clutter-gesture-action.c 		\
	$(srcdir)/clutter-group.c 		\
	$(srcdir)/clutter-input-device.c	\
	$(srcdir)/clutter-interval.c            \
	$(srcdir)/clutter-keysyms-table.c	\
	$(srcdir)/clutter-layout-manager.c	\
	$(srcdir)/clutter-layout-meta.c		\
	$(srcdir)/clutter-list-model.c		\
	$(srcdir)/clutter-main.c 		\
	$(srcdir)/clutter-master-clock.c	\
	$(srcdir)/clutter-media.c 		\
	$(srcdir)/clutter-model.c		\
	$(srcdir)/clutter-offscreen-effect.c	\
	$(srcdir)/clutter-page-turn-effect.c	\
	$(srcdir)/clutter-path-constraint.c	\
	$(srcdir)/clutter-path.c		\
	$(srcdir)/clutter-rectangle.c 		\
	$(srcdir)/clutter-score.c 		\
	$(srcdir)/clutter-script.c		\
	$(srcdir)/clutter-script-parser.c	\
	$(srcdir)/clutter-scriptable.c		\
	$(srcdir)/clutter-settings.c		\
	$(srcdir)/clutter-shader.c		\
	$(srcdir)/clutter-shader-effect.c	\
	$(srcdir)/clutter-shader-types.c	\
	$(srcdir)/clutter-swipe-action.c	\
	$(srcdir)/clutter-snap-constraint.c	\
	$(srcdir)/clutter-stage.c		\
	$(srcdir)/clutter-stage-manager.c	\
	$(srcdir)/clutter-stage-window.c	\
	$(srcdir)/clutter-state.c		\
	$(srcdir)/clutter-table-layout.c	\
	$(srcdir)/clutter-texture.c 		\
	$(srcdir)/clutter-text.c                \
	$(srcdir)/clutter-timeline.c 		\
	$(srcdir)/clutter-timeout-pool.c	\
	$(srcdir)/clutter-units.c		\
	$(srcdir)/clutter-util.c 		\
	$(srcdir)/clutter-paint-volume.c 	\
	$(NULL)

# private headers; these should not be distributed or introspected
source_h_priv = \
	$(srcdir)/clutter-actor-meta-private.h		\
	$(srcdir)/clutter-actor-private.h		\
	$(srcdir)/clutter-backend-private.h		\
	$(srcdir)/clutter-bezier.h			\
	$(srcdir)/clutter-debug.h 			\
	$(srcdir)/clutter-device-manager-private.h	\
	$(srcdir)/clutter-effect-private.h		\
	$(srcdir)/clutter-event-translator.h		\
	$(srcdir)/clutter-event-private.h		\
	$(srcdir)/clutter-flatten-effect.h		\
	$(srcdir)/clutter-id-pool.h 			\
	$(srcdir)/clutter-master-clock.h		\
	$(srcdir)/clutter-model-private.h		\
	$(srcdir)/clutter-offscreen-effect-private.h	\
	$(srcdir)/clutter-paint-volume-private.h	\
	$(srcdir)/clutter-private.h 			\
	$(srcdir)/clutter-profile.h			\
	$(srcdir)/clutter-script-private.h		\
	$(srcdir)/clutter-stage-manager-private.h	\
	$(srcdir)/clutter-stage-private.h		\
	$(srcdir)/clutter-timeout-interval.h    	\
	$(NULL)

# private source code; these should not be introspected
source_c_priv = \
	$(srcdir)/clutter-event-translator.c	\
	$(srcdir)/clutter-id-pool.c 		\
	$(srcdir)/clutter-profile.c		\
	$(srcdir)/clutter-timeout-interval.c    \
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

# version header
DISTCLEANFILES += clutter-version.h
EXTRA_DIST += clutter-version.h.in

# key symbol update script
EXTRA_DIST += clutter-keysyms-update.pl

pc_files += clutter-$(CLUTTER_API_VERSION).pc

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

# pkg-config file for the X11 meta-backend
clutter-x11-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-x11-$(CLUTTER_API_VERSION).pc

clutterx11_includedir = $(clutter_includedir)/x11
clutterx11_include_HEADERS = $(x11_source_h)
endif # SUPPORT_X11

# Shared cogl backend files
cogl_source_h =

cogl_source_c = \
	$(srcdir)/cogl/clutter-backend-cogl.c	\
	$(srcdir)/cogl/clutter-stage-cogl.c	\
	$(NULL)

cogl_source_h_priv = \
	$(srcdir)/cogl/clutter-backend-cogl.h	\
	$(srcdir)/cogl/clutter-stage-cogl.h	\
	$(NULL)

cogl_source_c_priv =

if SUPPORT_X11
# For compatability with the old GLX backend
#
# Note: there wasn't actually anything GLX specific so we can add
# the compatability if clutter supports x11
backend_source_c += $(srcdir)/cogl/clutter-glx-texture-pixmap.c
glx_source_h = $(srcdir)/cogl/clutter-glx-texture-pixmap.h \
	       $(srcdir)/cogl/clutter-glx.h
clutterglx_includedir = $(clutter_includedir)/glx
clutterglx_include_HEADERS = $(glx_source_h)

clutter-glx-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-glx-$(CLUTTER_API_VERSION).pc
endif

if SUPPORT_GLX
backend_source_h += $(cogl_source_h) $(glx_source_h)
backend_source_c += $(cogl_source_c) $(glx_source_c)
backend_source_h_priv += $(cogl_source_h_priv)
endif # SUPPORT_GLX

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
endif # SUPPORT_WIN32

EXTRA_DIST += \
	$(srcdir)/win32/invisible-cursor.cur 	\
	$(srcdir)/win32/resources.rc		\
	$(NULL)

# EGL backend rules
egl_source_h = \
	$(srcdir)/cogl/clutter-egl-headers.h	\
	$(srcdir)/cogl/clutter-egl.h		\
	$(NULL)

egl_tslib_c = $(srcdir)/cogl/clutter-event-tslib.c

evdev_c_priv = \
	$(srcdir)/evdev/clutter-xkb-utils.c 		\
	$(srcdir)/evdev/clutter-device-manager-evdev.c	\
	$(srcdir)/evdev/clutter-input-device-evdev.c	\
	$(NULL)
evdev_h_priv = \
	$(srcdir)/evdev/clutter-xkb-utils.h 		\
	$(srcdir)/evdev/clutter-device-manager-evdev.h	\
	$(srcdir)/evdev/clutter-input-device-evdev.h	\
	$(NULL)

cex_h = cogl/clutter-cex100.h
BUILT_SOURCES += $(cex_h)
EXTRA_DIST += $(srcdir)/$(cex_h).in

if USE_TSLIB
backend_source_c_priv += $(egl_tslib_c)
endif # SUPPORT_TSLIB

if SUPPORT_EVDEV
backend_source_c_priv += $(evdev_c_priv)
backend_source_h_priv += $(evdev_h_priv)
endif # SUPPORT_EVDEV

if SUPPORT_CEX100
backend_source_h += $(cex_h)
endif # SUPPORT_CEX100

if SUPPORT_EGL
backend_source_h += $(cogl_source_h) $(egl_source_h)
backend_source_c += $(cogl_source_c)
backend_source_h_priv += $(cogl_source_h_priv)
backend_source_c_priv += $(cogl_source_c_priv)

clutteregl_includedir = $(clutter_includedir)/egl
clutteregl_include_HEADERS = $(egl_source_h)
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
endif # SUPPORT_OSX

# Wayland backend rules
if SUPPORT_WAYLAND
backend_source_h += \
	$(srcdir)/wayland/clutter-wayland.h

backend_source_h_priv += \
	$(srcdir)/evdev/clutter-xkb-utils.h 		\
	$(srcdir)/wayland/clutter-backend-wayland.h 	\
	$(srcdir)/wayland/clutter-stage-wayland.h

backend_source_c += \
	$(srcdir)/evdev/clutter-xkb-utils.c 		\
	$(srcdir)/wayland/clutter-backend-wayland.c 	\
	$(srcdir)/wayland/clutter-stage-wayland.c 	\
	$(srcdir)/wayland/clutter-event-wayland.c 	\
	$(srcdir)/wayland/clutter-input-device-wayland.c

clutterwayland_includedir = $(clutter_includedir)/wayland
clutterwayland_include_HEADERS = $(wayland_source_h)
endif # SUPPORT_WAYLAND

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

# backend-specific pkg-config file
clutter-$(CLUTTER_WINSYS)-$(CLUTTER_API_VERSION).pc: clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += clutter-$(CLUTTER_WINSYS)-$(CLUTTER_API_VERSION).pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)
DISTCLEANFILES += $(pc_files)

clutter_include_HEADERS = $(source_h) $(top_srcdir)/clutter/clutter.h
nodist_clutter_include_HEADERS = \
	$(top_builddir)/clutter/clutter-config.h \
	$(top_builddir)/clutter/clutter-version.h \
	$(built_source_h)

lib_LTLIBRARIES += libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la

libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_LIBADD = \
	$(CLUTTER_LIBS) \
	$(CLUTTER_PROFILE_LIBS)

libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_DEPENDENCIES = \
	$(win32_resources)

libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_SOURCES = \
	$(backend_source_c) \
	$(backend_source_h) \
	$(backend_source_c_priv) \
	$(backend_source_h_priv) \
	$(source_c) \
	$(source_h) \
	$(source_c_priv) \
	$(source_h_priv) \
	$(cally_sources_c) \
	$(cally_sources_h) \
	$(cally_sources_private) \
	$(NULL)

nodist_libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_SOURCES = \
	$(backend_source_built) \
	$(built_source_c) \
	$(built_source_h)

libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_LDFLAGS = \
	$(CLUTTER_LINK_FLAGS) \
	$(CLUTTER_LT_LDFLAGS) \
	$(GCOV_LDFLAGS) \
	-export-dynamic \
	-export-symbols-regex "^(clutter|cally).*" \
	-rpath $(libdir) \
	$(win32_resources_ldflag) \
	$(NULL)

# gobject-introspection rules
-include $(INTROSPECTION_MAKEFILE)

if HAVE_INTROSPECTION
Clutter-@CLUTTER_API_VERSION@.gir: libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la Makefile

Clutter_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = Clutter
Clutter_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
Clutter_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la
Clutter_@CLUTTER_API_VERSION_AM@_gir_FILES = $(clutter_include_HEADERS) $(nodist_clutter_include_HEADERS) $(source_c) $(built_source_c)
Clutter_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(INCLUDES) $(CLUTTER_CFLAGS) $(AM_CPPFLAGS) -UCLUTTER_DISABLE_DEPRECATED
Clutter_@CLUTTER_API_VERSION_AM@_gir_INCLUDES = GL-1.0 GObject-2.0 cairo-1.0 Cogl-1.0 CoglPango-1.0 Atk-1.0 Json-1.0
Clutter_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--c-include='clutter/clutter.h' \
	--pkg-export=clutter-@CLUTTER_API_VERSION@

INTROSPECTION_GIRS += Clutter-@CLUTTER_API_VERSION@.gir

Cally-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir

Cally_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = Cally
Cally_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
Cally_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la
Cally_@CLUTTER_API_VERSION_AM@_gir_FILES = $(cally_sources_h) $(cally_sources_c)
Cally_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(INCLUDES) $(CLUTTER_CFLAGS) $(AM_CPPFLAGS) -UCLUTTER_DISABLE_DEPRECATED
Cally_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--identifier-prefix=Cally \
	--symbol-prefix=cally \
	--c-include='cally/cally.h' \
	--pkg-export=cally-@CLUTTER_API_VERSION@ \
	--include-uninstalled=$(top_builddir)/clutter/Clutter-@CLUTTER_API_VERSION@.gir

INTROSPECTION_GIRS += Cally-@CLUTTER_API_VERSION@.gir

if SUPPORT_X11
ClutterX11-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir

ClutterX11_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--identifier-prefix=ClutterX11 \
	--symbol-prefix=clutter_x11 \
	--c-include='clutter/x11/clutter-x11.h' \
	--pkg-export=clutter-x11-@CLUTTER_API_VERSION@ \
	--include-uninstalled=$(top_builddir)/clutter/Clutter-@CLUTTER_API_VERSION@.gir
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_INCLUDES = xlib-2.0
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_FILES = $(x11_introspection)
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(INCLUDES) $(CLUTTER_CFLAGS) $(AM_CPPFLAGS) -UCLUTTER_DISABLE_DEPRECATED

INTROSPECTION_GIRS += ClutterX11-@CLUTTER_API_VERSION@.gir
endif # SUPPORT_X11

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to
# install anything - we need to install inside our prefix.
girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif # HAVE_INTROSPECTION

# GCov rules
gcov_sources = $(source_c) $(backend_source_c)
include $(top_srcdir)/build/autotools/Makefile.am.gcov
