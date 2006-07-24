MARSHALFILES = clutter-marshal.c clutter-marshal.h
ENUMFILES = clutter-enum-types.c clutter-enum-types.h
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
GLIB_MKENUMS=`pkg-config --variable=glib_mkenums glib-2.0`

BUILT_SOURCES = $(MARSHALFILES) $(ENUMFILES)

source_h =				  \
	$(srcdir)/clutter-keysyms.h       \
	$(srcdir)/clutter-util.h          \
	$(srcdir)/clutter-media.h         \
        $(srcdir)/clutter-event.h         \
	$(srcdir)/clutter-color.h         \
	$(srcdir)/clutter-feature.h       \
	$(srcdir)/clutter-timeline.h      \
	$(srcdir)/clutter-actor.h 	  \
	$(srcdir)/clutter-group.h         \
	$(srcdir)/clutter-stage.h         \
	$(srcdir)/clutter-rectangle.h     \
	$(srcdir)/clutter-texture.h       \
	$(srcdir)/clutter-clone-texture.h \
	$(srcdir)/clutter-video-texture.h \
	$(srcdir)/clutter-label.h         \
        $(srcdir)/clutter-main.h          

clutter-marshal.h: clutter-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=clutter_marshal \
	$(srcdir)/clutter-marshal.list --header > clutter-marshal.h )

clutter-marshal.c: clutter-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=clutter_marshal \
	$(srcdir)/clutter-marshal.list --body --header > clutter-marshal.c )

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

CLEANFILES = $(BUILT_SOURCES) stamp-clutter-enum-types.h

source_c = clutter-main.c          \
	   clutter-util.c          \
	   clutter-feature.c       \
	   clutter-media.c         \
           clutter-event.c         \
	   clutter-color.c         \
	   clutter-timeline.c      \
	   clutter-group.c         \
	   clutter-stage.c         \
	   clutter-rectangle.c     \
	   clutter-texture.c       \
	   clutter-clone-texture.c \
	   clutter-video-texture.c \
	   clutter-label.c         \
           clutter-actor.c	   \
	   clutter-enum-types.c

source_h_priv = clutter-private.h

libclutter_@CLUTTER_MAJORMINOR@_la_SOURCES = $(MARSHALFILES) \
                                             $(source_c)     \
                                             $(source_h)     \
                                             $(source_h_priv)

INCLUDES = @GCC_FLAGS@ @CLUTTER_CFLAGS@  $(GST_CFLAGS) -I$(top_srcdir)

lib_LTLIBRARIES = libclutter-@CLUTTER_MAJORMINOR@.la

libclutter_@CLUTTER_MAJORMINOR@_la_LIBADD  = @CLUTTER_LIBS@ $(GST_LIBS)
libclutter_@CLUTTER_MAJORMINOR@_la_LDFLAGS = @CLUTTER_LT_LDFLAGS@

clutterheadersdir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter
clutterheaders_HEADERS = $(source_h) 		\
		      clutter-marshal.h 	\
		      clutter-enum-types.h 	\
		      clutter.h

EXTRA_DIST = clutter-marshal.list
