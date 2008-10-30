libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter
libclutterinclude_HEADERS = clutter-sdl.h

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterSDL\" \
        -DCLUTTER_COMPILATION \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter/ \
	-I$(top_builddir)/clutter/ \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-sdl.la

libclutter_sdl_la_SOURCES = \
	clutter-backend-sdl.h \
	clutter-backend-sdl.c \
	clutter-event-sdl.c \
	clutter-stage-sdl.h \
	clutter-stage-sdl.c \
	clutter-sdl.h
