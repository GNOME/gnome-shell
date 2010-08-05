include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

noinst_PROGRAMS = \
	animations-rotating	\
	text-shadow		\
	textures-reflection	\
	textures-split-go	\
	textures-sub-texture	\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/clutter \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_srcdir)/clutter/cogl/pango \
	-I$(top_builddir)/clutter \
	-I$(top_builddir)/clutter/cogl \
	$(NULL)

LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la

AM_CFLAGS = \
        $(CLUTTER_CFLAGS) \
        $(MAINTAINER_CFLAGS) \
        -DG_DISABLE_SINGLE_INCLUDES \
        -DTESTS_DATA_DIR=\""$(top_srcdir)/tests/data/"\"

AM_LDFLAGS = $(CLUTTER_LIBS)

animations_rotating_SOURCES   = animations-rotating.c
text_shadow_SOURCES           = text-shadow.c
textures_reflection_SOURCES   = textures-reflection.c
textures_split_go_SOURCES     = textures-split-go.c
textures_sub_texture_SOURCES  = textures-sub-texture.c
