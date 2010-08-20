include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

noinst_PROGRAMS = \
	animations-rotating	\
	text-shadow		\
	textures-reflection	\
	textures-split-go	\
	textures-sub-texture	\
	layouts-stacking \
	layouts-stacking-diff-sized-actors	\
	events-mouse-scroll \
	events-pointer-motion	\
	events-pointer-motion-crossing	\
	events-pointer-motion-stacked	\
	events-pointer-motion-scribbler	\
	textures-crossfade	\
	textures-crossfade-cogl	\
	textures-crossfade-slideshow	\
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

animations_rotating_SOURCES                = animations-rotating.c
text_shadow_SOURCES                        = text-shadow.c
textures_reflection_SOURCES                = textures-reflection.c
textures_split_go_SOURCES                  = textures-split-go.c
textures_sub_texture_SOURCES               = textures-sub-texture.c
layouts_stacking_SOURCES                   = layouts-stacking.c
layouts_stacking_diff_sized_actors_SOURCES = layouts-stacking-diff-sized-actors.c
events_mouse_scroll_SOURCES                = events-mouse-scroll.c
events_pointer_motion_SOURCES              = events-pointer-motion.c
events_pointer_motion_crossing_SOURCES     = events-pointer-motion-crossing.c
events_pointer_motion_stacked_SOURCES      = events-pointer-motion-stacked.c
events_pointer_motion_scribbler_SOURCES    = events-pointer-motion-scribbler.c
textures_crossfade_SOURCES                 = textures-crossfade.c
textures_crossfade_cogl_SOURCES            = textures-crossfade-cogl.c
textures_crossfade_slideshow_SOURCES       = textures-crossfade-slideshow.c

-include $(top_srcdir)/build/autotools/Makefile.am.gitignore
