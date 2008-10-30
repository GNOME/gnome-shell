libclutterincludedir = $(includedir)/clutter-@CLUTTER_MAJORMINOR@/clutter

INCLUDES = \
	-DG_LOG_DOMAIN=\"ClutterFruity\" \
        -DCLUTTER_COMPILATION \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter \
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(GCC_FLAGS)	\
    	-xobjective-c

LDADD = $(CLUTTER_LIBS)

noinst_LTLIBRARIES = libclutter-fruity.la

libclutter_fruity_la_SOURCES = 	\
	clutter-backend-fruity.h 	\
	clutter-backend-fruity.c 	\
	clutter-stage-fruity.h 		\
	clutter-stage-fruity.c 		\
	clutter-fruity.h 		\
	clutter-fruity.c
