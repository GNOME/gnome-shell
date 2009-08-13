NULL      =

if USE_SHAVE
QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)
else
QUIET_GEN = $(AM_V_GEN)
endif # USE_SHAVE

SUBDIRS = cogl pango $(backendextra) $(clutterbackend)

if LOCAL_JSON_GLIB
SUBDIRS += json
clutter_json_include = -I$(top_srcdir)/clutter/json
clutter_json_dep = json/libclutter-json.la
clutter_json_libadd = $(top_builddir)/clutter/json/libclutter-json.la
clutter_json_gir = ClutterJson-@CLUTTER_API_VERSION@.gir
endif

DIST_SUBDIRS = pango glx eglx eglnative cogl sdl json osx x11 win32 fruity

target = $(clutterbackend)

MARSHALFILES = clutter-marshal.c clutter-marshal.h
ENUMFILES = clutter-enum-types.c clutter-enum-types.h
STAMPFILES = stamp-clutter-marshal.h stamp-clutter-enum-types.h

INCLUDES = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/clutter/cogl			\
	-I$(top_srcdir)/clutter/pango			\
	-I$(top_srcdir)/clutter	                        \
	$(clutter_json_include)				\
	-I$(top_builddir)/clutter			\
	-DCLUTTER_PREFIX=\""$(prefix)"\"		\
	-DCLUTTER_LIBDIR=\""$(libdir)"\"		\
	-DCLUTTER_DATADIR=\""$(datadir)"\"		\
	-DCLUTTER_COMPILATION=1				\
	-DG_DISABLE_DEPRECATED 				\
	-DG_DISABLE_SINGLE_INCLUDES			\
	-DG_LOG_DOMAIN=\"Clutter\" 			\
	$(CLUTTER_CFLAGS) 				\
	$(NULL)

AM_CFLAGS = $(CLUTTER_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS)

LDADD = \
	$(CLUTTER_LT_LDFLAGS) \
	-export-dynamic \
	-export-symbols-regex "^(clutter|cogl|json|pango).*" \
	-rpath $(libdir) \
	$(NULL)

BUILT_SOURCES = $(MARSHALFILES) $(ENUMFILES)

# please, keep this sorted alphabetically
source_h =					\
	$(srcdir)/clutter-actor.h		\
	$(srcdir)/clutter-alpha.h		\
	$(srcdir)/clutter-animatable.h          \
	$(srcdir)/clutter-animation.h           \
	$(srcdir)/clutter-backend.h		\
	$(srcdir)/clutter-behaviour.h     	\
	$(srcdir)/clutter-behaviour-depth.h 	\
	$(srcdir)/clutter-behaviour-ellipse.h 	\
	$(srcdir)/clutter-behaviour-opacity.h 	\
	$(srcdir)/clutter-behaviour-path.h 	\
	$(srcdir)/clutter-behaviour-rotate.h 	\
	$(srcdir)/clutter-behaviour-scale.h 	\
	$(srcdir)/clutter-binding-pool.h 	\
	$(srcdir)/clutter-cairo-texture.h	\
	$(srcdir)/clutter-child-meta.h		\
	$(srcdir)/clutter-clone.h		\
	$(srcdir)/clutter-color.h		\
        $(srcdir)/clutter-container.h		\
        $(srcdir)/clutter-deprecated.h		\
	$(srcdir)/clutter-event.h 		\
	$(srcdir)/clutter-feature.h 		\
	$(srcdir)/clutter-fixed.h 		\
	$(srcdir)/clutter-frame-source.h        \
	$(srcdir)/clutter-group.h 		\
        $(srcdir)/clutter-interval.h            \
	$(srcdir)/clutter-keysyms.h 		\
	$(srcdir)/clutter-list-model.h		\
	$(srcdir)/clutter-main.h		\
	$(srcdir)/clutter-media.h		\
	$(srcdir)/clutter-model.h		\
	$(srcdir)/clutter-path.h		\
	$(srcdir)/clutter-rectangle.h		\
	$(srcdir)/clutter-score.h 		\
	$(srcdir)/clutter-script.h		\
	$(srcdir)/clutter-scriptable.h		\
	$(srcdir)/clutter-shader.h		\
	$(srcdir)/clutter-shader-types.h	\
	$(srcdir)/clutter-stage.h 		\
	$(srcdir)/clutter-stage-manager.h	\
	$(srcdir)/clutter-stage-window.h	\
	$(srcdir)/clutter-texture.h 		\
        $(srcdir)/clutter-text.h                \
	$(srcdir)/clutter-timeline.h 		\
	$(srcdir)/clutter-timeout-pool.h 	\
	$(srcdir)/clutter-types.h		\
	$(srcdir)/clutter-units.h 		\
	$(srcdir)/clutter-util.h 		\
	$(top_builddir)/clutter/clutter-version.h \
	$(NULL)

clutter-marshal.h: stamp-clutter-marshal.h
	@true
stamp-clutter-marshal.h: clutter-marshal.list
	$(QUIET_GEN)$(GLIB_GENMARSHAL) \
		--prefix=clutter_marshal \
		--header \
	  $(srcdir)/clutter-marshal.list > xgen-cmh \
	&& (cmp -s xgen-cmh clutter-marshal.h || cp xgen-cmh clutter-marshal.h ) \
	&& rm -f xgen-cmh \
	&& echo timestamp > $(@F)

clutter-marshal.c: clutter-marshal.h Makefile
	$(QUIET_GEN)( echo "#include \"clutter-marshal.h\"" ; \
	  $(GLIB_GENMARSHAL) \
	  	--prefix=clutter_marshal \
		--body \
	  $(srcdir)/clutter-marshal.list --body ) >> xgen-cmc \
	&& cp xgen-cmc clutter-marshal.c \
	&& rm -f xgen-cmc

clutter-enum-types.h: stamp-clutter-enum-types.h
	@true
stamp-clutter-enum-types.h: $(source_h) Makefile
	$(QUIET_GEN)( $(GLIB_MKENUMS) \
	  	--template $(srcdir)/clutter-enum-types.h.in \
	  $(source_h) ) >> xgen-ceth && \
	(cmp -s xgen-ceth clutter-enum-types.h || cp xgen-ceth clutter-enum-types.h) && \
	rm -f xgen-ceth && \
	echo timestamp > $(@F)

clutter-enum-types.c: clutter-enum-types.h
	$(QUIET_GEN)( $(GLIB_MKENUMS) \
	  	--template $(srcdir)/clutter-enum-types.c.in \
	  $(source_h) ) >> xgen-cetc && \
	cp xgen-cetc clutter-enum-types.c && \
	rm -f xgen-cetc

CLEANFILES = $(STAMPFILES)

# please, keep this sorted alphabetically
source_c = \
	$(srcdir)/clutter-actor.c		\
	$(srcdir)/clutter-alpha.c 		\
	$(srcdir)/clutter-animatable.c		\
	$(srcdir)/clutter-animation.c		\
	$(srcdir)/clutter-backend.c		\
	$(srcdir)/clutter-behaviour.c 		\
	$(srcdir)/clutter-behaviour-depth.c	\
	$(srcdir)/clutter-behaviour-ellipse.c	\
	$(srcdir)/clutter-behaviour-opacity.c	\
	$(srcdir)/clutter-behaviour-path.c	\
	$(srcdir)/clutter-behaviour-rotate.c	\
	$(srcdir)/clutter-behaviour-scale.c	\
	$(srcdir)/clutter-bezier.c		\
	$(srcdir)/clutter-binding-pool.c	\
	$(srcdir)/clutter-cairo-texture.c       \
	$(srcdir)/clutter-child-meta.c		\
	$(srcdir)/clutter-clone.c		\
	$(srcdir)/clutter-color.c 		\
	$(srcdir)/clutter-container.c		\
	clutter-enum-types.c			\
	$(srcdir)/clutter-event.c 		\
	$(srcdir)/clutter-feature.c 		\
	$(srcdir)/clutter-fixed.c		\
	$(srcdir)/clutter-frame-source.c	\
	$(srcdir)/clutter-group.c 		\
	$(srcdir)/clutter-id-pool.c 		\
	$(srcdir)/clutter-interval.c            \
	$(srcdir)/clutter-list-model.c		\
	$(srcdir)/clutter-main.c 		\
	clutter-marshal.c			\
	$(srcdir)/clutter-master-clock.c	\
	$(srcdir)/clutter-media.c 		\
	$(srcdir)/clutter-model.c		\
	$(srcdir)/clutter-path.c		\
	$(srcdir)/clutter-rectangle.c 		\
	$(srcdir)/clutter-score.c 		\
	$(srcdir)/clutter-script.c		\
	$(srcdir)/clutter-script-parser.c	\
	$(srcdir)/clutter-scriptable.c		\
	$(srcdir)/clutter-shader.c		\
	$(srcdir)/clutter-shader-types.c	\
	$(srcdir)/clutter-stage.c		\
	$(srcdir)/clutter-stage-manager.c	\
	$(srcdir)/clutter-stage-window.c	\
	$(srcdir)/clutter-texture.c 		\
	$(srcdir)/clutter-text.c                \
	$(srcdir)/clutter-timeline.c 		\
	$(srcdir)/clutter-timeout-interval.c    \
	$(srcdir)/clutter-timeout-pool.c	\
	$(srcdir)/clutter-units.c		\
	$(srcdir)/clutter-util.c 		\
	$(NULL)

source_h_priv = \
	$(srcdir)/clutter-bezier.h		\
	$(srcdir)/clutter-debug.h 		\
	$(srcdir)/clutter-keysyms-table.h	\
	$(srcdir)/clutter-master-clock.h	\
	$(srcdir)/clutter-model-private.h	\
	$(srcdir)/clutter-private.h 		\
	$(srcdir)/clutter-id-pool.h 		\
	$(srcdir)/clutter-script-private.h	\
	$(srcdir)/clutter-timeout-interval.h    \
	$(NULL)

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_LIBADD = \
  $(CLUTTER_LIBS) \
  $(top_builddir)/clutter/cogl/libclutter-cogl.la \
  $(top_builddir)/clutter/pango/libcoglpango.la \
  $(top_builddir)/clutter/$(CLUTTER_FLAVOUR)/libclutter-$(CLUTTER_FLAVOUR).la \
  $(clutter_json_libadd) \
  $(backendextralib)

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_DEPENDENCIES = \
  $(top_builddir)/clutter/cogl/libclutter-cogl.la \
  $(top_builddir)/clutter/pango/libcoglpango.la \
  $(top_builddir)/clutter/$(CLUTTER_FLAVOUR)/libclutter-$(CLUTTER_FLAVOUR).la \
  $(clutter_json_dep) \
  $(backendextralib)

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_SOURCES = \
	$(source_c) \
	$(source_h) \
	$(source_h_priv)

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_LDFLAGS = $(LDADD)

lib_LTLIBRARIES = $(clutterbackendlib)

EXTRA_LTLIBRARIES = libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_API_VERSION@.la

clutterdir = $(includedir)/clutter-$(CLUTTER_API_VERSION)/clutter
clutter_HEADERS = \
	$(source_h) \
	$(top_builddir)/clutter/clutter-json.h 		\
	$(top_builddir)/clutter/clutter-enum-types.h 	\
	$(top_srcdir)/clutter/clutter.h

if HAVE_INTROSPECTION
BUILT_GIRSOURCES =

if LOCAL_JSON_GLIB
json_gir_include_path=--add-include-path=json
json_gir_include=--include=ClutterJson-@CLUTTER_API_VERSION@
else
json_gir_include=--include=Json-1.0
endif

# We can't reference the list of COGL header files, since they are in a
# subdir Makefile.am, so just extract them from cogl.h instead. The doc
# comments for COGL are in the headers, so we don't need the source files.
Clutter-@CLUTTER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_API_VERSION@.la
	$(INTROSPECTION_SCANNER) -v \
		--namespace Clutter --nsversion=@CLUTTER_API_VERSION@ \
		$(INCLUDES) \
		--add-include-path=cogl \
		$(json_gir_include_path) \
		--c-include='clutter/clutter.h' \
		--include=GL-1.0 \
		--include=GObject-2.0 \
		--include=Pango-1.0 \
		--include=PangoCairo-1.0 \
		--include=Cogl-@CLUTTER_API_VERSION@ \
		$(json_gir_include) \
		--library=libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_API_VERSION@.la \
		--libtool="$(top_builddir)/libtool" \
	        --pkg gobject-2.0 \
	        --pkg pango \
	        --pkg pangocairo \
	        --output $@ \
		$(clutter_HEADERS) \
		$(source_c)

BUILT_GIRSOURCES += Clutter-@CLUTTER_API_VERSION@.gir

# ClutterJson.gir and Cogl.gir are used included into Clutter.gir, so need
# to be built before the typelib is generated
Clutter-@CLUTTER_API_VERSION@.typelib: $(clutter_json_gir) Cogl-@CLUTTER_API_VERSION@.gir

# We build Cogl.gir in the cogl/ subdir, but it needs to reference the shared
# library that it's built into, so we delay compiling the gir into typelib
# until after we've built the shared library. To create the final Cogl.gir
# that we compile and install, we transfer the shared-library="" line from
# Clutter.gir to Cogl.gir
Cogl-@CLUTTER_API_VERSION@.gir: Clutter-@CLUTTER_API_VERSION@.gir cogl/Cogl-@CLUTTER_API_VERSION@.gir
	shlib=`sed -n 's/.*shared-library="\([^"]*\)".*/\1/p' < Clutter-@CLUTTER_API_VERSION@.gir` ; \
	sed "s/shared-library=\"[^\"]*\"/shared-library=\"$$shlib\"/"< cogl/Cogl-@CLUTTER_API_VERSION@.gir > $@

BUILT_GIRSOURCES += Cogl-@CLUTTER_API_VERSION@.gir

if LOCAL_JSON_GLIB
# If we are building it, ClutterJson gets the same handling as described
# for Cogl above
ClutterJson-@CLUTTER_API_VERSION@.gir: Clutter-@CLUTTER_API_VERSION@.gir json/ClutterJson-@CLUTTER_API_VERSION@.gir
	shlib=`sed -n 's/.*shared-library="\([^"]*\)".*/\1/p' < Clutter-@CLUTTER_API_VERSION@.gir` ; \
	sed "s/shared-library=\"[^\"]*\"/shared-library=\"$$shlib\"/"< json/ClutterJson-@CLUTTER_API_VERSION@.gir > $@

BUILT_GIRSOURCES += ClutterJson-@CLUTTER_API_VERSION@.gir
endif

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to
# install anything - we need to install inside our prefix.
girdir = $(datadir)/gir-1.0
gir_DATA = $(BUILT_GIRSOURCES)

typelibsdir = $(libdir)/girepository-1.0/

typelibs_DATA = $(BUILT_GIRSOURCES:.gir=.typelib)

%.typelib: %.gir $(INTROSPECTION_COMPILER)
	$(QUIET_GEN) \
	LD_LIBRARY_PATH=.libs$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} \
	$(INTROSPECTION_COMPILER) \
		--includedir=$(srcdir) \
		--includedir=. \
		$(INTROSPECTION_COMPILER_OPTS) $< -o $(@F)

CLEANFILES += $(BUILT_GIRSOURCES) $(typelibs_DATA)
endif

DISTCLEANFILES = \
	$(ENUMFILES) \
	$(MARSHALFILES) \
	clutter-version.h \
	$(NULL)

EXTRA_DIST = \
	clutter-marshal.list \
	clutter-version.h.in \
	clutter-enum-types.h.in \
	clutter-enum-types.c.in \
	$(NULL)
