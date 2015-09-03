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
	deprecated/cogl-clip-state.h 		\
	deprecated/cogl-fixed.h 		\
	deprecated/cogl-material-compat.h 	\
	deprecated/cogl-vertex-buffer.h 	\
	deprecated/cogl-shader.h 		\
	deprecated/cogl-clutter.h       	\
	deprecated/cogl-type-casts.h       	\
	deprecated/cogl-framebuffer-deprecated.h \
	deprecated/cogl-texture-deprecated.h	\
	deprecated/cogl-auto-texture.h	\
	$(NULL)

# public 1.x api headers
cogl_1_public_h = \
	$(cogl_deprecated_h)			\
	cogl1-context.h 		\
	cogl-bitmap.h 		\
	cogl-color.h 			\
	cogl-matrix.h 		\
	cogl-offscreen.h 		\
	cogl-primitives.h 		\
	cogl-texture.h 		\
	cogl-types.h 			\
	cogl.h			\
	$(NULL)

# experimental 2.0 api headers
# Note: we don't run glib-mkenums over these headers
cogl_experimental_h = \
	cogl-object.h 		\
	cogl-renderer.h 		\
	cogl-swap-chain.h 		\
	cogl-onscreen-template.h 	\
	cogl-display.h 		\
	cogl-context.h 		\
	cogl-pipeline.h 		\
	cogl-pipeline-state.h 	\
	cogl-pipeline-layer-state.h 	\
	cogl-snippet.h		\
	cogl-gles2.h			\
	cogl-gles2-types.h		\
	cogl-index-buffer.h 		\
	cogl-attribute-buffer.h 	\
	cogl-indices.h 		\
	cogl-attribute.h 		\
	cogl-primitive.h 		\
	cogl-framebuffer.h		\
	cogl-onscreen.h		\
	cogl-frame-info.h		\
	cogl-vector.h 		\
	cogl-euler.h 			\
	cogl-output.h			\
	cogl-quaternion.h 		\
	cogl-matrix-stack.h		\
	cogl-poll.h			\
	cogl-texture-3d.h             \
	cogl-texture-2d.h             \
	cogl-texture-2d-gl.h 		\
	cogl-texture-rectangle.h      \
	cogl-texture-2d-sliced.h      \
	cogl-sub-texture.h            \
	cogl-atlas-texture.h          \
	cogl-meta-texture.h		\
	cogl-primitive-texture.h	\
	cogl-depth-state.h 		\
	cogl-buffer.h 		\
	cogl-pixel-buffer.h		\
	cogl2-experimental.h		\
	cogl-macros.h			\
	cogl-fence.h       		\
	cogl-version.h		\
	cogl-error.h			\
	$(NULL)

cogl_additional_experimental_h = \
	cogl-bitmap.h			\
	cogl-color.h			\
	cogl-matrix.h			\
	cogl-texture.h		\
	cogl-types.h			\
	cogl-gtype-private.h		\
	$(NULL)

cogl_nodist_experimental_h = \
	$(NULL)

# nop driver
cogl_driver_sources = \
	driver/nop/cogl-driver-nop.c \
	driver/nop/cogl-framebuffer-nop-private.h \
	driver/nop/cogl-framebuffer-nop.c \
	driver/nop/cogl-attribute-nop-private.h \
	driver/nop/cogl-attribute-nop.c \
	driver/nop/cogl-clip-stack-nop-private.h \
	driver/nop/cogl-clip-stack-nop.c \
	driver/nop/cogl-texture-2d-nop-private.h \
	driver/nop/cogl-texture-2d-nop.c \
	$(NULL)

# gl driver sources
cogl_gl_prototypes_h = \
	gl-prototypes/cogl-gles2-functions.h		\
	gl-prototypes/cogl-core-functions.h		\
	gl-prototypes/cogl-in-gles-core-functions.h	\
	gl-prototypes/cogl-in-gles2-core-functions.h	\
	gl-prototypes/cogl-glsl-functions.h		\
	$(NULL)

cogl_driver_sources += \
	driver/gl/cogl-util-gl-private.h \
	driver/gl/cogl-util-gl.c \
	driver/gl/cogl-framebuffer-gl-private.h \
	driver/gl/cogl-framebuffer-gl.c \
	driver/gl/cogl-texture-gl-private.h \
	driver/gl/cogl-texture-gl.c \
	driver/gl/cogl-texture-2d-gl-private.h \
	driver/gl/cogl-texture-2d-gl.c \
	driver/gl/cogl-attribute-gl-private.h \
	driver/gl/cogl-attribute-gl.c \
	driver/gl/cogl-clip-stack-gl-private.h \
	driver/gl/cogl-clip-stack-gl.c \
	driver/gl/cogl-buffer-gl-private.h \
	driver/gl/cogl-buffer-gl.c \
	driver/gl/cogl-pipeline-opengl.c \
	driver/gl/cogl-pipeline-opengl-private.h \
	driver/gl/cogl-pipeline-fragend-glsl.c \
	driver/gl/cogl-pipeline-fragend-glsl-private.h \
	driver/gl/gl/cogl-pipeline-fragend-arbfp.c \
	driver/gl/gl/cogl-pipeline-fragend-arbfp-private.h \
	driver/gl/gl/cogl-pipeline-progend-fixed-arbfp.c \
	driver/gl/gl/cogl-pipeline-progend-fixed-arbfp-private.h \
	driver/gl/cogl-pipeline-fragend-fixed.c \
	driver/gl/cogl-pipeline-fragend-fixed-private.h \
	driver/gl/cogl-pipeline-vertend-glsl.c \
	driver/gl/cogl-pipeline-vertend-glsl-private.h \
	driver/gl/cogl-pipeline-vertend-fixed.c \
	driver/gl/cogl-pipeline-vertend-fixed-private.h \
	driver/gl/cogl-pipeline-progend-fixed.c \
	driver/gl/cogl-pipeline-progend-fixed-private.h \
	driver/gl/cogl-pipeline-progend-glsl.c \
	driver/gl/cogl-pipeline-progend-glsl-private.h \
	$(NULL)

if COGL_DRIVER_GL_SUPPORTED
cogl_driver_sources += \
	driver/gl/gl/cogl-driver-gl.c \
	driver/gl/gl/cogl-texture-driver-gl.c \
	$(NULL)
endif

if COGL_DRIVER_GLES_SUPPORTED
cogl_driver_sources += \
	driver/gl/gles/cogl-driver-gles.c \
	driver/gl/gles/cogl-texture-driver-gles.c	\
	$(NULL)
endif

# winsys sources, common to all backends
cogl_winsys_common_sources = \
	winsys/cogl-winsys-private.h \
	winsys/cogl-winsys.c \
	$(NULL)

# sources
cogl_sources_c = \
	$(cogl_driver_sources)				\
	$(cogl_winsys_common_sources)			\
	cogl-private.h			\
	cogl-i18n-private.h 			\
	cogl-debug.h 				\
	cogl-debug-options.h			\
	cogl-gpu-info.c			\
	cogl-gpu-info-private.h		\
	cogl-context-private.h		\
	cogl-context.c			\
	cogl-renderer-private.h		\
	cogl-renderer.h			\
	cogl-renderer.c			\
	cogl-swap-chain-private.h		\
	cogl-swap-chain.h			\
	cogl-swap-chain.c			\
	cogl-onscreen-template-private.h 	\
	cogl-onscreen-template.h 		\
	cogl-onscreen-template.c 		\
	cogl-display-private.h		\
	cogl-display.h			\
	cogl-display.c			\
	cogl-driver.h				\
	cogl.c				\
	cogl-object-private.h			\
	cogl-object.h				\
	cogl-object.c				\
	cogl-util.h 				\
	cogl-util.c 				\
	cogl-bitmap-private.h 		\
	cogl-bitmap.c 			\
	cogl-bitmap-conversion.c 		\
	cogl-bitmap-packing.h			\
	cogl-primitives-private.h 		\
	cogl-primitives.h 			\
	cogl-primitives.c 			\
	cogl-bitmap-pixbuf.c 			\
	cogl-clip-stack.h 			\
	cogl-clip-stack.c			\
	cogl-feature-private.h                \
	cogl-feature-private.c                \
	cogl-color-private.h    		\
	cogl-color.c				\
	cogl-buffer-private.h 		\
	cogl-buffer.c				\
	cogl-pixel-buffer-private.h		\
	cogl-pixel-buffer.c			\
	cogl-index-buffer-private.h		\
	cogl-index-buffer.c			\
	cogl-attribute-buffer-private.h	\
	cogl-attribute-buffer.c		\
	cogl-indices-private.h		\
	cogl-indices.c			\
	cogl-attribute-private.h		\
	cogl-attribute.c			\
	cogl-primitive-private.h		\
	cogl-primitive.c			\
	cogl-matrix.c				\
	cogl-vector.c				\
	cogl-euler.c				\
	cogl-quaternion-private.h 		\
	cogl-quaternion.c			\
	cogl-matrix-private.h			\
	cogl-matrix-stack.c			\
	cogl-matrix-stack-private.h		\
	cogl-depth-state.c			\
	cogl-depth-state-private.h		\
	cogl-node.c				\
	cogl-node-private.h			\
	cogl-pipeline.c			\
	cogl-pipeline-private.h		\
	cogl-pipeline-layer.c			\
	cogl-pipeline-layer-private.h		\
	cogl-pipeline-state.c			\
	cogl-pipeline-layer-state-private.h	\
	cogl-pipeline-layer-state.c		\
	cogl-pipeline-state-private.h		\
	cogl-pipeline-debug.c			\
        cogl-glsl-shader.c                    \
        cogl-glsl-shader-private.h            \
        cogl-glsl-shader-boilerplate.h        \
	cogl-pipeline-snippet-private.h	\
	cogl-pipeline-snippet.c		\
	cogl-pipeline-cache.h			\
	cogl-pipeline-cache.c			\
	cogl-pipeline-hash-table.h		\
	cogl-pipeline-hash-table.c		\
	cogl-sampler-cache.c			\
	cogl-sampler-cache-private.h		\
	cogl-blend-string.c			\
	cogl-blend-string.h			\
	cogl-debug.c				\
	cogl-sub-texture-private.h            \
	cogl-texture-private.h		\
	cogl-texture-2d-private.h             \
	cogl-texture-2d-sliced-private.h 	\
	cogl-texture-3d-private.h             \
	cogl-texture-driver.h			\
	cogl-sub-texture.c                    \
	cogl-texture.c			\
	cogl-texture-2d.c                     \
	cogl-texture-2d-sliced.c		\
	cogl-texture-3d.c                     \
	cogl-texture-rectangle-private.h      \
	cogl-texture-rectangle.c              \
	cogl-rectangle-map.h                  \
	cogl-rectangle-map.c                  \
	cogl-atlas.h                          \
	cogl-atlas.c                          \
	cogl-atlas-texture-private.h          \
	cogl-atlas-texture.c                  \
	cogl-meta-texture.c			\
	cogl-primitive-texture.c		\
	cogl-blit.h				\
	cogl-blit.c				\
	cogl-spans.h				\
	cogl-spans.c				\
	cogl-journal-private.h		\
	cogl-journal.c			\
	cogl-frame-info-private.h		\
	cogl-frame-info.c			\
	cogl-framebuffer-private.h		\
	cogl-framebuffer.c 			\
	cogl-onscreen-private.h		\
	cogl-onscreen.c 			\
	cogl-output-private.h			\
	cogl-output.c				\
	cogl-profile.h 			\
	cogl-profile.c 			\
	cogl-flags.h				\
	cogl-bitmask.h                        \
	cogl-bitmask.c                        \
	cogl-gtype.c                          \
	cogl-gtype-private.h                  \
	cogl-point-in-poly-private.h       	\
	cogl-point-in-poly.c       		\
	cogl-list.c				\
	cogl-list.h				\
	winsys/cogl-winsys-stub-private.h	\
	winsys/cogl-winsys-stub.c		\
	cogl-config-private.h			\
	cogl-config.c				\
	cogl-boxed-value.h			\
	cogl-boxed-value.c			\
	cogl-snippet-private.h		\
	cogl-snippet.c			\
	cogl-poll-private.h			\
	cogl-poll.c				\
	gl-prototypes/cogl-all-functions.h	\
	gl-prototypes/cogl-gles1-functions.h	\
	gl-prototypes/cogl-gles2-functions.h	\
	gl-prototypes/cogl-core-functions.h	\
	gl-prototypes/cogl-in-gles-core-functions.h	\
	gl-prototypes/cogl-in-gles1-core-functions.h	\
	gl-prototypes/cogl-in-gles2-core-functions.h	\
	gl-prototypes/cogl-fixed-functions.h	\
	gl-prototypes/cogl-glsl-functions.h	\
	cogl-memory-stack-private.h		\
	cogl-memory-stack.c			\
	cogl-magazine-private.h		\
	cogl-magazine.c			\
	cogl-gles2-context-private.h		\
	cogl-gles2-context.c			\
	cogl-error-private.h			\
	cogl-error.c				\
	cogl-closure-list-private.h		\
	cogl-closure-list.c			\
	cogl-fence.c				\
	cogl-fence-private.h			\
	deprecated/cogl-clip-state.c			\
	deprecated/cogl-fixed.c		    	\
	deprecated/cogl-vertex-buffer-private.h	\
	deprecated/cogl-vertex-buffer.c		\
	deprecated/cogl-material-compat.c		\
	deprecated/cogl-program.c			\
	deprecated/cogl-program-private.h		\
	deprecated/cogl-auto-texture.c		\
	deprecated/cogl-shader-private.h		\
	deprecated/cogl-shader.c			\
	deprecated/cogl-clutter.c       		\
	deprecated/cogl-framebuffer-deprecated.c      \
	deprecated/cogl-texture-deprecated.c       	\
	$(NULL)

if USE_GLIB
cogl_experimental_h += cogl-glib-source.h
cogl_sources_c += cogl-glib-source.c
endif

if SUPPORT_XLIB
cogl_deprecated_h += deprecated/cogl-clutter-xlib.h
cogl_1_public_h += cogl-xlib-renderer.h

cogl_experimental_h += \
	winsys/cogl-texture-pixmap-x11.h \
	cogl-xlib.h

cogl_sources_c += \
	cogl-x11-renderer-private.h \
	cogl-xlib-renderer-private.h \
	cogl-xlib-renderer.c \
	cogl-xlib.c \
	cogl-xlib-private.h \
	winsys/cogl-texture-pixmap-x11.c \
	winsys/cogl-texture-pixmap-x11-private.h
endif
if SUPPORT_GLX
cogl_experimental_h += cogl-glx.h
cogl_sources_c += \
	cogl-glx-renderer-private.h \
	cogl-glx-display-private.h \
	winsys/cogl-winsys-glx-feature-functions.h \
	winsys/cogl-winsys-glx-private.h \
	winsys/cogl-winsys-glx.c
endif
if SUPPORT_WGL
cogl_experimental_h += cogl-win32-renderer.h

cogl_sources_c += \
	cogl-win32-renderer.c \
	winsys/cogl-winsys-wgl-private.h \
	winsys/cogl-winsys-wgl.c \
	winsys/cogl-winsys-wgl-feature-functions.h
endif
if SUPPORT_WAYLAND_EGL_SERVER
cogl_experimental_h += cogl-wayland-server.h
endif
if SUPPORT_EGL_PLATFORM_WAYLAND
cogl_experimental_h += \
	cogl-wayland-renderer.h \
	cogl-wayland-client.h
cogl_sources_c += \
	winsys/cogl-winsys-egl-wayland.c \
	winsys/cogl-winsys-egl-wayland-private.h
endif
if SUPPORT_EGL_PLATFORM_KMS
cogl_experimental_h += \
	cogl-kms-renderer.h \
	cogl-kms-display.h
cogl_sources_c += \
	winsys/cogl-winsys-egl-kms.c \
	winsys/cogl-winsys-egl-kms-private.h
endif
if SUPPORT_EGL_PLATFORM_XLIB
cogl_sources_c += \
	winsys/cogl-winsys-egl-x11.c \
	winsys/cogl-winsys-egl-x11-private.h
endif
if SUPPORT_EGL_PLATFORM_POWERVR_NULL
cogl_sources_c += \
	winsys/cogl-winsys-egl-null.c \
	winsys/cogl-winsys-egl-null-private.h
endif
if SUPPORT_EGL_PLATFORM_GDL
cogl_sources_c += \
	winsys/cogl-winsys-egl-gdl.c \
	winsys/cogl-winsys-egl-gdl-private.h
endif
if SUPPORT_EGL_PLATFORM_ANDROID
cogl_sources_c += \
	winsys/cogl-winsys-egl-android.c \
	winsys/cogl-winsys-egl-android-private.h
endif
if SUPPORT_EGL_PLATFORM_MIR
cogl_experimental_h += cogl-mir-renderer.h
cogl_sources_c += \
	winsys/cogl-winsys-egl-mir.c \
	winsys/cogl-winsys-egl-mir-private.h
endif
if SUPPORT_EGL
cogl_experimental_h += cogl-egl.h
cogl_nodist_experimental_h += cogl-egl-defines.h

cogl_sources_c += \
	cogl-egl-private.h \
	winsys/cogl-winsys-egl.c \
	winsys/cogl-winsys-egl-feature-functions.h \
	winsys/cogl-winsys-egl-private.h
endif
if SUPPORT_SDL
cogl_experimental_h += $(srcdir)/cogl-sdl.h
cogl_sources_c += \
	winsys/cogl-winsys-sdl-private.h \
	winsys/cogl-winsys-sdl.c \
	cogl-sdl.c
endif
if SUPPORT_SDL2
cogl_experimental_h += $(srcdir)/cogl-sdl.h
cogl_sources_c += \
	winsys/cogl-winsys-sdl-private.h \
	winsys/cogl-winsys-sdl2.c \
	cogl-sdl.c
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
# XXX: The aim is to eventually get rid of all private API exports
# for cogl-pango.
libcogl_la_LDFLAGS = \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-export-dynamic \
	-export-symbols-regex "^(cogl|_cogl_debug_flags|_cogl_atlas_new|_cogl_atlas_add_reorganize_callback|_cogl_atlas_reserve_space|_cogl_callback|_cogl_util_get_eye_planes_for_screen_poly|_cogl_atlas_texture_remove_reorganize_callback|_cogl_atlas_texture_add_reorganize_callback|_cogl_texture_get_format|_cogl_texture_foreach_sub_texture_in_region|_cogl_profile_trace_message|_cogl_context_get_default|_cogl_framebuffer_get_stencil_bits|_cogl_clip_stack_push_rectangle|_cogl_framebuffer_get_modelview_stack|_cogl_object_default_unref|_cogl_pipeline_foreach_layer_internal|_cogl_clip_stack_push_primitive|_cogl_buffer_unmap_for_fill_or_fallback|_cogl_framebuffer_draw_primitive|_cogl_debug_instances|_cogl_framebuffer_get_projection_stack|_cogl_pipeline_layer_get_texture|_cogl_buffer_map_for_fill_or_fallback|_cogl_texture_can_hardware_repeat|_cogl_pipeline_prune_to_n_layers|_cogl_primitive_draw|test_|unit_test_).*"

libcogl_la_SOURCES = $(cogl_sources_c)
nodist_libcogl_la_SOURCES = $(BUILT_SOURCES)

# Cogl installed headers
cogl_headers = \
	$(cogl_1_public_h) \
	cogl-deprecated.h \
	cogl-pango.h \
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

Cogl-2.0.gir: libcogl.la Makefile

Cogl_2_0_gir_NAMESPACE = Cogl
Cogl_2_0_gir_VERSION = 2.0
Cogl_2_0_gir_LIBS = libcogl.la
if UNIT_TESTS
Cogl_2_0_gir_LIBS += $(top_builddir)/test-fixtures/libtest-fixtures.la
endif
Cogl_2_0_gir_FILES = $(cogl_experimental_h) $(cogl_additional_experimental_h) cogl-enum-types.h

Cogl_1_0_gir_CFLAGS = $(AM_CPPFLAGS) $(COGL_DEP_CFLAGS) -UCOGL_ENABLE_EXPERIMENTAL_API -UCOGL_ENABLE_EXPERIMENTAL_2_0_API -UCOGL_COMPILATION -D__COGL_H_INSIDE__ -D__COGL_XLIB_H_INSIDE__ -D__COGL_EGL_H_INSIDE__ -D__COGL_GLX_H_INSIDE__ -DCOGL_GIR_SCANNING
Cogl_1_0_gir_INCLUDES = GL-1.0 GObject-2.0
Cogl_1_0_gir_EXPORT_PACKAGES = cogl-1.0
Cogl_1_0_gir_SCANNERFLAGS = --warn-all --c-include='cogl/cogl.h'

Cogl_2_0_gir_CFLAGS = $(AM_CPPFLAGS) $(COGL_DEP_CFLAGS) -DCOGL_ENABLE_EXPERIMENTAL_API=1 -UCOGL_COMPILATION -D__COGL_H_INSIDE__ -D__COGL_XLIB_H_INSIDE__ -DCOGL_GIR_SCANNING
Cogl_2_0_gir_INCLUDES = GL-1.0 GObject-2.0
Cogl_2_0_gir_EXPORT_PACKAGES = cogl-2.0-experimental
Cogl_2_0_gir_SCANNERFLAGS = --warn-all --c-include='cogl/cogl.h' --symbol-prefix=cogl --symbol-prefix=cogl2

INTROSPECTION_GIRS += Cogl-1.0.gir Cogl-2.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
