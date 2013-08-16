include $(top_srcdir)/build/autotools/Makefile.am.silent

common_ldadd = \
	$(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la

common_sources = \
	cally-examples-util.c	\
	cally-examples-util.h

AM_CPPFLAGS = \
	-DPREFIXDIR=\"$(libdir)\" \
	-DCLUTTER_DISABLE_DEPRECATION_WARNINGS \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I$(top_srcdir)	\
	-I$(top_builddir) \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter \
	-I$(top_srcdir)/tests/accessibility

AM_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)
LDADD = $(common_ldadd) $(CLUTTER_LIBS)

check_PROGRAMS = \
	cally-atkcomponent-example	\
	cally-atkeditabletext-example   \
	cally-atkevents-example		\
	cally-atktext-example		\
	cally-clone-example

cally_atkcomponent_example_SOURCES    = $(common_sources) cally-atkcomponent-example.c
cally_atktext_example_SOURCES         = $(common_sources) cally-atktext-example.c
cally_atkevents_example_SOURCES       = $(common_sources) cally-atkevents-example.c
cally_atkeditabletext_example_SOURCES = $(common_sources) cally-atkeditabletext-example.c
cally_clone_example_SOURCES           = $(common_sources) cally-clone-example.c

DISTCLEANFILES =

-include $(top_srcdir)/build/autotools/Makefile.am.gitignore
