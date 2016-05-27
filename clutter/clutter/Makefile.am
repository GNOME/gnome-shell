AUTOMAKE_OPTIONS = subdir-objects

# preamble
NULL =

# common definitions
CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

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
	-I$(top_srcdir)/../cogl			\
	-I$(top_builddir)/../cogl		\
	$(CLUTTER_DEPRECATED_CFLAGS)		\
	$(CLUTTER_DEBUG_CFLAGS)			\
	$(CLUTTER_HIDDEN_VISIBILITY_CFLAGS)	\
	$(NULL)

AM_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)

# these are the gir files we generate using g-ir-scanner
INTROSPECTION_GIRS =

# the base include path for headers
clutter_base_includedir = $(includedir)/mutter/clutter-$(CLUTTER_API_VERSION)
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
	clutter-input-device-tool.h	\
        clutter-interval.h            \
	clutter-keyframe-transition.h	\
	clutter-keysyms.h 		\
	clutter-layout-manager.h	\
	clutter-layout-meta.h		\
	clutter-macros.h		\
	clutter-main.h		\
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
	clutter-stage-view.h		\
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
	clutter-input-device-tool.c	\
	clutter-interval.c            \
	clutter-keyframe-transition.c	\
	clutter-keysyms-table.c	\
	clutter-layout-manager.c	\
	clutter-layout-meta.c		\
	clutter-main.c 		\
	clutter-master-clock.c	\
	clutter-master-clock-default.c	\
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
	clutter-stage-view.c	\
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
	clutter-offscreen-effect-private.h	\
	clutter-paint-node-private.h		\
	clutter-paint-volume-private.h		\
	clutter-private.h 			\
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
	deprecated/clutter-list-model.h			\
	deprecated/clutter-main.h			\
	deprecated/clutter-media.h			\
	deprecated/clutter-model.h			\
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
	deprecated/clutter-list-model.c			\
	deprecated/clutter-media.c			\
	deprecated/clutter-model.c			\
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
	deprecated/clutter-model-private.h	\
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

# version header
DISTCLEANFILES += clutter-version.h
EXTRA_DIST += clutter-version.h.in clutter-version.h

# key symbol update script
EXTRA_DIST += clutter-keysyms-update.pl

pc_files += mutter-clutter-$(CLUTTER_API_VERSION).pc

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

x11_source_c += \
	x11/clutter-device-manager-xi2.c	\
	x11/clutter-input-device-xi2.c	\
	$(NULL)

x11_source_h_priv += \
	x11/clutter-device-manager-xi2.h	\
	x11/clutter-input-device-xi2.h	\
	$(NULL)

backend_source_h += $(x11_source_h)
backend_source_c += $(x11_source_c)
backend_source_h_priv += $(x11_source_h_priv)
backend_source_c_priv += $(x11_source_c_priv)

# the list of files we want to introspect on X11
x11_introspection = $(x11_source_c) $(x11_source_h)

clutterx11_includedir = $(clutter_includedir)/x11
clutterx11_include_HEADERS = $(x11_source_h)

mutter-clutter-x11-$(CLUTTER_API_VERSION).pc: mutter-clutter-$(CLUTTER_API_VERSION).pc
	$(QUIET_GEN)cp -f $< $(@F)

pc_files += mutter-clutter-x11-$(CLUTTER_API_VERSION).pc

# Shared cogl backend files
cogl_source_h =

cogl_source_c = \
	cogl/clutter-stage-cogl.c	\
	$(NULL)

cogl_source_h_priv = \
	cogl/clutter-stage-cogl.h	\
	$(NULL)

cogl_source_c_priv =

backend_source_h += $(cogl_source_h)
backend_source_c += $(cogl_source_c)
backend_source_h_priv += $(cogl_source_h_priv)
backend_source_c_priv += $(cogl_source_c_priv)

backend_source_h += $(glx_source_h)
backend_source_c += $(glx_source_c)

evdev_c_priv = \
	evdev/clutter-device-manager-evdev.c	\
	evdev/clutter-input-device-evdev.c	\
	evdev/clutter-event-evdev.c		\
	evdev/clutter-input-device-tool-evdev.c	\
	$(NULL)
evdev_h_priv = \
	evdev/clutter-device-manager-evdev.h	\
	evdev/clutter-input-device-evdev.h	\
	evdev/clutter-input-device-tool-evdev.h	\
	$(NULL)
evdev_h = evdev/clutter-evdev.h

if SUPPORT_WAYLAND
backend_source_c_priv += $(evdev_c_priv)
backend_source_h_priv += $(evdev_h_priv)
backend_source_h += $(evdev_h)

clutterevdev_includedir = $(clutter_includedir)/evdev
clutterevdev_include_HEADERS = $(evdev_h)

backend_source_c += evdev/clutter-xkb-utils.c
backend_source_h_priv += evdev/clutter-xkb-utils.h

# EGL backend rules
egl_source_h = \
	egl/clutter-egl-headers.h	\
	egl/clutter-egl.h		\
	$(NULL)

egl_source_h_priv = egl/clutter-backend-eglnative.h
egl_source_c = egl/clutter-backend-eglnative.c

wayland_compositor_source_h = \
	wayland/clutter-wayland-compositor.h		\
	wayland/clutter-wayland-surface.h
backend_source_h += $(wayland_compositor_source_h)
backend_source_c += \
	wayland/clutter-wayland-surface.c

wayland_compositor_includedir = $(clutter_includedir)/wayland
wayland_compositor_include_HEADERS = $(wayland_compositor_source_h)

backend_source_h += $(egl_source_h)
backend_source_c += $(egl_source_c)
backend_source_h_priv += $(egl_source_h_priv)

clutteregl_includedir = $(clutter_includedir)/egl
clutteregl_include_HEADERS = $(egl_source_h)
endif # SUPPORT_WAYLAND

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

# general build rules:
# you should not need to modify anything below this point

# glib-genmarshal rules
glib_marshal_list = clutter-marshal.list
glib_marshal_prefix = _clutter_marshal
include $(srcdir)/Makefile.am.marshal

# glib-mkenums rules
glib_enum_h = clutter-enum-types.h
glib_enum_c = clutter-enum-types.c
glib_enum_headers = $(source_h) $(deprecated_h)
include $(srcdir)/Makefile.am.enums

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)
DISTCLEANFILES += $(pc_files)

clutter_include_HEADERS = $(source_h) clutter.h clutter-version.h clutter-autocleanups.h
nodist_clutter_include_HEADERS = clutter-config.h $(built_source_h)

clutter_deprecated_HEADERS = $(deprecated_h)

mutterlibdir = $(libdir)/mutter
mutterlib_LTLIBRARIES = libmutter-clutter-@CLUTTER_API_VERSION@.la

libmutter_clutter_@CLUTTER_API_VERSION@_la_LIBADD = \
	$(LIBM) \
	$(CLUTTER_LIBS) \
	$(top_builddir)/../cogl/cogl/libmutter-cogl.la \
	$(top_builddir)/../cogl/cogl-pango/libmutter-cogl-pango.la \
	$(top_builddir)/../cogl/cogl-path/libmutter-cogl-path.la \
	$(NULL)

libmutter_clutter_@CLUTTER_API_VERSION@_la_SOURCES = \
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

nodist_libmutter_clutter_@CLUTTER_API_VERSION@_la_SOURCES = \
	$(backend_source_built) \
	$(built_source_c) \
	$(built_source_h)

libmutter_clutter_@CLUTTER_API_VERSION@_la_LDFLAGS = \
	$(CLUTTER_LINK_FLAGS) \
	$(CLUTTER_LT_LDFLAGS) \
	-export-dynamic \
	-rpath $(mutterlibdir) \
	$(NULL)

install-exec-local:
	test -z "$(mutterlibdir)" || $(MKDIR_P) "$(DESTDIR)$(mutterlibdir)"
	for lib in `echo $(compat_libs)`; do \
	  (cd $(DESTDIR)$(mutterlibdir) && \
	    rm -f $$lib.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION); \
	  ) ; \
	  (cd $(DESTDIR)$(mutterlibdir) && \
	    { ln -s -f libmutter-clutter-$(CLUTTER_API_VERSION).so.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION) $$lib.0 || \
	      { rm -f $$lib.0 && ln -s libmutter-clutter-1.0.so.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION) $$lib.0; }; \
	    } \
	  ) ; \
	  (cd $(DESTDIR)$(mutterlibdir) && \
	    { ln -s -f libmutter-clutter-$(CLUTTER_API_VERSION).so.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION) $$lib || \
	      { rm -f $$lib && ln -s libmutter-clutter-1.0.so.0.$(CLUTTER_LT_CURRENT).$(CLUTTER_LT_REVISION) $$lib; }; \
	    } \
	  ) ; \
	done

# gobject-introspection rules
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_SCANNER_ARGS = --add-include-path=$(top_builddir)/../cogl/cogl --add-include-path=$(top_builddir)/../cogl/cogl-pango
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_builddir)/../cogl/cogl --includedir=$(top_builddir)/../cogl/cogl-pango

Clutter-@CLUTTER_API_VERSION@.gir: libmutter-clutter-@CLUTTER_API_VERSION@.la Makefile

Clutter_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = Clutter
Clutter_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
Clutter_@CLUTTER_API_VERSION_AM@_gir_LIBS = libmutter-clutter-@CLUTTER_API_VERSION@.la
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
	--pkg-export=mutter-clutter-@CLUTTER_API_VERSION@

INTROSPECTION_GIRS += Clutter-@CLUTTER_API_VERSION@.gir

Cally-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir

Cally_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = Cally
Cally_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
Cally_@CLUTTER_API_VERSION_AM@_gir_LIBS = libmutter-clutter-@CLUTTER_API_VERSION@.la
Cally_@CLUTTER_API_VERSION_AM@_gir_FILES = $(cally_sources_h) $(cally_sources_c)
Cally_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(AM_CPPFLAGS) $(CLUTTER_CFLAGS)
Cally_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--c-include='cally/cally.h' \
	--pkg-export=mutter-cally-@CLUTTER_API_VERSION@ \
	--include-uninstalled=$(top_builddir)/clutter/Clutter-@CLUTTER_API_VERSION@.gir

INTROSPECTION_GIRS += Cally-@CLUTTER_API_VERSION@.gir

ClutterX11-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir

ClutterX11_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = ClutterX11
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_INCLUDES = xlib-2.0
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_LIBS = libmutter-clutter-@CLUTTER_API_VERSION@.la
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_FILES = $(x11_introspection)
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(AM_CPPFLAGS) $(CLUTTER_CFLAGS)
ClutterX11_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--c-include='clutter/x11/clutter-x11.h' \
	--pkg-export=mutter-clutter-x11-@CLUTTER_API_VERSION@ \
	--include-uninstalled=$(top_builddir)/clutter/Clutter-@CLUTTER_API_VERSION@.gir

INTROSPECTION_GIRS += ClutterX11-@CLUTTER_API_VERSION@.gir

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to
# install anything - we need to install inside our prefix.
girdir = $(mutterlibdir)
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(mutterlibdir)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

EXTRA_DIST += \
	Makefile.am.marshal	\
	Makefile.am.enums

CLEANFILES += $(gir_DATA) $(typelib_DATA)
