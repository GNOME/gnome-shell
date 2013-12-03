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

AM_CPPFLAGS = \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(srcdir)/deprecated			\
	-I$(srcdir)/winsys 			\
	-I$(srcdir)/driver/gl 			\
	-I$(srcdir)/driver/gl/gl 		\
	-I$(srcdir)/driver/gl/gles 		\
	$(NULL)

if !USE_GLIB
	AM_CPPFLAGS += -I$(top_builddir)/deps/glib
endif

AM_CPPFLAGS += \
	-DG_LOG_DOMAIN=\"Cogl\" 	\
	-DCOGL_COMPILATION		\
	-DCOGL_GL_LIBNAME=\"$(COGL_GL_LIBNAME)\" \
	-DCOGL_GLES1_LIBNAME=\"$(COGL_GLES1_LIBNAME)\" \
	-DCOGL_GLES2_LIBNAME=\"$(COGL_GLES2_LIBNAME)\" \
	-DCOGL_LOCALEDIR=\""$(localedir)"\" \
	$(NULL)

if HAVE_COGL_DEFAULT_DRIVER
AM_CPPFLAGS += \
	-DCOGL_DEFAULT_DRIVER=\"$(COGL_DEFAULT_DRIVER)\"
endif


AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)

BUILT_SOURCES += cogl-defines.h cogl-egl-defines.h cogl-gl-header.h
DISTCLEANFILES += cogl-defines.h cogl-egl-defines.h cogl-gl-header.h
EXTRA_DIST += cogl-defines.h.in cogl-egl-defines.h.in cogl-gl-header.h.in

# Note: The cogl-1.0/cogl-gl-1.0 files are essentially for
# compatability only.  I'm not really sure who could possibly be using
# them so we may decide to remove them entirely at some point.
pc_files = \
	cogl-1.0.pc \
	cogl-gl-1.0.pc

pc_files += cogl-$(COGL_API_VERSION)-experimental.pc

cogl-gl-1.0.pc: cogl-1.0.pc
	$(QUIET_GEN)cp -f $< $(@F)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cogl-1.0.pc.in cogl-$(COGL_API_VERSION)-experimental.pc.in
DISTCLEANFILES += $(pc_files)

cogl_deprecated_h = \
	$(srcdir)/deprecated/cogl-fixed.h 		\
	$(srcdir)/deprecated/cogl-material-compat.h 	\
	$(srcdir)/deprecated/cogl-vertex-buffer.h 	\
	$(srcdir)/deprecated/cogl-shader.h 		\
	$(srcdir)/deprecated/cogl-clutter.h       	\
	$(srcdir)/deprecated/cogl-type-casts.h       	\
	$(NULL)

# public 1.x api headers
cogl_1_public_h = \
	$(cogl_deprecated_h)			\
	$(srcdir)/cogl1-context.h 		\
	$(srcdir)/cogl-bitmap.h 		\
	$(srcdir)/cogl-color.h 			\
	$(srcdir)/cogl-matrix.h 		\
	$(srcdir)/cogl-offscreen.h 		\
	$(srcdir)/cogl-primitives.h 		\
	$(srcdir)/cogl-texture.h 		\
	$(srcdir)/cogl-types.h 			\
	$(srcdir)/cogl.h			\
	$(NULL)

# experimental 2.0 api headers
# Note: we don't run glib-mkenums over these headers
cogl_experimental_h = \
	$(srcdir)/cogl-object.h 		\
	$(srcdir)/cogl-renderer.h 		\
	$(srcdir)/cogl-swap-chain.h 		\
	$(srcdir)/cogl-onscreen-template.h 	\
	$(srcdir)/cogl-display.h 		\
	$(srcdir)/cogl-context.h 		\
	$(srcdir)/cogl-pipeline.h 		\
	$(srcdir)/cogl-pipeline-state.h 	\
	$(srcdir)/cogl-pipeline-layer-state.h 	\
	$(srcdir)/cogl-snippet.h		\
	$(srcdir)/cogl-gles2.h			\
	$(srcdir)/cogl-gles2-types.h		\
	$(srcdir)/cogl-index-buffer.h 		\
	$(srcdir)/cogl-attribute-buffer.h 	\
	$(srcdir)/cogl-indices.h 		\
	$(srcdir)/cogl-attribute.h 		\
	$(srcdir)/cogl-primitive.h 		\
	$(srcdir)/cogl-clip-state.h		\
	$(srcdir)/cogl-framebuffer.h		\
	$(srcdir)/cogl-onscreen.h		\
	$(srcdir)/cogl-frame-info.h		\
	$(srcdir)/cogl-vector.h 		\
	$(srcdir)/cogl-euler.h 			\
	$(srcdir)/cogl-output.h			\
	$(srcdir)/cogl-quaternion.h 		\
	$(srcdir)/cogl-matrix-stack.h		\
	$(srcdir)/cogl-poll.h			\
	$(srcdir)/cogl-texture-3d.h             \
	$(srcdir)/cogl-texture-2d.h             \
	$(srcdir)/cogl-texture-2d-gl.h 		\
	$(srcdir)/cogl-texture-rectangle.h      \
	$(srcdir)/cogl-texture-2d-sliced.h      \
	$(srcdir)/cogl-sub-texture.h            \
	$(srcdir)/cogl-atlas-texture.h          \
	$(srcdir)/cogl-meta-texture.h		\
	$(srcdir)/cogl-primitive-texture.h	\
	$(srcdir)/cogl-depth-state.h 		\
	$(srcdir)/cogl-buffer.h 		\
	$(srcdir)/cogl-pixel-buffer.h		\
	$(srcdir)/cogl2-experimental.h		\
	$(srcdir)/cogl-macros.h			\
	$(srcdir)/cogl-fence.h       		\
	$(srcdir)/cogl-version.h		\
	$(srcdir)/cogl-error.h			\
	$(NULL)

cogl_nodist_experimental_h = \
	$(NULL)

# nop driver
cogl_driver_sources = \
	$(srcdir)/driver/nop/cogl-driver-nop.c \
	$(srcdir)/driver/nop/cogl-framebuffer-nop-private.h \
	$(srcdir)/driver/nop/cogl-framebuffer-nop.c \
	$(srcdir)/driver/nop/cogl-attribute-nop-private.h \
	$(srcdir)/driver/nop/cogl-attribute-nop.c \
	$(srcdir)/driver/nop/cogl-clip-stack-nop-private.h \
	$(srcdir)/driver/nop/cogl-clip-stack-nop.c \
	$(srcdir)/driver/nop/cogl-texture-2d-nop-private.h \
	$(srcdir)/driver/nop/cogl-texture-2d-nop.c \
	$(NULL)

# gl driver sources
cogl_gl_prototypes_h = \
	$(srcdir)/gl-prototypes/cogl-gles2-functions.h		\
	$(srcdir)/gl-prototypes/cogl-core-functions.h		\
	$(srcdir)/gl-prototypes/cogl-in-gles-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-in-gles2-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-glsl-functions.h		\
	$(NULL)

cogl_driver_sources += \
	$(srcdir)/driver/gl/cogl-util-gl-private.h \
	$(srcdir)/driver/gl/cogl-util-gl.c \
	$(srcdir)/driver/gl/cogl-framebuffer-gl-private.h \
	$(srcdir)/driver/gl/cogl-framebuffer-gl.c \
	$(srcdir)/driver/gl/cogl-texture-gl-private.h \
	$(srcdir)/driver/gl/cogl-texture-gl.c \
	$(srcdir)/driver/gl/cogl-texture-2d-gl-private.h \
	$(srcdir)/driver/gl/cogl-texture-2d-gl.c \
	$(srcdir)/driver/gl/cogl-attribute-gl-private.h \
	$(srcdir)/driver/gl/cogl-attribute-gl.c \
	$(srcdir)/driver/gl/cogl-clip-stack-gl-private.h \
	$(srcdir)/driver/gl/cogl-clip-stack-gl.c \
	$(srcdir)/driver/gl/cogl-buffer-gl-private.h \
	$(srcdir)/driver/gl/cogl-buffer-gl.c \
	$(srcdir)/driver/gl/cogl-pipeline-opengl.c \
	$(srcdir)/driver/gl/cogl-pipeline-opengl-private.h \
	$(srcdir)/driver/gl/cogl-pipeline-fragend-glsl.c \
	$(srcdir)/driver/gl/cogl-pipeline-fragend-glsl-private.h \
	$(srcdir)/driver/gl/gl/cogl-pipeline-fragend-arbfp.c \
	$(srcdir)/driver/gl/gl/cogl-pipeline-fragend-arbfp-private.h \
	$(srcdir)/driver/gl/gl/cogl-pipeline-progend-fixed-arbfp.c \
	$(srcdir)/driver/gl/gl/cogl-pipeline-progend-fixed-arbfp-private.h \
	$(srcdir)/driver/gl/cogl-pipeline-fragend-fixed.c \
	$(srcdir)/driver/gl/cogl-pipeline-fragend-fixed-private.h \
	$(srcdir)/driver/gl/cogl-pipeline-vertend-glsl.c \
	$(srcdir)/driver/gl/cogl-pipeline-vertend-glsl-private.h \
	$(srcdir)/driver/gl/cogl-pipeline-vertend-fixed.c \
	$(srcdir)/driver/gl/cogl-pipeline-vertend-fixed-private.h \
	$(srcdir)/driver/gl/cogl-pipeline-progend-fixed.c \
	$(srcdir)/driver/gl/cogl-pipeline-progend-fixed-private.h \
	$(srcdir)/driver/gl/cogl-pipeline-progend-glsl.c \
	$(srcdir)/driver/gl/cogl-pipeline-progend-glsl-private.h \
	$(NULL)

if COGL_DRIVER_GL_SUPPORTED
cogl_driver_sources += \
	$(srcdir)/driver/gl/gl/cogl-driver-gl.c \
	$(srcdir)/driver/gl/gl/cogl-texture-driver-gl.c \
	$(NULL)
endif

if COGL_DRIVER_GLES_SUPPORTED
cogl_driver_sources += \
	$(srcdir)/driver/gl/gles/cogl-driver-gles.c \
	$(srcdir)/driver/gl/gles/cogl-texture-driver-gles.c	\
	$(NULL)
endif

# winsys sources, common to all backends
cogl_winsys_common_sources = \
	$(srcdir)/winsys/cogl-winsys-private.h \
	$(srcdir)/winsys/cogl-winsys.c \
	$(NULL)

# sources
cogl_sources_c = \
	$(cogl_driver_sources)				\
	$(cogl_winsys_common_sources)			\
	$(srcdir)/cogl-private.h			\
	$(srcdir)/cogl-i18n-private.h 			\
	$(srcdir)/cogl-debug.h 				\
	$(srcdir)/cogl-debug-options.h			\
	$(srcdir)/cogl-gpu-info.c			\
	$(srcdir)/cogl-gpu-info-private.h		\
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
	$(srcdir)/cogl-driver.h				\
	$(srcdir)/cogl.c				\
	$(srcdir)/cogl-object-private.h			\
	$(srcdir)/cogl-object.h				\
	$(srcdir)/cogl-object.c				\
	$(srcdir)/cogl-util.h 				\
	$(srcdir)/cogl-util.c 				\
	$(srcdir)/cogl-bitmap-private.h 		\
	$(srcdir)/cogl-bitmap.c 			\
	$(srcdir)/cogl-bitmap-conversion.c 		\
	$(srcdir)/cogl-bitmap-packing.h			\
	$(srcdir)/cogl-primitives-private.h 		\
	$(srcdir)/cogl-primitives.h 			\
	$(srcdir)/cogl-primitives.c 			\
	$(srcdir)/cogl-bitmap-pixbuf.c 			\
	$(srcdir)/cogl-clip-stack.h 			\
	$(srcdir)/cogl-clip-stack.c			\
	$(srcdir)/cogl-clip-state-private.h		\
	$(srcdir)/cogl-clip-state.h			\
	$(srcdir)/cogl-clip-state.c			\
	$(srcdir)/cogl-feature-private.h                \
	$(srcdir)/cogl-feature-private.c                \
	$(srcdir)/cogl-color-private.h    		\
	$(srcdir)/cogl-color.c				\
	$(srcdir)/cogl-buffer-private.h 		\
	$(srcdir)/cogl-buffer.c				\
	$(srcdir)/cogl-pixel-buffer-private.h		\
	$(srcdir)/cogl-pixel-buffer.c			\
	$(srcdir)/cogl-index-buffer-private.h		\
	$(srcdir)/cogl-index-buffer.c			\
	$(srcdir)/cogl-attribute-buffer-private.h	\
	$(srcdir)/cogl-attribute-buffer.c		\
	$(srcdir)/cogl-indices-private.h		\
	$(srcdir)/cogl-indices.c			\
	$(srcdir)/cogl-attribute-private.h		\
	$(srcdir)/cogl-attribute.c			\
	$(srcdir)/cogl-primitive-private.h		\
	$(srcdir)/cogl-primitive.c			\
	$(srcdir)/cogl-matrix.c				\
	$(srcdir)/cogl-vector.c				\
	$(srcdir)/cogl-euler.c				\
	$(srcdir)/cogl-quaternion-private.h 		\
	$(srcdir)/cogl-quaternion.c			\
	$(srcdir)/cogl-matrix-private.h			\
	$(srcdir)/cogl-matrix-stack.c			\
	$(srcdir)/cogl-matrix-stack-private.h		\
	$(srcdir)/cogl-depth-state.c			\
	$(srcdir)/cogl-depth-state-private.h		\
	$(srcdir)/cogl-node.c				\
	$(srcdir)/cogl-node-private.h			\
	$(srcdir)/cogl-pipeline.c			\
	$(srcdir)/cogl-pipeline-private.h		\
	$(srcdir)/cogl-pipeline-layer.c			\
	$(srcdir)/cogl-pipeline-layer-private.h		\
	$(srcdir)/cogl-pipeline-state.c			\
	$(srcdir)/cogl-pipeline-layer-state-private.h	\
	$(srcdir)/cogl-pipeline-layer-state.c		\
	$(srcdir)/cogl-pipeline-state-private.h		\
	$(srcdir)/cogl-pipeline-debug.c			\
        $(srcdir)/cogl-glsl-shader.c                    \
        $(srcdir)/cogl-glsl-shader-private.h            \
        $(srcdir)/cogl-glsl-shader-boilerplate.h        \
	$(srcdir)/cogl-pipeline-snippet-private.h	\
	$(srcdir)/cogl-pipeline-snippet.c		\
	$(srcdir)/cogl-pipeline-cache.h			\
	$(srcdir)/cogl-pipeline-cache.c			\
	$(srcdir)/cogl-pipeline-hash-table.h		\
	$(srcdir)/cogl-pipeline-hash-table.c		\
	$(srcdir)/cogl-sampler-cache.c			\
	$(srcdir)/cogl-sampler-cache-private.h		\
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
	$(srcdir)/cogl-meta-texture.c			\
	$(srcdir)/cogl-primitive-texture.c		\
	$(srcdir)/cogl-blit.h				\
	$(srcdir)/cogl-blit.c				\
	$(srcdir)/cogl-spans.h				\
	$(srcdir)/cogl-spans.c				\
	$(srcdir)/cogl-journal-private.h		\
	$(srcdir)/cogl-journal.c			\
	$(srcdir)/cogl-frame-info-private.h		\
	$(srcdir)/cogl-frame-info.c			\
	$(srcdir)/cogl-framebuffer-private.h		\
	$(srcdir)/cogl-framebuffer.c 			\
	$(srcdir)/cogl-onscreen-private.h		\
	$(srcdir)/cogl-onscreen.c 			\
	$(srcdir)/cogl-output-private.h			\
	$(srcdir)/cogl-output.c				\
	$(srcdir)/cogl-profile.h 			\
	$(srcdir)/cogl-profile.c 			\
	$(srcdir)/cogl-flags.h				\
	$(srcdir)/cogl-bitmask.h                        \
	$(srcdir)/cogl-bitmask.c                        \
	$(srcdir)/cogl-gtype-private.h                  \
	$(srcdir)/cogl-point-in-poly-private.h       	\
	$(srcdir)/cogl-point-in-poly.c       		\
	$(srcdir)/cogl-list.c				\
	$(srcdir)/cogl-list.h				\
	$(srcdir)/winsys/cogl-winsys-stub-private.h	\
	$(srcdir)/winsys/cogl-winsys-stub.c		\
	$(srcdir)/cogl-config-private.h			\
	$(srcdir)/cogl-config.c				\
	$(srcdir)/cogl-boxed-value.h			\
	$(srcdir)/cogl-boxed-value.c			\
	$(srcdir)/cogl-snippet-private.h		\
	$(srcdir)/cogl-snippet.c			\
	$(srcdir)/cogl-poll-private.h			\
	$(srcdir)/cogl-poll.c				\
	$(srcdir)/gl-prototypes/cogl-all-functions.h	\
	$(srcdir)/gl-prototypes/cogl-gles1-functions.h	\
	$(srcdir)/gl-prototypes/cogl-gles2-functions.h	\
	$(srcdir)/gl-prototypes/cogl-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-in-gles-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-in-gles1-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-in-gles2-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-fixed-functions.h	\
	$(srcdir)/gl-prototypes/cogl-glsl-functions.h	\
	$(srcdir)/cogl-memory-stack-private.h		\
	$(srcdir)/cogl-memory-stack.c			\
	$(srcdir)/cogl-magazine-private.h		\
	$(srcdir)/cogl-magazine.c			\
	$(srcdir)/cogl-gles2-context-private.h		\
	$(srcdir)/cogl-gles2-context.c			\
	$(srcdir)/cogl-error-private.h			\
	$(srcdir)/cogl-error.c				\
	$(srcdir)/cogl-closure-list-private.h		\
	$(srcdir)/cogl-closure-list.c			\
	$(srcdir)/cogl-fence.c				\
	$(srcdir)/cogl-fence-private.h			\
	$(srcdir)/deprecated/cogl-fixed.c		    	\
	$(srcdir)/deprecated/cogl-vertex-buffer-private.h	\
	$(srcdir)/deprecated/cogl-vertex-buffer.c		\
	$(srcdir)/deprecated/cogl-material-compat.c		\
	$(srcdir)/deprecated/cogl-program.c			\
	$(srcdir)/deprecated/cogl-program-private.h		\
	$(srcdir)/deprecated/cogl-auto-texture.c		\
	$(srcdir)/deprecated/cogl-shader-private.h		\
	$(srcdir)/deprecated/cogl-shader.c			\
	$(srcdir)/deprecated/cogl-clutter.c       		\
	$(NULL)

if USE_GLIB
cogl_experimental_h += \
	$(srcdir)/cogl-glib-source.h
cogl_sources_c += \
	$(srcdir)/cogl-glib-source.c
endif

if SUPPORT_XLIB
cogl_deprecated_h += \
	$(srcdir)/deprecated/cogl-clutter-xlib.h
cogl_1_public_h += \
	$(srcdir)/cogl-xlib-renderer.h

cogl_experimental_h += \
       $(srcdir)/winsys/cogl-texture-pixmap-x11.h \
       $(srcdir)/cogl-xlib.h

cogl_sources_c += \
	$(srcdir)/cogl-x11-renderer-private.h \
	$(srcdir)/cogl-xlib-renderer-private.h \
	$(srcdir)/cogl-xlib-renderer.c \
	$(srcdir)/cogl-xlib.c \
	$(srcdir)/cogl-xlib-private.h \
	$(srcdir)/winsys/cogl-texture-pixmap-x11.c \
	$(srcdir)/winsys/cogl-texture-pixmap-x11-private.h
endif
if SUPPORT_GLX
cogl_sources_c += \
	$(srcdir)/cogl-glx-renderer-private.h \
	$(srcdir)/cogl-glx-display-private.h \
	$(srcdir)/winsys/cogl-winsys-glx-feature-functions.h \
	$(srcdir)/winsys/cogl-winsys-glx-private.h \
	$(srcdir)/winsys/cogl-winsys-glx.c
endif
if SUPPORT_WGL
cogl_experimental_h += \
	$(srcdir)/cogl-win32-renderer.h

cogl_sources_c += \
	$(srcdir)/cogl-win32-renderer.c \
	$(srcdir)/winsys/cogl-winsys-wgl-private.h \
	$(srcdir)/winsys/cogl-winsys-wgl.c \
	$(srcdir)/winsys/cogl-winsys-wgl-feature-functions.h
endif
if SUPPORT_WAYLAND_EGL_SERVER
cogl_experimental_h += \
	$(srcdir)/cogl-wayland-server.h
endif
if SUPPORT_EGL_PLATFORM_WAYLAND
cogl_experimental_h += \
	$(srcdir)/cogl-wayland-renderer.h \
	$(srcdir)/cogl-wayland-client.h
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-wayland.c \
	$(srcdir)/winsys/cogl-winsys-egl-wayland-private.h
endif
if SUPPORT_EGL_PLATFORM_KMS
cogl_experimental_h += \
	$(srcdir)/cogl-kms-renderer.h \
	$(srcdir)/cogl-kms-display.h
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-kms.c \
	$(srcdir)/winsys/cogl-winsys-egl-kms-private.h
endif
if SUPPORT_EGL_PLATFORM_XLIB
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-x11.c \
	$(srcdir)/winsys/cogl-winsys-egl-x11-private.h
endif
if SUPPORT_EGL_PLATFORM_POWERVR_NULL
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-null.c \
	$(srcdir)/winsys/cogl-winsys-egl-null-private.h
endif
if SUPPORT_EGL_PLATFORM_GDL
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-gdl.c \
	$(srcdir)/winsys/cogl-winsys-egl-gdl-private.h
endif
if SUPPORT_EGL_PLATFORM_ANDROID
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-android.c \
	$(srcdir)/winsys/cogl-winsys-egl-android-private.h
endif
if SUPPORT_EGL
cogl_experimental_h += \
       $(srcdir)/cogl-egl.h
cogl_nodist_experimental_h += \
       $(builddir)/cogl-egl-defines.h

cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-egl.c \
       $(srcdir)/winsys/cogl-winsys-egl-feature-functions.h \
       $(srcdir)/winsys/cogl-winsys-egl-private.h
endif
if SUPPORT_SDL
cogl_experimental_h += $(srcdir)/cogl-sdl.h
cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-sdl-private.h \
       $(srcdir)/winsys/cogl-winsys-sdl.c \
       $(srcdir)/cogl-sdl.c
endif
if SUPPORT_SDL2
cogl_experimental_h += $(srcdir)/cogl-sdl.h
cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-sdl-private.h \
       $(srcdir)/winsys/cogl-winsys-sdl2.c \
       $(srcdir)/cogl-sdl.c
endif

EXTRA_DIST += stb_image.c

# glib-mkenums rules
glib_enum_h = cogl-enum-types.h
glib_enum_c = cogl-enum-types.c
glib_enum_headers = $(cogl_1_public_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

lib_LTLIBRARIES += libcogl.la

libcogl_la_LIBADD = $(LIBM) $(COGL_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
if !USE_GLIB
libcogl_la_LIBADD += $(top_builddir)/deps/glib/libglib.la
libcogl_la_LIBADD += $(top_builddir)/deps/gmodule/libgmodule.la
endif
if UNIT_TESTS
libcogl_la_LIBADD += $(top_builddir)/test-fixtures/libtest-fixtures.la
endif
if BUILD_COGL_PATH
libcogl_la_LIBADD += $(top_builddir)/cogl-path/libcogl-path.la
endif
# XXX: The aim is to eventually get rid of all private API exports
# for cogl-pango.
libcogl_la_LDFLAGS = \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-export-dynamic \
	-export-symbols-regex "^(cogl|_cogl_debug_flags|_cogl_atlas_new|_cogl_atlas_add_reorganize_callback|_cogl_atlas_reserve_space|_cogl_callback|_cogl_util_get_eye_planes_for_screen_poly|_cogl_atlas_texture_remove_reorganize_callback|_cogl_atlas_texture_add_reorganize_callback|_cogl_texture_foreach_sub_texture_in_region|_cogl_profile_trace_message|_cogl_context_get_default|_cogl_framebuffer_get_stencil_bits|_cogl_clip_stack_push_rectangle|_cogl_framebuffer_get_modelview_stack|_cogl_object_default_unref|_cogl_pipeline_foreach_layer_internal|_cogl_clip_stack_push_primitive|_cogl_buffer_unmap_for_fill_or_fallback|_cogl_framebuffer_draw_primitive|_cogl_debug_instances|_cogl_framebuffer_get_projection_stack|_cogl_pipeline_layer_get_texture|_cogl_buffer_map_for_fill_or_fallback|_cogl_framebuffer_get_clip_state|_cogl_texture_can_hardware_repeat|_cogl_pipeline_prune_to_n_layers|_cogl_primitive_draw|test_|unit_test_).*"

libcogl_la_SOURCES = $(cogl_sources_c)
nodist_libcogl_la_SOURCES = $(BUILT_SOURCES)

# Cogl installed headers
cogl_headers = \
	$(cogl_1_public_h) \
	$(srcdir)/cogl-deprecated.h \
	$(srcdir)/cogl-pango.h \
	$(NULL)

cogldeprecatedincludedir = $(includedir)/cogl/cogl/deprecated
cogldeprecatedinclude_HEADERS = $(cogl_deprecated_h)

coglincludedir = $(includedir)/cogl/cogl
coglinclude_HEADERS = $(cogl_headers) $(cogl_experimental_h)
nodist_coglinclude_HEADERS = $(cogl_nodist_experimental_h) cogl-defines.h cogl-enum-types.h

cogl_proto_includedir = $(includedir)/cogl/cogl/gl-prototypes
cogl_proto_include_HEADERS = $(cogl_gl_prototypes_h)

dist-hook: ../build/win32/vs9/cogl.vcproj ../build/win32/vs10/cogl.vcxproj ../build/win32/vs10/cogl.vcxproj.filters ../build/win32/gen-enums.bat

# I know those filters below don't look nice, but this is to ensure the right files are in the Project files only *once*
../build/win32/vs9/cogl.vcproj: $(top_srcdir)/build/win32/vs9/cogl.vcprojin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-egl-*.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '   <File RelativePath="..\..\..\cogl\'$$F'" />' \
			;; \
		esac; \
	done >cogl.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/cogl.vcprojin >$@
	rm cogl.sourcefiles

../build/win32/vs10/cogl.vcxproj: $(top_srcdir)/build/win32/vs10/cogl.vcxprojin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-egl-*.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '    <ClCompile Include="..\..\..\cogl\'$$F'" />' \
			;; \
		esac; \
	done >cogl.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/cogl.vcxprojin >$@
	rm cogl.vs10.sourcefiles

../build/win32/vs10/cogl.vcxproj.filters: $(top_srcdir)/build/win32/vs10/cogl.vcxproj.filtersin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-egl-*.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '    <ClCompile Include="..\..\..\cogl\'$$F'"><Filter>Sources</Filter></ClCompile>' \
			;; \
		esac; \
	done >cogl.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/cogl.vcxproj.filtersin >$@
	rm cogl.vs10.sourcefiles.filters

cogl.vsenums_h:
	echo 'perl %1\bin\glib-mkenums --template ../../cogl/cogl-enum-types.h.in ' >vsenums_h.temp1
	for F in `echo $(cogl_1_public_h) $(srcdir)/cogl-win32-renderer.h`; do \
		case $$F in \
		*xlib*.h|*wayland*.h) ;; \
		*.h) echo '../../cogl'$$F' '	\
			;;	\
		esac;	\
	done >>vsenums_h.temp1
	cat vsenums_h.temp1 | sed 's_/cogl./_/cogl/_' >vsenums_h.temp
	cat vsenums_h.temp | tr -d '\n' >>$@
	echo '> ..\..\cogl\cogl-enum-types.h' >>$@
	rm vsenums_h.temp1
	rm vsenums_h.temp

cogl.vsenums_c:
	echo 'perl %1\bin\glib-mkenums --template ../../cogl/cogl-enum-types.c.in ' >vsenums_c.temp1
	for F in `echo $(cogl_1_public_h) $(srcdir)/cogl-win32-renderer.h`; do \
		case $$F in \
		*xlib*.h|*wayland*.h) ;; \
		*.h) echo '../../cogl'$$F' '	\
			;;	\
		esac;	\
	done >>vsenums_c.temp1
	cat vsenums_c.temp1 | sed 's_/cogl./_/cogl/_' >vsenums_c.temp
	cat vsenums_c.temp | tr -d '\n' >>$@
	echo '> ..\..\cogl\cogl-enum-types.c' >>$@
	rm vsenums_c.temp1
	rm vsenums_c.temp

../build/win32/gen-enums.bat: cogl.vsenums_h cogl.vsenums_c
	echo 'if exist ..\..\cogl\cogl-enum-types.h goto DONE_COGL_ENUMS_H' >$@
	cat cogl.vsenums_h >>$@
	echo ':DONE_COGL_ENUMS_H' >>$@
	echo 'if exist ..\..\cogl\cogl-enum-types.c goto DONE_COGL_ENUMS_C' >>$@
	cat cogl.vsenums_c >>$@
	echo ':DONE_COGL_ENUMS_C' >>$@
	rm cogl.vsenums_h
	rm cogl.vsenums_c

EXTRA_DIST += \
	cogl-defines.h.win32.in	\
	cogl-defines.h.win32	\
	cogl-defines.h.win32_SDL.in	\
	cogl-defines.h.win32_SDL	\
	cogl-gl-header.h.win32	\
	cogl.symbols	\
	cogl.rc.in	\
	cogl.rc

DISTCLEANFILES += ../build/win32/vs9/cogl.vcproj ../build/win32/vs10/cogl.vcxproj ../build/win32/vs10/cogl.vcxproj.filters ../build/win32/gen-enums.bat cogl.rc

-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =

if HAVE_INTROSPECTION
Cogl-1.0.gir: libcogl.la Makefile

Cogl_1_0_gir_NAMESPACE = Cogl
Cogl_1_0_gir_VERSION = 1.0
Cogl_1_0_gir_LIBS = libcogl.la
if UNIT_TESTS
Cogl_1_0_gir_LIBS += $(top_builddir)/test-fixtures/libtest-fixtures.la
endif
Cogl_1_0_gir_FILES = $(cogl_1_public_h) cogl-enum-types.h
if BUILD_COGL_PATH
Cogl_1_0_gir_FILES += \
	$(top_builddir)/cogl-path/cogl-path-enum-types.h \
	$(top_srcdir)/cogl-path/cogl-path-types.h \
	$(top_srcdir)/cogl-path/cogl1-path-functions.h
endif
Cogl_1_0_gir_CFLAGS = $(AM_CPPFLAGS) $(COGL_DEP_CFLAGS) -UCOGL_ENABLE_EXPERIMENTAL_API -UCOGL_ENABLE_EXPERIMENTAL_2_0_API -UCOGL_COMPILATION -D__COGL_H_INSIDE__ -D__COGL_XLIB_H_INSIDE__
Cogl_1_0_gir_INCLUDES = GL-1.0 GObject-2.0
Cogl_1_0_gir_EXPORT_PACKAGES = cogl-1.0
Cogl_1_0_gir_SCANNERFLAGS = --warn-all --c-include='cogl/cogl.h'

INTROSPECTION_GIRS += Cogl-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
