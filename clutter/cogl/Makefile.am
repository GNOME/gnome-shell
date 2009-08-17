NULL =

V         = @
Q         = $(V:1=)
QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

SUBDIRS = common $(CLUTTER_COGL)

BUILT_SOURCES = cogl.h

EXTRA_DIST = cogl.h.in cogl.pc.in

DIST_SUBDIRS = common gl gles

pc_files = \
	cogl-$(CLUTTER_COGL)-$(CLUTTER_API_VERSION).pc \
	cogl-$(CLUTTER_API_VERSION).pc

cogl-$(CLUTTER_API_VERSION).pc: cogl.pc
	$(QUIET_GEN)cp -f $< $(@F)

cogl-$(CLUTTER_COGL)-$(CLUTTER_API_VERSION).pc: cogl.pc
	$(QUIET_GEN)cp -f $< $(@F)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

CLEANFILES = $(pc_files)

AM_CPPFLAGS = $(CLUTTER_CFLAGS) $(CLUTTER_DEBUG_CFLAGS) $(MAINTAINER_CFLAGS)

# COGL installed headers
cogl_headers = \
	$(top_srcdir)/clutter/cogl/cogl-bitmap.h			\
	$(top_srcdir)/clutter/cogl/cogl-color.h				\
	$(top_srcdir)/clutter/cogl/cogl-debug.h				\
	$(top_srcdir)/clutter/cogl/cogl-deprecated.h			\
	$(top_srcdir)/clutter/cogl/cogl-fixed.h				\
	$(top_srcdir)/clutter/cogl/cogl-material.h			\
	$(top_srcdir)/clutter/cogl/cogl-matrix.h			\
	$(top_srcdir)/clutter/cogl/cogl-offscreen.h			\
	$(top_srcdir)/clutter/cogl/cogl-path.h				\
	$(top_srcdir)/clutter/cogl/cogl-shader.h			\
	$(top_srcdir)/clutter/cogl/cogl-texture.h			\
	$(top_srcdir)/clutter/cogl/cogl-types.h				\
	$(top_srcdir)/clutter/cogl/cogl-vertex-buffer.h			\
	$(top_builddir)/clutter/cogl/cogl-defines-@CLUTTER_COGL@.h 	\
	$(top_builddir)/clutter/cogl/cogl-enum-types.h			\
	$(top_builddir)/clutter/cogl/cogl.h				\
	$(NULL)

# this is copied in from common/ to make cogl.h work, but we
# need to clean it up ourselves once we're done
DISTCLEANFILES = cogl-enum-types.h

# HACK - gobject-introspection can't scan a library in another directory
# so we create a libclutter-cogl.la that's just identical to the one
# in the subdir
noinst_LTLIBRARIES = libclutter-cogl.la
libclutter_cogl_la_LIBADD = $(CLUTTER_COGL)/libclutter-cogl-$(CLUTTER_COGL).la
libclutter_cogl_la_SOURCES = $(cogl_headers)

coglincludedir = $(includedir)/clutter-@CLUTTER_API_VERSION@/cogl
coglinclude_HEADERS = $(cogl_headers)

if HAVE_INTROSPECTION
Cogl-@CLUTTER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) libclutter-cogl.la
	$(QUIET_GEN)$(INTROSPECTION_SCANNER) -v \
		--namespace Cogl --nsversion=@CLUTTER_API_VERSION@ \
		-I$(top_srcdir)/clutter/cogl \
		-I$(top_srcdir)/clutter/cogl/common \
		-I$(top_srcdir)/clutter/cogl/@CLUTTER_COGL@ \
		-I$(top_builddir)/clutter \
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
