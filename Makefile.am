SUBDIRS=clutter doc examples 

pcfiles = clutter-@CLUTTER_MAJORMINOR@.pc

%-@CLUTTER_MAJORMINOR@.pc: %.pc
	cp $< $@

pkgconfig_DATA = $(pcfiles)
pkgconfigdir   = $(libdir)/pkgconfig

EXTRA_DIST = clutter.pc.in

CLEANFILES = $(pcfiles)

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc --enable-python --enable-gtk

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub \
                       configure depcomp install-sh ltmain.sh     \
                       Makefile.in missing config.h.in
