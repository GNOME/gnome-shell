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
