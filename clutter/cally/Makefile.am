include $(top_srcdir)/build/autotools/Makefile.am.silent

EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =

# pkg-config ==================================================================
pc_files = \
	cally-$(CLUTTER_API_VERSION).pc

cally-$(CLUTTER_API_VERSION).pc: cally.pc
	$(QUIET_GEN)cp -f $< $(@F)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cally.pc.in
CLEANFILES += $(pc_files)

noinst_LTLIBRARIES = libcally.la

cally_h_sources = 		cally.h			\
				cally-actor.h		\
				cally-factory.h		\
				cally-group.h    	\
				cally-main.h		\
				cally-rectangle.h	\
				cally-root.h		\
				cally-stage.h		\
				cally-text.h		\
				cally-texture.h		\
				cally-clone.h		\
				cally-util.h

cally_private_h_sources = 	cally-actor-private.h

cally_c_sources = 		cally.c			\
				cally-actor.c		\
				cally-group.c    	\
				cally-rectangle.c	\
				cally-root.c		\
				cally-stage.c		\
				cally-text.c		\
				cally-texture.c		\
				cally-clone.c		\
				cally-util.c

libcally_la_SOURCES =					\
	$(cally_private_h_sources)			\
	$(cally_h_sources)				\
	$(cally_c_sources)

INCLUDES = \
	-I$(top_srcdir)		 			\
	-I$(top_srcdir)/clutter	 			\
	-I$(top_srcdir)/clutter/cally 			\
	-I$(top_srcdir)/clutter/cogl

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Cally\"			\
	-DCLUTTER_COMPILATION				\
	-DVERSION=\"$(VERSION)\"			\
	$(CLUTTER_DEBUG_CFLAGS)

AM_CFLAGS = \
	$(CLUTTER_CFLAGS) 				\
	$(MAINTAINER_CFLAGS)


libcallydir=$(includedir)/clutter-@CLUTTER_API_VERSION@/cally

# In opposit to GAIL, CALLY exports all the headers. It will very
# unlikely in any real final clutter-based application to use only raw
# CALLY. In fact, after HAIL experience, probably export GAIL
# interfaces would be a good idea
libcally_HEADERS = \
	$(cally_h_sources)

libcally_la_LIBADD =     				\
	$(CLUTTER_LIBS)
