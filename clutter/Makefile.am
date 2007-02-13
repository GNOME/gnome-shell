SUBDIRS=pango

MARSHALFILES = clutter-marshal.c clutter-marshal.h
ENUMFILES = clutter-enum-types.c clutter-enum-types.h
STAMPFILES = stamp-clutter-marshal.h stamp-clutter-enum-types.h
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
GLIB_MKENUMS=`pkg-config --variable=glib_mkenums glib-2.0`

BUILT_SOURCES = $(MARSHALFILES) $(ENUMFILES)

source_h =					\
	$(srcdir)/clutter-keysyms.h 		\
	$(srcdir)/clutter-util.h 		\
	$(srcdir)/clutter-fixed.h 		\
        $(srcdir)/clutter-event.h 		\
	$(srcdir)/clutter-color.h		\
	$(srcdir)/clutter-feature.h 		\
	$(srcdir)/clutter-timeline.h 		\
	$(srcdir)/clutter-actor.h		\
	$(srcdir)/clutter-group.h 		\
	$(srcdir)/clutter-stage.h 		\
	$(srcdir)/clutter-rectangle.h		\
	$(srcdir)/clutter-texture.h 		\
	$(srcdir)/clutter-clone-texture.h 	\
	$(srcdir)/clutter-label.h 		\
	$(srcdir)/clutter-behaviour.h     	\
	$(srcdir)/clutter-behaviour-opacity.h 	\
	$(srcdir)/clutter-behaviour-path.h 	\
	$(srcdir)/clutter-behaviour-scale.h 	\
	$(srcdir)/clutter-alpha.h		\
	$(srcdir)/clutter-media.h		\
	$(srcdir)/clutter-version.h		\
        $(srcdir)/clutter-main.h

backend_h =                                     \
	$(srcdir)/clutter-stage-glx.h 		\
	$(srcdir)/clutter-backend-glx.h 	

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

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(STAMPFILES)

source_c = clutter-main.c 		\
	   clutter-util.c 		\
	   clutter-feature.c 		\
	   clutter-fixed.c		\
           clutter-event.c 		\
	   clutter-color.c 		\
	   clutter-timeline.c 		\
	   clutter-group.c 		\
	   clutter-stage.c 		\
	   clutter-rectangle.c 		\
	   clutter-texture.c 		\
	   clutter-clone-texture.c 	\
	   clutter-label.c 		\
           clutter-actor.c		\
           clutter-behaviour.c 		\
	   clutter-behaviour-opacity.c	\
	   clutter-behaviour-path.c	\
	   clutter-behaviour-scale.c	\
	   clutter-alpha.c 		\
	   clutter-media.c 		\
	   clutter-enum-types.c

backend_c =                                     \
	$(srcdir)/clutter-stage-glx.c 		\
	$(srcdir)/clutter-backend-glx.c 	

source_h_priv = clutter-debug.h clutter-private.h

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_MAJORMINOR@_la_SOURCES = \
					     $(MARSHALFILES) \
                                             $(source_c)     \
                                             $(source_h)     \
                                             $(backend_c)    \
                                             $(backend_h)    \
                                             $(source_h_priv)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter/pango \
	-DPREFIX=\""$(prefix)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DG_DISABLE_DEPRECATED \
	-DG_LOG_DOMAIN=\"Clutter\" \
	$(GCC_FLAGS) \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) 

lib_LTLIBRARIES = libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@.la

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_MAJORMINOR@_la_LIBADD  = \
            @CLUTTER_LIBS@ $(top_builddir)/clutter/pango/libpangoclutter.la

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_MAJORMINOR@_la_LDFLAGS = \
           @CLUTTER_LT_LDFLAGS@

libclutter_@CLUTTER_FLAVOUR@_@CLUTTER_MAJORMINOR@_la_DEPENDENCIES = \
            $(top_builddir)/clutter/pango/libpangoclutter.la

clutterheadersdir = \
       $(includedir)/clutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@/clutter

clutterheaders_HEADERS = $(source_h) 		\
		      clutter-marshal.h 	\
		      clutter-enum-types.h 	\
		      clutter.h

EXTRA_DIST = clutter-marshal.list clutter-version.h.in
