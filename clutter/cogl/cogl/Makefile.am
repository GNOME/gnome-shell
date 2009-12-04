include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = driver

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

cogl-defines.h: $(top_builddir)/clutter/cogl/cogl/driver/gl/cogl-defines.h $(top_builddir)/clutter/cogl/cogl/driver/gles/cogl-defines.h
	$(QUIET_GEN)cp -f $(top_builddir)/clutter/cogl/cogl/driver/$(COGL_DRIVER)/cogl-defines.h $(@F)

BUILT_SOURCES += cogl-defines.h
DISTCLEANFILES += cogl-defines.h

# public headers
cogl_public_h = \
	$(srcdir)/cogl-bitmap.h 		\
	$(srcdir)/cogl-color.h 			\
	$(srcdir)/cogl-debug.h 			\
	$(srcdir)/cogl-fixed.h 			\
	$(srcdir)/cogl-material.h 		\
	$(srcdir)/cogl-matrix.h 		\
	$(srcdir)/cogl-offscreen.h 		\
	$(srcdir)/cogl-path.h 			\
	$(srcdir)/cogl-shader.h 		\
	$(srcdir)/cogl-texture.h 		\
	$(srcdir)/cogl-types.h 			\
	$(srcdir)/cogl-vertex-buffer.h 		\
	$(srcdir)/cogl.h			\
	$(NULL)

# windowing system; all sources have to be distributed, but we'll
# compile just the one we need
cogl_winsys_sources = \
	$(srcdir)/winsys/cogl-eglnative.c	\
	$(srcdir)/winsys/cogl-eglx.c		\
	$(srcdir)/winsys/cogl-glx.c		\
	$(srcdir)/winsys/cogl-osx.c		\
	$(srcdir)/winsys/cogl-sdl.c		\
	$(srcdir)/winsys/cogl-win32.c		\
	$(NULL)

# glib-mkenums rules
glib_enum_h = cogl-enum-types.h
glib_enum_c = cogl-enum-types.c
glib_enum_headers = $(cogl_public_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

noinst_LTLIBRARIES = libclutter-cogl.la
libclutter_cogl_la_CPPFLAGS = 		\
	$(COGL_DEBUG_CFLAGS) 		\
	$(CLUTTER_DEBUG_CFLAGS) 	\
	$(MAINTAINER_CFLAGS)		\
	-DG_DISABLE_SINGLE_INCLUDES 	\
	-DG_LOG_DOMAIN=\"Cogl-$(COGL_WINSYS)\" \
	-DCLUTTER_COMPILATION

libclutter_cogl_la_CFLAGS = $(CLUTTER_CFLAGS)

libclutter_cogl_la_LIBADD = -lm $(CLUTTER_LIBS) \
	$(top_builddir)/clutter/cogl/cogl/driver/$(COGL_DRIVER)/libclutter-cogl-driver.la

libclutter_cogl_la_SOURCES = 				\
	$(BUILT_SOURCES)				\
	$(srcdir)/winsys/cogl-winsys.h			\
	$(srcdir)/winsys/cogl-@COGL_WINSYS@.c		\
	$(srcdir)/cogl-handle.h 			\
	$(srcdir)/cogl-context.h			\
	$(srcdir)/cogl-context.c			\
	$(srcdir)/cogl-internal.h			\
	$(srcdir)/cogl.c				\
	$(srcdir)/cogl-util.h 				\
	$(srcdir)/cogl-util.c 				\
	$(srcdir)/cogl-bitmap-private.h 		\
	$(srcdir)/cogl-bitmap.c 			\
	$(srcdir)/cogl-bitmap-fallback.c 		\
	$(srcdir)/cogl-primitives.h 			\
	$(srcdir)/cogl-primitives.c 			\
	$(srcdir)/cogl-bitmap-pixbuf.c 			\
	$(srcdir)/cogl-clip-stack.h 			\
	$(srcdir)/cogl-clip-stack.c			\
	$(srcdir)/cogl-feature-private.h                \
	$(srcdir)/cogl-feature-private.c                \
	$(srcdir)/cogl-fixed.c		    		\
	$(srcdir)/cogl-color.c				\
	$(srcdir)/cogl-vertex-buffer-private.h 		\
	$(srcdir)/cogl-vertex-buffer.c			\
	$(srcdir)/cogl-matrix.c				\
	$(srcdir)/cogl-matrix-private.h			\
	$(srcdir)/cogl-matrix-stack.c			\
	$(srcdir)/cogl-matrix-stack.h			\
	$(srcdir)/cogl-material.c			\
	$(srcdir)/cogl-material-private.h		\
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
	$(NULL)

EXTRA_DIST += $(cogl_winsys_sources)
EXTRA_DIST += stb_image.c

# COGL installed headers
cogl_headers = \
	$(cogl_public_h) \
	$(srcdir)/cogl-deprecated.h \
	cogl-defines.h \
	cogl-enum-types.h \
	$(NULL)

coglincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
coglinclude_HEADERS = $(cogl_headers)

if HAVE_INTROSPECTION
Cogl-@CLUTTER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) libclutter-cogl.la
	$(QUIET_GEN)$(INTROSPECTION_SCANNER) -v \
		--namespace Cogl --nsversion=@CLUTTER_API_VERSION@ \
		-I$(srcdir) \
		-I$(srcdir)/.. \
		-I$(srcdir)/winsys \
		-I$(srcdir)/driver/@COGL_DRIVER@ \
		-DCLUTTER_COMPILATION \
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

