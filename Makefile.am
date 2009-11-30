include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

SUBDIRS = doc build clutter tests po

DIST_SUBDIRS = build clutter tests doc po

ACLOCAL_AMFLAGS = -I build/autotools

pcfiles = \
	clutter-$(CLUTTER_API_VERSION).pc \
	clutter-$(CLUTTER_WINSYS)-$(CLUTTER_API_VERSION).pc \
	$(NULL)

# clutter-<major>.<minor>.pc - for generic dependencies
clutter-$(CLUTTER_API_VERSION).pc: clutter.pc
	$(QUIET_GEN)cp $< $@

# clutter-<winsys>-<major>.<minor>.pc - for backend-specific dependencies
clutter-$(CLUTTER_WINSYS)-$(CLUTTER_API_VERSION).pc: clutter.pc
	$(QUIET_GEN)cp $< $@

.PHONY: test-report full-report
test-report full-report:
	$(MAKE) -C tests/conform $(@)

pkgconfig_DATA = $(pcfiles)
pkgconfigdir   = $(libdir)/pkgconfig

EXTRA_DIST = 				\
	ChangeLog.pre-git-import	\
	clutter.pc.in 			\
	HACKING 			\
	HACKING.backends 		\
	CODING_STYLE			\
	$(NULL)

CLEANFILES = $(pcfiles)

DISTCLEANFILES = doltcompile doltlibtool

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-maintainer-flags

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = 	\
	aclocal.m4 	\
	config.guess 	\
	config.h.in 	\
        configure 	\
	gtk-doc.make 	\
	Makefile.in 	\
	$(NULL)

include $(top_srcdir)/build/autotools/Makefile.am.changelog
