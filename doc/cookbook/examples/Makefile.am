include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

noinst_PROGRAMS = \
	textures-reflection	\
	text-shadow		\
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

textures_reflection_SOURCES 	= textures-reflection.c

text_shadow_SOURCES 		= text-shadow.c
