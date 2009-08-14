libclutterincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterFruity\" 	\
        -DCLUTTER_COMPILATION 			\
	-I$(top_srcdir) 			\
	-I$(top_srcdir)/clutter			\
	-I$(top_builddir)/clutter

common_ldadd = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-fruity.la

libclutter_fruity_la_CFLAGS = $(CLUTTER_CFLAGS) $(CLUTTER_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS) -xobjective-c
libclutter_fruity_la_LIBADD = $(common_ldadd)
libclutter_fruity_la_SOURCES = 		\
	clutter-backend-fruity.h 	\
	clutter-backend-fruity.c 	\
	clutter-stage-fruity.h 		\
	clutter-stage-fruity.c 		\
	clutter-fruity.h 		\
	clutter-fruity.c
