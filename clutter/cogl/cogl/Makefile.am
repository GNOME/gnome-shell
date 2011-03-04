include $(top_srcdir)/build/autotools/Makefile.am.silent

# preamble

NULL =

SUBDIRS =

BUILT_SOURCES =

EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =

noinst_LTLIBRARIES =
lib_LTLIBRARIES =

INCLUDES = \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(srcdir)/tesselator			\
	-I$(srcdir)/winsys 			\
	-I$(srcdir)/driver/$(COGL_DRIVER) 	\
	$(NULL)

if COGL_STANDALONE_BUILD
INCLUDES += \
	-I$(top_srcdir)/clutter/cogl		\
	-I$(top_builddir)/clutter/cogl          \
	$(NULL)
else
INCLUDES += \
	-I$(top_srcdir)/clutter/cogl		\
	-I$(top_builddir)/clutter/cogl          \
	$(NULL)
endif

AM_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES 	\
	-DG_DISABLE_DEPRECATED		\
	-DG_LOG_DOMAIN=\"Cogl\" 	\
	-DCLUTTER_COMPILATION		\
	$(NULL)

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)

BUILT_SOURCES += cogl-defines.h
DISTCLEANFILES += cogl-defines.h
EXTRA_DIST += cogl-defines.h.in

# Note: The cogl-1.0/cogl-gl-1.0 files are essentially for
# compatability only.  I'm not really sure who could possibly be using
# them so we may decide to remove them entirely at some point.
pc_files = \
	cogl-1.0.pc \
	cogl-gl-1.0.pc

if COGL_STANDALONE_BUILD
pc_files += cogl-$(COGL_API_VERSION).pc
endif

cogl-gl-1.0.pc: cogl-1.0.pc
	$(QUIET_GEN)cp -f $< $(@F)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cogl.pc.in
DISTCLEANFILES += $(pc_files)

# public headers
cogl_public_h = \
	$(srcdir)/cogl-object.h 		\
	$(srcdir)/cogl-bitmap.h 		\
	$(srcdir)/cogl-buffer.h 		\
	$(srcdir)/cogl-color.h 			\
	$(srcdir)/cogl-fixed.h 			\
	$(srcdir)/cogl-material-compat.h 	\
	$(srcdir)/cogl-pipeline.h 		\
	$(srcdir)/cogl-vector.h 		\
	$(srcdir)/cogl-matrix.h 		\
	$(srcdir)/cogl-offscreen.h 		\
	$(srcdir)/cogl-primitives.h 		\
	$(srcdir)/cogl-path.h 			\
	$(srcdir)/cogl-pixel-array.h		\
	$(srcdir)/cogl-shader.h 		\
	$(srcdir)/cogl-texture.h 		\
	$(srcdir)/cogl-texture-3d.h             \
	$(srcdir)/cogl-types.h 			\
	$(srcdir)/cogl-vertex-buffer.h 		\
	$(srcdir)/cogl-index-array.h 		\
	$(srcdir)/cogl-vertex-array.h 		\
	$(srcdir)/cogl-indices.h 		\
	$(srcdir)/cogl-attribute.h 		\
	$(srcdir)/cogl-primitive.h 		\
	$(srcdir)/cogl-clip-state.h		\
	$(srcdir)/cogl-framebuffer.h		\
	$(srcdir)/cogl-clutter.h       		\
	$(srcdir)/cogl.h			\
	$(NULL)

cogl_experimental_h = \
	$(srcdir)/cogl-renderer.h 		\
	$(srcdir)/cogl-swap-chain.h 		\
	$(srcdir)/cogl-onscreen-template.h 	\
	$(srcdir)/cogl-display.h 		\
	$(srcdir)/cogl-context.h 		\
	$(srcdir)/cogl2-path.h 			\
	$(srcdir)/cogl2-clip-state.h		\
	$(NULL)

# driver sources
cogl_driver_sources =

if COGL_DRIVER_GL
cogl_driver_sources += \
	$(srcdir)/driver/gl/cogl-feature-functions-gl.h	\
	$(srcdir)/driver/gl/cogl-gl.c			\
	$(srcdir)/driver/gl/cogl-texture-driver-gl.c	\
	$(NULL)
endif

if COGL_DRIVER_GLES
cogl_driver_sources += \
	$(srcdir)/driver/gles/cogl-feature-functions-gles.h	\
	$(srcdir)/driver/gles/cogl-gles.c			\
	$(srcdir)/driver/gles/cogl-texture-driver-gles.c	\
	$(NULL)

endif # COGL_DRIVER_GLES

# winsys sources, common to all backends
cogl_winsys_common_sources = \
	$(srcdir)/winsys/cogl-winsys-private.h \
	$(srcdir)/winsys/cogl-winsys.c \
	$(NULL)

# tesselator sources
cogl_tesselator_sources = \
	$(srcdir)/tesselator/dict-list.h 	\
	$(srcdir)/tesselator/dict.c 		\
	$(srcdir)/tesselator/dict.h 		\
	$(srcdir)/tesselator/geom.c 		\
	$(srcdir)/tesselator/geom.h 		\
	$(srcdir)/tesselator/gluos.h 		\
	$(srcdir)/tesselator/memalloc.h 	\
	$(srcdir)/tesselator/mesh.c 		\
	$(srcdir)/tesselator/mesh.h 		\
	$(srcdir)/tesselator/normal.c 		\
	$(srcdir)/tesselator/normal.h 		\
	$(srcdir)/tesselator/priorityq-heap.h 	\
	$(srcdir)/tesselator/priorityq-sort.h 	\
	$(srcdir)/tesselator/priorityq.c 	\
	$(srcdir)/tesselator/priorityq.h 	\
	$(srcdir)/tesselator/render.c 		\
	$(srcdir)/tesselator/render.h 		\
	$(srcdir)/tesselator/sweep.c 		\
	$(srcdir)/tesselator/sweep.h 		\
	$(srcdir)/tesselator/tess.c 		\
	$(srcdir)/tesselator/tess.h 		\
	$(srcdir)/tesselator/tesselator.h 	\
	$(srcdir)/tesselator/tessmono.c 	\
	$(srcdir)/tesselator/tessmono.h 	\
	$(srcdir)/tesselator/GL/glu.h 		\
	$(NULL)

EXTRA_DIST += \
	$(srcdir)/tesselator/README 		\
	$(srcdir)/tesselator/priorityq-heap.c	\
	$(NULL)

# sources
cogl_sources_c = \
	$(cogl_driver_sources)				\
	$(cogl_winsys_common_sources)			\
	$(cogl_tesselator_sources)			\
	$(srcdir)/cogl-private.h			\
	$(srcdir)/cogl-debug.h 				\
	$(srcdir)/cogl-debug-options.h			\
	$(srcdir)/cogl-handle.h 			\
	$(srcdir)/cogl-context-private.h		\
	$(srcdir)/cogl-context.c			\
	$(srcdir)/cogl-renderer-private.h		\
	$(srcdir)/cogl-renderer.h			\
	$(srcdir)/cogl-renderer.c			\
	$(srcdir)/cogl-swap-chain-private.h		\
	$(srcdir)/cogl-swap-chain.h			\
	$(srcdir)/cogl-swap-chain.c			\
	$(srcdir)/cogl-onscreen-template-private.h 	\
	$(srcdir)/cogl-onscreen-template.h 		\
	$(srcdir)/cogl-onscreen-template.c 		\
	$(srcdir)/cogl-display-private.h		\
	$(srcdir)/cogl-display.h			\
	$(srcdir)/cogl-display.c			\
	$(srcdir)/cogl-internal.h			\
	$(srcdir)/cogl.c				\
	$(srcdir)/cogl-object-private.h			\
	$(srcdir)/cogl-object.h				\
	$(srcdir)/cogl-object.c				\
	$(srcdir)/cogl-util.h 				\
	$(srcdir)/cogl-util.c 				\
	$(srcdir)/cogl-bitmap-private.h 		\
	$(srcdir)/cogl-bitmap.c 			\
	$(srcdir)/cogl-bitmap-fallback.c 		\
	$(srcdir)/cogl-primitives-private.h 		\
	$(srcdir)/cogl-primitives.h 			\
	$(srcdir)/cogl-primitives.c 			\
	$(srcdir)/cogl-path-private.h 			\
	$(srcdir)/cogl-path.h 				\
	$(srcdir)/cogl-path.c 				\
	$(srcdir)/cogl2-path.h 				\
	$(srcdir)/cogl2-path.c 				\
	$(srcdir)/cogl-bitmap-pixbuf.c 			\
	$(srcdir)/cogl-clip-stack.h 			\
	$(srcdir)/cogl-clip-stack.c			\
	$(srcdir)/cogl-clip-state-private.h		\
	$(srcdir)/cogl-clip-state.h			\
	$(srcdir)/cogl-clip-state.c			\
	$(srcdir)/cogl2-clip-state.h 			\
	$(srcdir)/cogl2-clip-state.c 			\
	$(srcdir)/cogl-feature-private.h                \
	$(srcdir)/cogl-feature-private.c                \
	$(srcdir)/cogl-fixed.c		    		\
	$(srcdir)/cogl-color-private.h    		\
	$(srcdir)/cogl-color.c				\
	$(srcdir)/cogl-buffer-private.h 		\
	$(srcdir)/cogl-buffer.c				\
	$(srcdir)/cogl-pixel-array-private.h		\
	$(srcdir)/cogl-pixel-array.c			\
	$(srcdir)/cogl-vertex-buffer-private.h 		\
	$(srcdir)/cogl-vertex-buffer.c			\
	$(srcdir)/cogl-index-array-private.h		\
	$(srcdir)/cogl-index-array.c			\
	$(srcdir)/cogl-vertex-array-private.h		\
	$(srcdir)/cogl-vertex-array.c			\
	$(srcdir)/cogl-indices-private.h		\
	$(srcdir)/cogl-indices.c			\
	$(srcdir)/cogl-attribute-private.h		\
	$(srcdir)/cogl-attribute.c			\
	$(srcdir)/cogl-primitive-private.h		\
	$(srcdir)/cogl-primitive.c			\
	$(srcdir)/cogl-matrix.c				\
	$(srcdir)/cogl-vector.c				\
	$(srcdir)/cogl-matrix-private.h			\
	$(srcdir)/cogl-matrix-stack.c			\
	$(srcdir)/cogl-matrix-stack.h			\
	$(srcdir)/cogl-pipeline.c			\
	$(srcdir)/cogl-pipeline-private.h		\
	$(srcdir)/cogl-pipeline-opengl.c		\
	$(srcdir)/cogl-pipeline-opengl-private.h	\
	$(srcdir)/cogl-pipeline-fragend-glsl.c		\
	$(srcdir)/cogl-pipeline-fragend-glsl-private.h	\
	$(srcdir)/cogl-pipeline-fragend-arbfp.c		\
	$(srcdir)/cogl-pipeline-fragend-arbfp-private.h	\
	$(srcdir)/cogl-pipeline-fragend-fixed.c		\
	$(srcdir)/cogl-pipeline-fragend-fixed-private.h	\
	$(srcdir)/cogl-pipeline-vertend-glsl.c		\
	$(srcdir)/cogl-pipeline-vertend-glsl-private.h	\
	$(srcdir)/cogl-pipeline-vertend-fixed.c		\
	$(srcdir)/cogl-pipeline-vertend-fixed-private.h	\
	$(srcdir)/cogl-pipeline-progend-glsl.c		\
	$(srcdir)/cogl-pipeline-progend-glsl-private.h	\
	$(srcdir)/cogl-material-compat.c		\
	$(srcdir)/cogl-program.c			\
	$(srcdir)/cogl-program-private.h		\
	$(srcdir)/cogl-blend-string.c			\
	$(srcdir)/cogl-blend-string.h			\
	$(srcdir)/cogl-debug.c				\
	$(srcdir)/cogl-sub-texture-private.h            \
	$(srcdir)/cogl-texture-private.h		\
	$(srcdir)/cogl-texture-2d-private.h             \
	$(srcdir)/cogl-texture-2d-sliced-private.h 	\
	$(srcdir)/cogl-texture-3d-private.h             \
	$(srcdir)/cogl-texture-driver.h			\
	$(srcdir)/cogl-sub-texture.c                    \
	$(srcdir)/cogl-texture.c			\
	$(srcdir)/cogl-texture-2d.c                     \
	$(srcdir)/cogl-texture-2d-sliced.c		\
	$(srcdir)/cogl-texture-3d.c                     \
	$(srcdir)/cogl-texture-rectangle-private.h      \
	$(srcdir)/cogl-texture-rectangle.c              \
	$(srcdir)/cogl-rectangle-map.h                  \
	$(srcdir)/cogl-rectangle-map.c                  \
	$(srcdir)/cogl-atlas.h                          \
	$(srcdir)/cogl-atlas.c                          \
	$(srcdir)/cogl-atlas-texture-private.h          \
	$(srcdir)/cogl-atlas-texture.c                  \
	$(srcdir)/cogl-blit.h				\
	$(srcdir)/cogl-blit.c				\
	$(srcdir)/cogl-spans.h				\
	$(srcdir)/cogl-spans.c				\
	$(srcdir)/cogl-journal-private.h		\
	$(srcdir)/cogl-journal.c			\
	$(srcdir)/cogl-framebuffer-private.h		\
	$(srcdir)/cogl-framebuffer.c 			\
	$(srcdir)/cogl-matrix-mesa.h 			\
	$(srcdir)/cogl-matrix-mesa.c 			\
	$(srcdir)/cogl-profile.h 			\
	$(srcdir)/cogl-profile.c 			\
	$(srcdir)/cogl-flags.h				\
	$(srcdir)/cogl-bitmask.h                        \
	$(srcdir)/cogl-bitmask.c                        \
	$(srcdir)/cogl-shader-boilerplate.h		\
	$(srcdir)/cogl-shader-private.h			\
	$(srcdir)/cogl-shader.c                        	\
	$(srcdir)/cogl-gtype-private.h                  \
	$(srcdir)/cogl-point-in-poly-private.h       	\
	$(srcdir)/cogl-point-in-poly.c       		\
	$(srcdir)/cogl-clutter.c       			\
	$(NULL)

if SUPPORT_XLIB
cogl_experimental_h += \
       $(srcdir)/winsys/cogl-texture-pixmap-x11.h \
       $(srcdir)/cogl-xlib.h

cogl_sources_c += \
	$(srcdir)/cogl-renderer-x11-private.h \
	$(srcdir)/cogl-renderer-xlib-private.h \
	$(srcdir)/cogl-renderer-xlib.c \
	$(srcdir)/cogl-display-xlib-private.h \
	$(srcdir)/cogl-xlib.c \
	$(srcdir)/winsys/cogl-texture-pixmap-x11.c \
	$(srcdir)/winsys/cogl-texture-pixmap-x11-private.h
endif
if SUPPORT_GLX
cogl_sources_c += \
	$(srcdir)/cogl-renderer-glx-private.h \
	$(srcdir)/cogl-display-glx-private.h \
	$(srcdir)/winsys/cogl-winsys-glx-feature-functions.h \
	$(srcdir)/winsys/cogl-winsys-glx.c
endif
if SUPPORT_EGL_PLATFORM_POWERVR_X11
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl.c
endif
if SUPPORT_EGL_PLATFORM_POWERVR_NULL
cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-egl.c
endif
if SUPPORT_EGL_PLATFORM_GDL
cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-egl.c
endif
if SUPPORT_WIN32
cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-win32.c \
       $(srcdir)/winsys/cogl-winsys-stub.c
endif
if SUPPORT_OSX
cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-osx.c \
       $(srcdir)/winsys/cogl-winsys-stub.c
endif

EXTRA_DIST += stb_image.c

# glib-mkenums rules
glib_enum_h = cogl-enum-types.h
glib_enum_c = cogl-enum-types.c
glib_enum_headers = $(cogl_public_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

if COGL_STANDALONE_BUILD
lib_LTLIBRARIES += libcogl.la
else
noinst_LTLIBRARIES += libcogl.la
endif

libcogl_la_LIBADD = -lm $(COGL_DEP_LIBS)
if SUPPORT_GLX
libcogl_la_LIBADD += -ldl
endif
if COGL_STANDALONE_BUILD
# XXX: The aim is to eventually get rid of all private API exports
# for coglpango.
libcogl_la_LDFLAGS = \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-export-dynamic \
	-export-symbols-regex "^(cogl|_cogl_debug_flags|_cogl_atlas_new|_cogl_atlas_add_reorganize_callback|_cogl_atlas_reserve_space|_cogl_callback|_cogl_util_get_eye_planes_for_screen_poly).*"
endif

libcogl_la_SOURCES = $(cogl_sources_c)
nodist_libcogl_la_SOURCES = $(BUILT_SOURCES)

# Cogl installed headers
cogl_headers = \
	$(cogl_public_h) \
	$(srcdir)/cogl-deprecated.h \
	$(NULL)

if COGL_STANDALONE_BUILD
coglincludedir = $(includedir)/cogl/cogl
coglinclude_HEADERS = $(cogl_headers) $(cogl_experimental_h)
nodist_coglinclude_HEADERS = cogl-defines.h cogl-enum-types.h
else
coglincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
coglinclude_HEADERS = $(cogl_headers) $(cogl_experimental_h)
nodist_coglinclude_HEADERS = cogl-defines.h cogl-enum-types.h
endif

-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =

if HAVE_INTROSPECTION
Cogl-1.0.gir: libcogl.la Makefile

Cogl_1_0_gir_NAMESPACE = Cogl
Cogl_1_0_gir_VERSION = 1.0
Cogl_1_0_gir_LIBS = libcogl.la
Cogl_1_0_gir_FILES = $(cogl_headers) cogl-enum-types.h
Cogl_1_0_gir_CFLAGS = $(INCLUDES) $(AM_CPPFLAGS)
Cogl_1_0_gir_INCLUDES = GL-1.0 GObject-2.0
Cogl_1_0_gir_EXPORT_PACKAGES = cogl-1.0
Cogl_1_0_gir_SCANNERFLAGS = --warn-all --c-include='cogl/cogl.h'

INTROSPECTION_GIRS += Cogl-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

CLEANFILES += $(INTROSPECTION_GIRS)
endif
