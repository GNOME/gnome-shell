SUBDIRS = clutter tests doc

pcfiles = clutter-$(CLUTTER_FLAVOUR)-$(CLUTTER_API_VERSION).pc

clutter-$(CLUTTER_FLAVOUR)-$(CLUTTER_API_VERSION).pc: clutter.pc
	cp $< $@

pkgconfig_DATA = $(pcfiles)
pkgconfigdir   = $(libdir)/pkgconfig

DEFAULT_FLAVOUR = @CLUTTER_FLAVOUR@
install-data-hook:
	(cd $(DESTDIR)$(pkgconfigdir) && \
	 test -f clutter-$(DEFAULT_FLAVOUR)-@CLUTTER_MAJORMINOR@.pc && \
	 rm -f clutter-@CLUTTER_MAJORMINOR@.pc && \
	 cp -f clutter-$(DEFAULT_FLAVOUR)-@CLUTTER_MAJORMINOR@.pc clutter-@CLUTTER_MAJORMINOR@.pc)

uninstall-local:
	rm -f $(DESTDIR)$(pkgconfigdir)/clutter-@CLUTTER_MAJORMINOR@.pc


EXTRA_DIST = clutter.pc.in HACKING

CLEANFILES = $(pcfiles)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-maintainer-flags

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub \
                       configure depcomp install-sh ltmain.sh     \
                       Makefile.in missing config.h.in
