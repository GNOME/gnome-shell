include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = winsys driver

BUILT_SOURCES = cogl.h

EXTRA_DIST = cogl.h.in cogl.pc.in

pc_files = \
	cogl-$(COGL_DRIVER)-$(CLUTTER_API_VERSION).pc \
	cogl-$(CLUTTER_API_VERSION).pc

cogl-$(CLUTTER_API_VERSION).pc: cogl.pc
	$(QUIET_GEN)cp -f $< $(@F)

cogl-$(COGL_DRIVER)-$(CLUTTER_API_VERSION).pc: cogl.pc
	$(QUIET_GEN)cp -f $< $(@F)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

CLEANFILES = $(pc_files)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(srcdir) \
	-I$(srcdir)/.. \
	-I$(srcdir)/winsys \
	-I$(srcdir)/driver/$(COGL_DRIVER) \
	-I. \
	-I.. \
	-DG_DISABLE_SINGLE_INCLUDES \
        -DG_LOG_DOMAIN=\"Cogl-Core\" \
        -DCLUTTER_COMPILATION

cogl_public_h = \
	$(srcdir)/cogl-bitmap.h \
	$(srcdir)/cogl-color.h \
	$(srcdir)/cogl-debug.h \
	$(srcdir)/cogl-fixed.h \
	$(srcdir)/cogl-material.h	\
	$(srcdir)/cogl-matrix.h \
	$(srcdir)/cogl-offscreen.h \
	$(srcdir)/cogl-path.h \
	$(srcdir)/cogl-shader.h \
	$(srcdir)/cogl-texture.h \
	$(srcdir)/cogl-types.h \
	$(srcdir)/cogl-vertex-buffer.h \
	cogl.h \
	$(NULL)

cogl-enum-types.h: stamp-cogl-enum-types.h
	@true
stamp-cogl-enum-types.h: $(cogl_public_h) Makefile
	$(QUIET_GEN)( $(GLIB_MKENUMS) \
		--template $(srcdir)/cogl-enum-types.h.in \
	$(cogl_public_h) ) > xgen-ceth \
	&& (cmp -s xgen-ceth cogl-enum-types.h || cp -f xgen-ceth cogl-enum-types.h) \
	&& rm -f xgen-ceth \
	&& echo timestamp > $(@F)

cogl-enum-types.c: cogl-enum-types.h
	$(QUIET_GEN)( $(GLIB_MKENUMS) \
		--template $(srcdir)/cogl-enum-types.c.in \
	$(cogl_public_h) ) > xgen-cetc \
	&& cp -f xgen-cetc cogl-enum-types.c \
	&& rm -f xgen-cetc

BUILT_SOURCES += cogl-enum-types.h cogl-enum-types.c

noinst_LTLIBRARIES = libclutter-cogl.la
libclutter_cogl_la_CPPFLAGS = \
	$(CLUTTER_CFLAGS) \
	$(COGL_DEBUG_CFLAGS) \
	$(CLUTTER_DEBUG_CFLAGS) \
	$(MAINTAINER_CFLAGS)
libclutter_cogl_la_LIBADD = \
	-lm $(CLUTTER_LIBS) \
	winsys/libclutter-cogl-winsys.la	\
	driver/$(COGL_DRIVER)/libclutter-cogl-driver.la
#	os/$(COGL_DRIVER)/libclutter-cogl-os.la
libclutter_cogl_la_SOURCES = \
	cogl-enum-types.h		\
	cogl-enum-types.c		\
	cogl-handle.h 			\
	cogl-context.h			\
	cogl-context.c			\
	cogl-internal.h			\
	cogl.c				\
	cogl-util.h 			\
	cogl-util.c 			\
	cogl-bitmap-private.h 		\
	cogl-bitmap.c 			\
	cogl-bitmap-fallback.c 		\
	cogl-current-matrix.c		\
	cogl-current-matrix.h		\
	cogl-primitives.h 		\
	cogl-primitives.c 		\
	cogl-bitmap-pixbuf.c 		\
	cogl-clip-stack.h 		\
	cogl-clip-stack.c               \
        cogl-fixed.c                    \
        cogl-color.c			\
	cogl-vertex-buffer-private.h 	\
	cogl-vertex-buffer.c		\
	cogl-matrix.c			\
	cogl-matrix-stack.c		\
	cogl-matrix-stack.h		\
	cogl-material.c			\
	cogl-material-private.h		\
	cogl-blend-string.c		\
	cogl-blend-string.h		\
	cogl-debug.c			\
	cogl-texture-private.h		\
	cogl-texture-2d-sliced-private.h \
	cogl-texture-driver.h		\
	cogl-texture.c			\
	cogl-texture-2d-sliced.c	\
	cogl-spans.h			\
	cogl-spans.c			\
	$(NULL)

EXTRA_DIST += stb_image.c cogl-enum-types.h.in cogl-enum-types.c.in
CLEANFILES += stamp-cogl-enum-types.h
DISTCLEANFILES = cogl-enum-types.h cogl-enum-types.c

# COGL installed headers
cogl_headers = \
	$(cogl_public_h) \
	cogl-deprecated.h \
	cogl-defines-@COGL_DRIVER@.h \
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

