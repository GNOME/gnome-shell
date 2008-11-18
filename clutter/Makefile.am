NULL =

SUBDIRS = cogl pango $(backendextra) $(clutterbackend)

if LOCAL_JSON_GLIB
SUBDIRS += json
clutter_json_include = -I$(top_srcdir)/clutter/json
clutter_json_dep = json/libclutter-json.la
clutter_json_libadd = $(top_builddir)/clutter/json/libclutter-json.la
endif

DIST_SUBDIRS = pango glx eglx eglnative cogl sdl json osx x11 win32 fruity

target = $(clutterbackend)

MARSHALFILES = clutter-marshal.c clutter-marshal.h
ENUMFILES = clutter-enum-types.c clutter-enum-types.h
STAMPFILES = stamp-clutter-marshal.h stamp-clutter-enum-types.h

INCLUDES = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/clutter/pango			\
	-I$(top_srcdir)/clutter	                        \
	$(clutter_json_include)				\
	-I$(top_builddir)/clutter			\
	-DCLUTTER_PREFIX=\""$(prefix)"\"		\
	-DCLUTTER_LIBDIR=\""$(libdir)"\"		\
	-DCLUTTER_DATADIR=\""$(datadir)"\"		\
	-DCLUTTER_COMPILATION=1				\
	-DG_DISABLE_DEPRECATED 				\
	-DG_LOG_DOMAIN=\"Clutter\" 			\
	$(GCC_FLAGS) 					\
	$(CLUTTER_CFLAGS) 				\
	$(CLUTTER_DEBUG_CFLAGS)				\
	$(NULL)

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
	$(srcdir)/clutter-animation.h           \
	$(srcdir)/clutter-backend.h		\
	$(srcdir)/clutter-behaviour.h     	\
	$(srcdir)/clutter-behaviour-bspline.h 	\
	$(srcdir)/clutter-behaviour-depth.h 	\
	$(srcdir)/clutter-behaviour-ellipse.h 	\
	$(srcdir)/clutter-behaviour-opacity.h 	\
	$(srcdir)/clutter-behaviour-path.h 	\
	$(srcdir)/clutter-behaviour-rotate.h 	\
	$(srcdir)/clutter-behaviour-scale.h 	\
	$(srcdir)/clutter-child-meta.h		\
	$(srcdir)/clutter-clone-texture.h 	\
	$(srcdir)/clutter-color.h		\
        $(srcdir)/clutter-container.h		\
        $(srcdir)/clutter-deprecated.h		\
	$(srcdir)/clutter-effect.h 		\
        $(srcdir)/clutter-entry.h               \
	$(srcdir)/clutter-event.h 		\
	$(srcdir)/clutter-feature.h 		\
	$(srcdir)/clutter-fixed.h 		\
	$(srcdir)/clutter-frame-source.h        \
	$(srcdir)/clutter-group.h 		\
        $(srcdir)/clutter-interval.h            \
	$(srcdir)/clutter-keysyms.h 		\
	$(srcdir)/clutter-label.h 		\
	$(srcdir)/clutter-list-model.h		\
	$(srcdir)/clutter-main.h		\
	$(srcdir)/clutter-media.h		\
	$(srcdir)/clutter-model.h		\
	$(srcdir)/clutter-rectangle.h		\
	$(srcdir)/clutter-score.h 		\
	$(srcdir)/clutter-script.h		\
	$(srcdir)/clutter-scriptable.h		\
	$(srcdir)/clutter-shader.h		\
	$(srcdir)/clutter-stage.h 		\
	$(srcdir)/clutter-stage-manager.h	\
	$(srcdir)/clutter-texture.h 		\
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
	$(GLIB_GENMARSHAL) \
		--prefix=clutter_marshal \
		--header \
	  $(srcdir)/clutter-marshal.list > xgen-cmh \
	&& (cmp -s xgen-cmh clutter-marshal.h || cp xgen-cmh clutter-marshal.h ) \
	&& rm -f xgen-cmh \
	&& echo timestamp > $(@F)

clutter-marshal.c: clutter-marshal.h Makefile
	( echo "#include \"clutter-marshal.h\"" ; \
	  $(GLIB_GENMARSHAL) \
	  	--prefix=clutter_marshal \
		--body \
	  $(srcdir)/clutter-marshal.list --body ) >> xgen-cmc \
	&& cp xgen-cmc clutter-marshal.c \
	&& rm -f xgen-cmc

clutter-enum-types.h: stamp-clutter-enum-types.h
	@true
stamp-clutter-enum-types.h: $(source_h) Makefile
	( $(GLIB_MKENUMS) \
	  	--template $(srcdir)/clutter-enum-types.h.in \
	  $(source_h) ) >> xgen-ceth && \
	(cmp xgen-ceth clutter-enum-types.h || cp xgen-ceth clutter-enum-types.h) && \
	rm -f xgen-ceth && \
	echo timestamp > $(@F)

clutter-enum-types.c: clutter-enum-types.h
	( $(GLIB_MKENUMS) \
	  	--template $(srcdir)/clutter-enum-types.c.in \
	  $(source_h) ) >> xgen-cetc && \
	cp xgen-cetc clutter-enum-types.c && \
	rm -f xgen-cetc

CLEANFILES = $(STAMPFILES)

# please, keep this sorted alphabetically
source_c = \
        clutter-actor.c			\
	clutter-alpha.c 		\
	clutter-animation.c             \
	clutter-backend.c		\
        clutter-behaviour.c 		\
	clutter-behaviour-bspline.c	\
	clutter-behaviour-depth.c	\
	clutter-behaviour-ellipse.c	\
	clutter-behaviour-opacity.c	\
	clutter-behaviour-path.c	\
	clutter-behaviour-rotate.c	\
	clutter-behaviour-scale.c	\
	clutter-child-meta.c		\
	clutter-clone-texture.c 	\
	clutter-color.c 		\
	clutter-container.c		\
	clutter-effect.c      		\
        clutter-entry.c                 \
	clutter-enum-types.c		\
        clutter-event.c 		\
	clutter-feature.c 		\
	clutter-fixed.c			\
	clutter-frame-source.c		\
	clutter-group.c 		\
	clutter-id-pool.c 		\
        clutter-interval.c              \
	clutter-label.c 		\
	clutter-list-model.c		\
	clutter-main.c 			\
	clutter-marshal.c		\
	clutter-media.c 		\
	clutter-model.c			\
	clutter-rectangle.c 		\
	clutter-score.c 		\
	clutter-script.c		\
	clutter-script-parser.c		\
	clutter-scriptable.c		\
	clutter-shader.c		\
	clutter-stage.c			\
	clutter-stage-manager.c		\
	clutter-stage-window.c		\
	clutter-texture.c 		\
	clutter-timeline.c 		\
	clutter-timeout-pool.c		\
	clutter-units.c			\
	clutter-util.c 			\
	$(NULL)

source_h_priv = \
	clutter-debug.h 		\
	clutter-keysyms-table.h		\
	clutter-model-private.h		\
	clutter-private.h 		\
	clutter-id-pool.h 		\
	clutter-script-private.h	\
	clutter-stage-window.h		\
	$(NULL)

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_LIBADD = \
  $(CLUTTER_LIBS) \
  $(top_builddir)/clutter/pango/libcoglpango.la \
  $(top_builddir)/clutter/$(CLUTTER_FLAVOUR)/libclutter-$(CLUTTER_FLAVOUR).la \
  $(top_builddir)/clutter/cogl/$(CLUTTER_COGL)/libclutter-cogl.la \
  $(clutter_json_libadd) \
  $(backendextralib)

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_DEPENDENCIES = \
  pango/libcoglpango.la \
  $(CLUTTER_FLAVOUR)/libclutter-$(CLUTTER_FLAVOUR).la \
  cogl/$(CLUTTER_COGL)/libclutter-cogl.la \
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
	clutter-json.h \
	clutter-enum-types.h \
	clutter-version.h \
	clutter.h

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
