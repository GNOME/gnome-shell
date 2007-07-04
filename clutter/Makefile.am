NULL =

SUBDIRS = cogl pango $(clutterbackend) 

DIST_SUBDIRS = pango glx egl cogl sdl

target = $(clutterbackend)

MARSHALFILES = clutter-marshal.c clutter-marshal.h
ENUMFILES = clutter-enum-types.c clutter-enum-types.h
STAMPFILES = stamp-clutter-marshal.h stamp-clutter-enum-types.h
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
GLIB_MKENUMS=`pkg-config --variable=glib_mkenums glib-2.0`

INCLUDES = \
	-I$(top_srcdir) 			\
	-I$(top_srcdir)/clutter/pango 		\
	-I$(top_srcdir)/clutter/cogl 	        \
	-I$(top_srcdir)/clutter/cogl/@CLUTTER_COGL@ \
	-DPREFIX=\""$(prefix)"\" 		\
	-DLIBDIR=\""$(libdir)"\" 		\
	-DDATADIR=\""$(datadir)"\" 		\
	-DG_DISABLE_DEPRECATED 			\
	-DG_LOG_DOMAIN=\"Clutter\" 		\
	$(GCC_FLAGS) 				\
	$(CLUTTER_CFLAGS) 			\
	$(CLUTTER_DEBUG_CFLAGS)			\
	$(NULL)

LDADD = \
	-version-info $(CLUTTER_LT_VERSION) \
	-export-dynamic \
	-rpath $(libdir) \
	$(NULL)

BUILT_SOURCES = $(MARSHALFILES) $(ENUMFILES)

# please, keep this sorted alphabetically
source_h =					\
	$(srcdir)/clutter-actor.h		\
	$(srcdir)/clutter-alpha.h		\
	$(srcdir)/clutter-backend.h		\
	$(srcdir)/clutter-behaviour.h     	\
	$(srcdir)/clutter-behaviour-bspline.h 	\
	$(srcdir)/clutter-behaviour-ellipse.h 	\
	$(srcdir)/clutter-behaviour-opacity.h 	\
	$(srcdir)/clutter-behaviour-path.h 	\
	$(srcdir)/clutter-behaviour-rotate.h 	\
	$(srcdir)/clutter-behaviour-scale.h 	\
	$(srcdir)/clutter-box.h			\
	$(srcdir)/clutter-clone-texture.h 	\
	$(srcdir)/clutter-color.h		\
        $(srcdir)/clutter-container.h		\
        $(srcdir)/clutter-entry.h               \
	$(srcdir)/clutter-event.h 		\
	$(srcdir)/clutter-feature.h 		\
	$(srcdir)/clutter-fixed.h 		\
	$(srcdir)/clutter-group.h 		\
	$(srcdir)/clutter-hbox.h		\
	$(srcdir)/clutter-keysyms.h 		\
	$(srcdir)/clutter-label.h 		\
        $(srcdir)/clutter-layout.h		\
	$(srcdir)/clutter-main.h		\
	$(srcdir)/clutter-media.h		\
	$(srcdir)/clutter-rectangle.h		\
	$(srcdir)/clutter-stage.h 		\
	$(srcdir)/clutter-texture.h 		\
	$(srcdir)/clutter-timeline.h 		\
	$(srcdir)/clutter-timeout-pool.h 	\
	$(srcdir)/clutter-effect.h 		\
	$(srcdir)/clutter-units.h 		\
	$(srcdir)/clutter-util.h 		\
	$(srcdir)/clutter-vbox.h		\
	$(srcdir)/clutter-version.h		\
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
	$(GLIB_MKENUMS) \
		--fhead "#ifndef __CLUTTER_ENUM_TYPES_H__\n" \
                --fhead "#define __CLUTTER_ENUM_TYPES_H__\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif\n" \
		--fprod "/* --- @filename@ --- */\n" \
		--eprod "#define CLUTTER_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n\n" \
	$(source_h) >> xgen-ceth \
	&& (cmp xgen-ceth clutter-enum-types.h || cp xgen-ceth clutter-enum-types.h) \
	&& rm -f xgen-ceth \
	&& echo timestamp > $(@F)

clutter-enum-types.c: clutter-enum-types.h
	$(GLIB_MKENUMS) \
		--fhead "#include <glib-object.h>\n" \
		--fhead "#include \"clutter-enum-types.h\"\n" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--fprod "\n#include \"@filename@\"" \
		--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
		--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "  { 0, NULL, NULL }\n};\n\n" \
		--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
		--vtail "  static GType type = 0;\n\n" \
		--vtail "  if (!type)\n" \
		--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
		--vtail "  return type;\n}\n\n" \
	$(source_h) >> xgen-cetc \
	&& cp xgen-cetc clutter-enum-types.c \
	&& rm -f xgen-cetc

CLEANFILES = $(STAMPFILES)

# please, keep this sorted alphabetically
source_c = \
        clutter-actor.c			\
	clutter-alpha.c 		\
	clutter-backend.c		\
        clutter-behaviour.c 		\
	clutter-behaviour-bspline.c	\
	clutter-behaviour-ellipse.c	\
	clutter-behaviour-opacity.c	\
	clutter-behaviour-path.c	\
	clutter-behaviour-rotate.c	\
	clutter-behaviour-scale.c	\
	clutter-box.c			\
	clutter-color.c 		\
	clutter-container.c		\
	clutter-clone-texture.c 	\
	clutter-effect.c      		\
        clutter-entry.c                 \
	clutter-enum-types.c		\
        clutter-event.c 		\
	clutter-feature.c 		\
	clutter-fixed.c			\
	clutter-group.c 		\
	clutter-hbox.c			\
	clutter-label.c 		\
	clutter-layout.c		\
	clutter-main.c 			\
	clutter-marshal.c		\
	clutter-media.c 		\
	clutter-stage.c 		\
	clutter-rectangle.c 		\
	clutter-texture.c 		\
	clutter-timeline.c 		\
	clutter-timeout-pool.c		\
	clutter-util.c 			\
	clutter-vbox.c			\
	$(NULL)

source_h_priv = \
	clutter-keysyms-table.h		\
	clutter-debug.h 		\
	clutter-private.h 		\
	$(NULL)


libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_LIBADD =   \
  $(CLUTTER_LIBS) pango/libpangoclutter.la                       \
  @CLUTTER_FLAVOUR@/libclutter-@CLUTTER_FLAVOUR@.la              \
  cogl/@CLUTTER_COGL@/libclutter-cogl.la	  

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_SOURCES =  \
  $(source_c) $(source_h) $(source_h_priv)                      

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_API_VERSION@_la_LDFLAGS = $(LDADD)

lib_LTLIBRARIES = $(clutterbackendlib)

EXTRA_LTLIBRARIES = libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_API_VERSION@.la

clutterdir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter

clutter_HEADERS = \
	$(source_h) \
	clutter-enum-types.h \
	clutter-version.h \
	clutter.h

DISTCLEANFILES = \
	$(ENUMFILES) \
	$(MARSHALFILES) \
	clutter-version.h \
	$(NULL)

EXTRA_DIST = clutter-marshal.list clutter-version.h.in
