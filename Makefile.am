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

if HAVE_INTROSPECTION

cogl_headers = \
	$(top_srcdir)/clutter/cogl/cogl-bitmap.h	\
	$(top_srcdir)/clutter/cogl/cogl-color.h		\
	$(top_srcdir)/clutter/cogl/cogl-debug.h		\
	$(top_srcdir)/clutter/cogl/cogl-fixed.h		\
	$(top_srcdir)/clutter/cogl/cogl-material.h	\
	$(top_srcdir)/clutter/cogl/cogl-matrix.h	\
	$(top_srcdir)/clutter/cogl/cogl-offscreen.h	\
	$(top_srcdir)/clutter/cogl/cogl-path.h		\
	$(top_srcdir)/clutter/cogl/cogl-shader.h	\
	$(top_srcdir)/clutter/cogl/cogl-texture.h	\
	$(top_srcdir)/clutter/cogl/cogl-types.h		\
	$(top_srcdir)/clutter/cogl/cogl-vertex-buffer.h

Cogl-@CLUTTER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) $(CLUTTER_COGL)/libclutter-cogl.la
	$(QUIET_GEN)$(INTROSPECTION_SCANNER) -v \
		--namespace Cogl --nsversion=@CLUTTER_API_VERSION@ \
		-I$(top_srcdir)/clutter/cogl \
		-I$(top_srcdir)/clutter/cogl/common \
		-I$(top_srcdir)/clutter/cogl/@CLUTTER_COGL@ \
		-I$(top_builddir)/clutter \
		-DCLUTTER_COMPILATION \
		--include=GL-1.0 \
		--include=GObject-2.0 \
		--library=clutter-@CLUTTER_FLAVOUR@-@CLUTTER_API_VERSION@ \
		--libtool="$(top_builddir)/doltlibtool" \
		--pkg gobject-2.0 \
		--output $@ \
		$(top_builddir)/clutter/cogl/cogl-defines-@CLUTTER_COGL@.h \
		$(top_builddir)/clutter/cogl/cogl.h \
		$(cogl_headers)

BUILT_GIRSOURCES = Cogl-@CLUTTER_API_VERSION@.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(BUILT_GIRSOURCES)

#typelibsdir = $(libdir)/girepository-1.0
#typelibs_DATA = Cogl-@CLUTTER_API_VERSION@.typelib

#%.typelib: %.gir $(INTROSPECTION_COMPILER)
#	$(QUIET_GEN)$(DEBUG) $(INTROSPECTION_COMPILER) \
#		--includedir=$(top_builddir)/clutter \
#		--includedir=$(top_srcdir)/clutter \
#		--includedir=$(srcdir) \
#		--includedir=. \
#		$(INTROSPECTION_COMPILER_OPTS) \
#		$< -o $(@F)

CLEANFILES += $(BUILT_GIRSOURCES) #$(typelibs_DATA)

endif
