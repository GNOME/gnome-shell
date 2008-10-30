libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter
libclutterinclude_HEADERS = clutter-osx.h

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterOSX\" \
        -DCLUTTER_COMPILATION \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS) \
	-xobjective-c

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-osx.la

libclutter_osx_la_SOURCES = \
	clutter-backend-osx.h \
	clutter-backend-osx.c \
	clutter-event-osx.c \
	clutter-stage-osx.h \
	clutter-stage-osx.c \
	clutter-osx.h
