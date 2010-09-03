include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = cogl $(CLUTTER_WINSYS_BASE) $(CLUTTER_WINSYS) cally

if LOCAL_JSON_GLIB
SUBDIRS += json
clutter_json_include = -I$(top_srcdir)/clutter/json
clutter_json_dep = json/libclutter-json.la
clutter_json_libadd = $(top_builddir)/clutter/json/libclutter-json.la
clutter_json_gir = ClutterJson-@CLUTTER_API_VERSION@.gir
endif

DIST_SUBDIRS = glx egl cogl json osx x11 win32 fruity cally

# common definitions
CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

if SUPPORT_WIN32

# Ideally this resources stuff would go in win32/ but libtool doesn't
# seem to pass on the -Wl argument when linking a convenience library
# so we need to do it here as part of linking the dll. libtool also
# won't let you link against the .o directly because it wants you to
# link against libtool objects for dynamic libraries.
.rc.o :
	$(WINDRES) -I$(srcdir)/win32 $< $@

win32/resources.o : $(srcdir)/win32/invisible-cursor.cur

win32_resources = win32/resources.o
win32_resources_ldflag = -Wl,win32/resources.o

endif # SUPPORT_WIN32

INCLUDES = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/clutter/cally			\
	-I$(top_srcdir)/clutter/cogl			\
	-I$(top_srcdir)/clutter/cogl/pango		\
	-I$(top_srcdir)/clutter				\
	$(clutter_json_include)				\
	-I$(top_builddir)				\
	-I$(top_builddir)/clutter			\
	-I$(top_builddir)/clutter/cogl			\
	$(NULL)

AM_CPPFLAGS = \
	-DCLUTTER_PREFIX=\""$(prefix)"\"	\
	-DCLUTTER_LIBDIR=\""$(libdir)"\"	\
	-DCLUTTER_DATADIR=\""$(datadir)"\"	\
	-DCLUTTER_COMPILATION=1			\
	-DCOGL_DISABLE_DEPRECATED		\
	-DCOGL_ENABLE_EXPERIMENTAL_API		\
	-DG_DISABLE_DEPRECATED 			\
	-DG_DISABLE_SINGLE_INCLUDES		\
	-DG_LOG_DOMAIN=\"Clutter\" 		\
	$(CLUTTER_DEBUG_CFLAGS)			\
	$(NULL)

AM_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS) $(GCOV_CFLAGS)

# please, keep this sorted alphabetically
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
	$(srcdir)/clutter-color.h		\
	$(srcdir)/clutter-colorize-effect.h	\
	$(srcdir)/clutter-constraint.h		\
	$(srcdir)/clutter-container.h		\
	$(srcdir)/clutter-deform-effect.h	\
	$(srcdir)/clutter-deprecated.h		\
	$(srcdir)/clutter-desaturate-effect.h	\
	$(srcdir)/clutter-device-manager.h	\
	$(srcdir)/clutter-drag-action.h		\
	$(srcdir)/clutter-effect.h		\
	$(srcdir)/clutter-event.h 		\
	$(srcdir)/clutter-feature.h 		\
	$(srcdir)/clutter-fixed.h 		\
	$(srcdir)/clutter-fixed-layout.h	\
	$(srcdir)/clutter-flow-layout.h		\
	$(srcdir)/clutter-frame-source.h        \
	$(srcdir)/clutter-group.h 		\
	$(srcdir)/clutter-input-device.h	\
        $(srcdir)/clutter-interval.h            \
	$(srcdir)/clutter-keysyms.h 		\
	$(srcdir)/clutter-layout-manager.h	\
	$(srcdir)/clutter-layout-meta.h		\
	$(srcdir)/clutter-list-model.h		\
	$(srcdir)/clutter-main.h		\
	$(srcdir)/clutter-media.h		\
	$(srcdir)/clutter-model.h		\
	$(srcdir)/clutter-offscreen-effect.h	\
	$(srcdir)/clutter-page-turn-effect.h	\
	$(srcdir)/clutter-path.h		\
	$(srcdir)/clutter-rectangle.h		\
	$(srcdir)/clutter-score.h 		\
	$(srcdir)/clutter-script.h		\
	$(srcdir)/clutter-scriptable.h		\
	$(srcdir)/clutter-settings.h		\
	$(srcdir)/clutter-shader.h		\
	$(srcdir)/clutter-shader-effect.h	\
	$(srcdir)/clutter-shader-types.h	\
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

built_source_h = \
	clutter-enum-types.h			\
	clutter-marshal.h

# glib-genmarshal rules
glib_marshal_list = clutter-marshal.list
glib_marshal_prefix = _clutter_marshal
include $(top_srcdir)/build/autotools/Makefile.am.marshal

# glib-mkenums rules
glib_enum_h = clutter-enum-types.h
glib_enum_c = clutter-enum-types.c
glib_enum_headers = $(source_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

# please, keep this sorted alphabetically
source_c = \
	$(srcdir)/clutter-action.c		\
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
	$(srcdir)/clutter-effect.c		\
	$(srcdir)/clutter-event.c 		\
	$(srcdir)/clutter-feature.c 		\
	$(srcdir)/clutter-fixed.c		\
	$(srcdir)/clutter-fixed-layout.c	\
	$(srcdir)/clutter-flow-layout.c		\
	$(srcdir)/clutter-frame-source.c	\
	$(srcdir)/clutter-group.c 		\
	$(srcdir)/clutter-input-device.c	\
	$(srcdir)/clutter-interval.c            \
	$(srcdir)/clutter-layout-manager.c	\
	$(srcdir)/clutter-layout-meta.c		\
	$(srcdir)/clutter-list-model.c		\
	$(srcdir)/clutter-main.c 		\
	$(srcdir)/clutter-master-clock.c	\
	$(srcdir)/clutter-media.c 		\
	$(srcdir)/clutter-model.c		\
	$(srcdir)/clutter-offscreen-effect.c	\
	$(srcdir)/clutter-page-turn-effect.c	\
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
	$(NULL)

source_c_priv = \
	$(srcdir)/clutter-id-pool.c 		\
	$(srcdir)/clutter-profile.c		\
	$(srcdir)/clutter-timeout-interval.c    \
	$(NULL)

built_source_c = \
	clutter-enum-types.c			\
	clutter-marshal.c

source_h_priv = \
	$(srcdir)/clutter-actor-meta-private.h	\
	$(srcdir)/clutter-bezier.h		\
	$(srcdir)/clutter-debug.h 		\
	$(srcdir)/clutter-id-pool.h 		\
	$(srcdir)/clutter-keysyms-table.h	\
	$(srcdir)/clutter-master-clock.h	\
	$(srcdir)/clutter-model-private.h	\
	$(srcdir)/clutter-private.h 		\
	$(srcdir)/clutter-profile.h		\
	$(srcdir)/clutter-script-private.h	\
	$(srcdir)/clutter-timeout-interval.h    \
	$(NULL)

libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_LIBADD = \
	$(CLUTTER_LIBS) \
	$(top_builddir)/clutter/cally/libcally.la \
	$(top_builddir)/clutter/cogl/cogl/libclutter-cogl.la \
	$(top_builddir)/clutter/cogl/pango/libcoglpango.la \
	$(top_builddir)/clutter/$(CLUTTER_WINSYS)/libclutter-$(CLUTTER_WINSYS).la \
	$(clutter_json_libadd) \
	$(CLUTTER_WINSYS_BASE_LIB)


libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_DEPENDENCIES = \
	$(top_builddir)/clutter/cally/libcally.la \
	$(top_builddir)/clutter/cogl/cogl/libclutter-cogl.la \
	$(top_builddir)/clutter/cogl/pango/libcoglpango.la \
	$(top_builddir)/clutter/$(CLUTTER_WINSYS)/libclutter-$(CLUTTER_WINSYS).la \
	$(clutter_json_dep) \
	$(CLUTTER_WINSYS_BASE_LIB) \
	$(win32_resources)

libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_SOURCES = \
	$(source_c) \
	$(source_h) \
	$(source_c_priv) \
	$(source_h_priv)

nodist_libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_SOURCES = \
	$(built_source_c) \
	$(built_source_h)

libclutter_@CLUTTER_SONAME_INFIX@_@CLUTTER_API_VERSION@_la_LDFLAGS = \
	$(CLUTTER_LINK_FLAGS) \
	$(CLUTTER_LT_LDFLAGS) \
	$(GCOV_LDFLAGS) \
	-export-dynamic \
	-export-symbols-regex "^(clutter|cogl|cally|json).*" \
	-rpath $(libdir) \
	$(win32_resources_ldflag) \
	$(NULL)

lib_LTLIBRARIES = libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la

clutterdir = $(includedir)/clutter-$(CLUTTER_API_VERSION)/clutter
clutter_HEADERS = \
	$(source_h) \
	$(top_srcdir)/clutter/clutter.h
nodist_clutter_HEADERS = \
	$(top_builddir)/clutter/clutter-json.h \
	$(top_builddir)/clutter/clutter-version.h \
	$(built_source_h)


DISTCLEANFILES += clutter-version.h
EXTRA_DIST += clutter-version.h.in

# these are the gir files we generate
BUILT_GIRSOURCES =

# these are the gir files we scan
INTROSPECTION_GIRS =

if LOCAL_JSON_GLIB
json_gir_include=--include-uninstalled=$(top_builddir)/clutter/json/ClutterJson-@CLUTTER_API_VERSION@.gir

# If we are building it, ClutterJson gets the same handling as described
# for Cogl above
ClutterJson-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir json/ClutterJson-@CLUTTER_API_VERSION@.gir
	$(QUIET_GEN) \
	shlib=`sed -n 's/.*shared-library="\([^"]*\)".*/\1/p' < Clutter-@CLUTTER_API_VERSION@.gir` ; \
	sed "s/shared-library=\"[^\"]*\"/shared-library=\"$$shlib\"/"< json/ClutterJson-@CLUTTER_API_VERSION@.gir > $@

BUILT_GIRSOURCES += ClutterJson-@CLUTTER_API_VERSION@.gir
else
json_gir_include=--include=Json-1.0
endif # LOCAL_JSON_GLIB

-include $(INTROSPECTION_MAKEFILE)

if HAVE_INTROSPECTION
INTROSPECTION_GIRS += Clutter-@CLUTTER_API_VERSION@.gir

Clutter-@CLUTTER_API_VERSION@.gir: libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la Makefile

Clutter_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = Clutter
Clutter_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
Clutter_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la
Clutter_@CLUTTER_API_VERSION_AM@_gir_FILES = $(clutter_HEADERS) $(nodist_clutter_HEADERS) $(source_c) $(built_source_c)
Clutter_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(INCLUDES) $(AM_CPPFLAGS)
Clutter_@CLUTTER_API_VERSION_AM@_gir_INCLUDES = GL-1.0 GObject-2.0 Atk-1.0 Pango-1.0 PangoCairo-1.0
Clutter_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--c-include='clutter/clutter.h' \
	--pkg-export clutter-1.0 \
	$(json_gir_include) \
	--include-uninstalled=$(top_builddir)/clutter/cogl/cogl/Cogl-@CLUTTER_API_VERSION@.gir

# ClutterJson.gir and Cogl.gir are used included into Clutter.gir, so need
# to be built before the typelib is generated
Clutter-@CLUTTER_API_VERSION@.typelib: $(clutter_json_gir) Cogl-@CLUTTER_API_VERSION@.gir

# We build Cogl.gir in the cogl/ subdir, but it needs to reference the shared
# library that it's built into, so we delay compiling the gir into typelib
# until after we've built the shared library. To create the final Cogl.gir
# that we compile and install, we transfer the shared-library="" line from
# Clutter.gir to Cogl.gir
Cogl-@CLUTTER_API_VERSION@.gir: Makefile Clutter-@CLUTTER_API_VERSION@.gir cogl/cogl/Cogl-@CLUTTER_API_VERSION@.gir
	$(QUIET_GEN) \
	shlib=`sed -n 's/.*shared-library="\([^"]*\)".*/\1/p' < Clutter-@CLUTTER_API_VERSION@.gir` ; \
	sed "s/shared-library=\"[^\"]*\"/shared-library=\"$$shlib\"/"< cogl/cogl/Cogl-@CLUTTER_API_VERSION@.gir > $@

BUILT_GIRSOURCES += Cogl-@CLUTTER_API_VERSION@.gir

# Cally depends on Clutter because it exposes Clutter types; for this reason,
# we cannot build Cally.gir under cally/ and then do the shlib trick we do
# for Cogl and ClutterJson.
cally_sources=$(top_srcdir)/clutter/cally/*.h $(top_srcdir)/clutter/cally/*.c
cally_introspection_files=$(filter-out %-private.h, $(cally_sources))

Cally-@CLUTTER_API_VERSION@.gir: Clutter-@CLUTTER_API_VERSION@.gir Makefile

Cally_@CLUTTER_API_VERSION_AM@_gir_NAMESPACE = Cally
Cally_@CLUTTER_API_VERSION_AM@_gir_VERSION = @CLUTTER_API_VERSION@
Cally_@CLUTTER_API_VERSION_AM@_gir_LIBS = libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la
Cally_@CLUTTER_API_VERSION_AM@_gir_FILES = $(cally_introspection_files)
Cally_@CLUTTER_API_VERSION_AM@_gir_CFLAGS = $(INCLUDES) $(AM_CPPFLAGS)
Cally_@CLUTTER_API_VERSION_AM@_gir_SCANNERFLAGS = \
	--warn-all \
	--c-include='cally/cally.h' \
	--add-include-path $(top_builddir)/clutter \
	--include-uninstalled $(top_builddir)/clutter/Clutter-@CLUTTER_API_VERSION@.gir

INTROSPECTION_GIRS += Cally-@CLUTTER_API_VERSION@.gir

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to
# install anything - we need to install inside our prefix.
girdir = $(datadir)/gir-1.0
dist_gir_DATA = $(BUILT_GIRSOURCES) $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0/
typelibs_DATA = $(BUILT_GIRSOURCES:.gir=.typelib) $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelibs_DATA)
endif # HAVE_INTROSPECTION

gcov_sources = $(source_c)
include $(top_srcdir)/build/autotools/Makefile.am.gcov
