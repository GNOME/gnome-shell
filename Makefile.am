NULL =

SUBDIRS = clutter tests po

if BUILD_GTK_DOC
SUBDIRS += doc
endif

DIST_SUBDIRS = clutter tests doc po

pcfiles = \
	clutter-$(CLUTTER_API_VERSION).pc \
	clutter-$(CLUTTER_FLAVOUR)-$(CLUTTER_API_VERSION).pc \
	$(NULL)

# clutter-<major>.<minor>.pc - for generic dependencies
clutter-$(CLUTTER_API_VERSION).pc: clutter.pc
	@cp $< $@

# clutter-<flavour>-<major>.<minor>.pc - for backend-specific dependencies
clutter-$(CLUTTER_FLAVOUR)-$(CLUTTER_API_VERSION).pc: clutter.pc
	@cp $< $@

.PHONY: test-report full-report
test-report full-report:
	$(MAKE) -C tests/conform $(@)

pkgconfig_DATA = $(pcfiles)
pkgconfigdir   = $(libdir)/pkgconfig

DEFAULT_FLAVOUR = @CLUTTER_FLAVOUR@

EXTRA_DIST = clutter.pc.in HACKING HACKING.backends

CLEANFILES = $(pcfiles)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-maintainer-flags

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = \
	aclocal.m4 \
	compile \
	config.guess \
	config.h.in \
	config.sub \
        configure \
	depcomp \
	gtk-doc.make \
	install-sh \
	ltmain.sh \
	Makefile.in \
	missing \
	$(NULL)
