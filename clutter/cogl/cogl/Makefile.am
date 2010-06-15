include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = driver tesselator

BUILT_SOURCES =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =

# pkg-config ==================================================================
pc_files = \
	cogl-$(COGL_DRIVER)-$(CLUTTER_API_VERSION).pc \
	cogl-$(CLUTTER_API_VERSION).pc

cogl-$(CLUTTER_API_VERSION).pc: cogl.pc
	$(QUIET_GEN)cp -f $< $(@F)

cogl-$(COGL_DRIVER)-$(CLUTTER_API_VERSION).pc: cogl.pc
	$(QUIET_GEN)cp -f $< $(@F)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cogl.pc.in
CLEANFILES += $(pc_files)

INCLUDES = \
	-I$(top_srcdir)				\
	-I$(top_srcdir)/clutter/cogl		\
	-I$(srcdir)/winsys 			\
	-I$(srcdir)/driver/$(COGL_DRIVER) 	\
	-I$(top_builddir)/clutter/cogl          \
	$(NULL)

AM_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES 		\
	-DG_DISABLE_DEPRECATED			\
	-DG_LOG_DOMAIN=\"Cogl-$(COGL_WINSYS)\" 	\
	-DCLUTTER_COMPILATION			\
	-DCOGL_ENABLE_EXPERIMENTAL_API		\
	$(COGL_DEBUG_CFLAGS) 			\
	$(CLUTTER_DEBUG_CFLAGS) 		\
	$(NULL)

AM_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)

BUILT_SOURCES += cogl-defines.h
DISTCLEANFILES += cogl-defines.h

EXTRA_DIST += cogl-defines.h.in

# public headers
cogl_public_h = \
	$(srcdir)/cogl-object.h 		\
	$(srcdir)/cogl-bitmap.h 		\
	$(srcdir)/cogl-buffer.h 		\
	$(srcdir)/cogl-color.h 			\
	$(srcdir)/cogl-debug.h 			\
	$(srcdir)/cogl-fixed.h 			\
	$(srcdir)/cogl-material.h 		\
	$(srcdir)/cogl-vector.h 		\
	$(srcdir)/cogl-matrix.h 		\
	$(srcdir)/cogl-offscreen.h 		\
	$(srcdir)/cogl-primitives.h 		\
	$(srcdir)/cogl-path.h 			\
	$(srcdir)/cogl-pixel-buffer.h		\
	$(srcdir)/cogl-shader.h 		\
	$(srcdir)/cogl-texture.h 		\
	$(srcdir)/cogl-types.h 			\
	$(srcdir)/cogl-vertex-buffer.h 		\
	$(srcdir)/cogl.h			\
	$(NULL)

cogl_sources_c = \
	$(srcdir)/winsys/cogl-winsys.h			\
	$(srcdir)/winsys/cogl-context-winsys.h          \
	$(srcdir)/winsys/cogl-context-winsys.c          \
	$(srcdir)/winsys/cogl-winsys-feature-functions.h \
	$(srcdir)/cogl-handle.h 			\
	$(srcdir)/cogl-context.h			\
	$(srcdir)/cogl-context.c			\
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
	$(srcdir)/cogl-primitives.h 			\
	$(srcdir)/cogl-primitives.c 			\
	$(srcdir)/cogl-path-private.h                   \
	$(srcdir)/cogl-path.h 				\
	$(srcdir)/cogl-path.c 				\
	$(srcdir)/cogl-bitmap-pixbuf.c 			\
	$(srcdir)/cogl-clip-stack.h 			\
	$(srcdir)/cogl-clip-stack.c			\
	$(srcdir)/cogl-clip-state.h                     \
	$(srcdir)/cogl-clip-state.c                     \
	$(srcdir)/cogl-feature-private.h                \
	$(srcdir)/cogl-feature-private.c                \
	$(srcdir)/cogl-fixed.c		    		\
	$(srcdir)/cogl-color-private.h    		\
	$(srcdir)/cogl-color.c				\
	$(srcdir)/cogl-buffer-private.h 		\
	$(srcdir)/cogl-buffer.c				\
	$(srcdir)/cogl-pixel-buffer-private.h		\
	$(srcdir)/cogl-pixel-buffer.c			\
	$(srcdir)/cogl-vertex-buffer-private.h 		\
	$(srcdir)/cogl-vertex-buffer.c			\
	$(srcdir)/cogl-matrix.c				\
	$(srcdir)/cogl-vector.c				\
	$(srcdir)/cogl-matrix-private.h			\
	$(srcdir)/cogl-matrix-stack.c			\
	$(srcdir)/cogl-matrix-stack.h			\
	$(srcdir)/cogl-material.c			\
	$(srcdir)/cogl-material-private.h		\
	$(srcdir)/cogl-material-glsl.c			\
	$(srcdir)/cogl-material-glsl-private.h		\
	$(srcdir)/cogl-material-arbfp.c			\
	$(srcdir)/cogl-material-arbfp-private.h		\
	$(srcdir)/cogl-material-fixed.c			\
	$(srcdir)/cogl-material-fixed-private.h		\
	$(srcdir)/cogl-blend-string.c			\
	$(srcdir)/cogl-blend-string.h			\
	$(srcdir)/cogl-debug.c				\
	$(srcdir)/cogl-sub-texture-private.h            \
	$(srcdir)/cogl-texture-private.h		\
	$(srcdir)/cogl-texture-2d-private.h             \
	$(srcdir)/cogl-texture-2d-sliced-private.h 	\
	$(srcdir)/cogl-texture-driver.h			\
	$(srcdir)/cogl-sub-texture.c                    \
	$(srcdir)/cogl-texture.c			\
	$(srcdir)/cogl-texture-2d.c                     \
	$(srcdir)/cogl-texture-2d-sliced.c		\
	$(srcdir)/cogl-texture-rectangle-private.h      \
	$(srcdir)/cogl-texture-rectangle.c              \
	$(srcdir)/cogl-atlas.h                          \
	$(srcdir)/cogl-atlas.c                          \
	$(srcdir)/cogl-atlas-texture-private.h          \
	$(srcdir)/cogl-atlas-texture.c                  \
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
	$(srcdir)/cogl-bitmask.h                        \
	$(srcdir)/cogl-bitmask.c                        \
	$(NULL)

# glib-mkenums rules
glib_enum_h = cogl-enum-types.h
glib_enum_c = cogl-enum-types.c
glib_enum_headers = $(cogl_public_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

noinst_LTLIBRARIES = libclutter-cogl.la

libclutter_cogl_la_LIBADD = \
	-lm $(CLUTTER_LIBS) \
	$(top_builddir)/clutter/cogl/cogl/driver/$(COGL_DRIVER)/libclutter-cogl-driver.la \
	$(top_builddir)/clutter/cogl/cogl/tesselator/libtesselator.la

libclutter_cogl_la_SOURCES = $(BUILT_SOURCES) $(cogl_sources_c)

if SUPPORT_XLIB
cogl_public_h += \
       $(srcdir)/winsys/cogl-texture-pixmap-x11.h

libclutter_cogl_la_SOURCES += \
       $(srcdir)/winsys/cogl-xlib.h \
       $(srcdir)/winsys/cogl-xlib.c \
       $(srcdir)/winsys/cogl-texture-pixmap-x11.c \
       $(srcdir)/winsys/cogl-texture-pixmap-x11-private.h
endif
if SUPPORT_GLX
libclutter_cogl_la_SOURCES += \
       $(srcdir)/winsys/cogl-glx.c
endif
if SUPPORT_EGL_PLATFORM_POWERVR_X11
libclutter_cogl_la_SOURCES += \
       $(srcdir)/winsys/cogl-egl.c
endif
if SUPPORT_EGL_PLATFORM_POWERVR_NULL
libclutter_cogl_la_SOURCES += \
       $(srcdir)/winsys/cogl-egl.c
endif
if SUPPORT_EGL_PLATFORM_FRUITY
libclutter_cogl_la_SOURCES += \
       $(srcdir)/winsys/cogl-fruity.c
endif
if SUPPORT_WIN32
libclutter_cogl_la_SOURCES += \
       $(srcdir)/winsys/cogl-win32.c
endif
if SUPPORT_OSX
libclutter_cogl_la_SOURCES += \
       $(srcdir)/winsys/cogl-osx.c
endif

EXTRA_DIST += stb_image.c

# COGL installed headers
cogl_headers = \
	$(cogl_public_h) \
	$(srcdir)/cogl-deprecated.h \
	cogl-enum-types.h \
	$(NULL)

coglincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
coglinclude_HEADERS = $(cogl_headers)
nodist_coglinclude_HEADERS = cogl-defines.h

if HAVE_INTROSPECTION
Cogl-@CLUTTER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) libclutter-cogl.la
	$(QUIET_GEN)$(INTROSPECTION_SCANNER) -v \
		--namespace Cogl --nsversion=@CLUTTER_API_VERSION@ \
		$(INCLUDES) \
		$(AM_CPPFLAGS) \
		--c-include='cogl/cogl.h' \
		--include=GL-1.0 \
		--include=GObject-2.0 \
		--library=libclutter-cogl.la \
		--libtool="$(top_builddir)/libtool" \
		--pkg gobject-2.0 \
		--output $@ \
		$(cogl_headers)

BUILT_GIRSOURCES = Cogl-@CLUTTER_API_VERSION@.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(BUILT_GIRSOURCES)

CLEANFILES += $(BUILT_GIRSOURCES)
endif
