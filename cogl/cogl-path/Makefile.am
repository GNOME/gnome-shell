NULL =

BUILT_SOURCES =

CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =

# tesselator sources
cogl_tesselator_sources = \
	tesselator/dict-list.h 	\
	tesselator/dict.c 		\
	tesselator/dict.h 		\
	tesselator/geom.c 		\
	tesselator/geom.h 		\
	tesselator/gluos.h 		\
	tesselator/memalloc.h 	\
	tesselator/mesh.c 		\
	tesselator/mesh.h 		\
	tesselator/normal.c 		\
	tesselator/normal.h 		\
	tesselator/priorityq-heap.h 	\
	tesselator/priorityq-sort.h 	\
	tesselator/priorityq.c 	\
	tesselator/priorityq.h 	\
	tesselator/render.c 		\
	tesselator/render.h 		\
	tesselator/sweep.c 		\
	tesselator/sweep.h 		\
	tesselator/tess.c 		\
	tesselator/tess.h 		\
	tesselator/tesselator.h 	\
	tesselator/tessmono.c 	\
	tesselator/tessmono.h 	\
	tesselator/GL/glu.h 		\
	$(NULL)

source_c = \
	$(cogl_tesselator_sources) \
	cogl-path-private.h \
	cogl-path.c \
	$(NULL)

EXTRA_DIST += \
	tesselator/README 		\
	tesselator/priorityq-heap.c	\
	cogl-path.symbols		\
	$(NULL)

source_1_x_h = \
	cogl-path-types.h \
	$(NULL)

source_h = \
	cogl-path.h \
	$(source_1_x_h) \
	cogl-path-functions.h \
	$(NULL)

# glib-mkenums rules
glib_enum_h = cogl-path-enum-types.h
glib_enum_c = cogl-path-enum-types.c
glib_enum_headers = $(source_1_x_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

mutterlibdir = $(libdir)/mutter
mutterlib_LTLIBRARIES = libmutter-cogl-path.la

libmutter_cogl_path_la_SOURCES = $(source_c) $(source_h)
nodist_libmutter_cogl_path_la_SOURCES = $(BUILT_SOURCES)
libmutter_cogl_path_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libmutter_cogl_path_la_LIBADD = $(top_builddir)/cogl/libmutter-cogl.la
libmutter_cogl_path_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libmutter_cogl_path_la_LDFLAGS = \
	-export-dynamic \
	-export-symbols-regex "^(cogl|cogl2)_(framebuffer|path|is|clip|[sg]et)_.*" \
	-no-undefined \
	-avoid-version \
	-rpath $(mutterlibdir)

AM_CPPFLAGS = \
	-DCOGL_COMPILATION		\
	-DG_LOG_DOMAIN=\"CoglPath\"	\
	-I$(srcdir)/tesselator		\
	-I$(top_srcdir)/cogl		\
	-I$(top_builddir)/cogl		\
	-I$(top_srcdir)/cogl/winsys	\
	-I$(top_srcdir)			\
	-I$(top_builddir)

cogl_base_includedir = $(includedir)/mutter
cogl_pathheadersdir = $(cogl_base_includedir)/cogl/cogl-path
cogl_pathheaders_HEADERS = $(source_h)
nodist_cogl_pathheaders_HEADERS = cogl-path-enum-types.h

pc_files = mutter-cogl-path-1.0.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

DISTCLEANFILES += $(pc_files)
