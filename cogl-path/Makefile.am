include $(top_srcdir)/build/autotools/Makefile.am.silent

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
	cogl1-path.c \
	cogl-path.c \
	$(NULL)

EXTRA_DIST += \
	tesselator/README 		\
	tesselator/priorityq-heap.c	\
	cogl-path.symbols		\
	$(NULL)

source_1_x_h = \
	cogl-path-types.h \
	cogl1-path-functions.h \
	$(NULL)

source_h = \
	cogl-path.h \
	$(source_1_x_h) \
	cogl2-path-functions.h \
	$(NULL)

if USE_GLIB
# glib-mkenums rules
glib_enum_h = cogl-path-enum-types.h
glib_enum_c = cogl-path-enum-types.c
glib_enum_headers = $(source_1_x_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums
endif

lib_LTLIBRARIES = libcogl-path.la

libcogl_path_la_SOURCES = $(source_c) $(source_h)
nodist_libcogl_path_la_SOURCES = $(BUILT_SOURCES)
libcogl_path_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libcogl_path_la_LIBADD = $(top_builddir)/cogl/libcogl.la
libcogl_path_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl_path_la_LDFLAGS = \
	-export-dynamic \
	-export-symbols-regex "^(cogl|cogl2)_(framebuffer|path|is|clip|[sg]et)_.*" \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-rpath $(libdir)

AM_CPPFLAGS = \
	-DCOGL_COMPILATION		\
	-DG_LOG_DOMAIN=\"CoglPath\"	\
	-I$(srcdir)/tesselator		\
	-I$(top_srcdir)/cogl		\
	-I$(top_builddir)/cogl		\
	-I$(top_srcdir)/cogl/winsys	\
	-I$(top_srcdir)			\
	-I$(top_builddir)

cogl_pathheadersdir = $(includedir)/cogl/cogl-path
cogl_pathheaders_HEADERS = $(source_h)
if USE_GLIB
nodist_cogl_pathheaders_HEADERS = cogl-path-enum-types.h
endif

pc_files = cogl-path-1.0.pc \
	   cogl-path-2.0-experimental.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cogl-path.pc.in
DISTCLEANFILES += $(pc_files)

dist-hook:	\
	$(top_builddir)/build/win32/vs9/cogl-path.vcproj	\
	$(top_builddir)/build/win32/vs10/cogl-path.vcxproj	\
	$(top_builddir)/build/win32/vs10/cogl-path.vcxproj.filters

MSVC_PROJECT = cogl-path
MSVC_PROJECT_SRCDIR = cogl-path
MSVC_PROJECT_SRCS = $(source_c)
MSVC_PROJECT_EXCLUDES = dummy

include $(top_srcdir)/build/Makefile-msvcproj.mak
