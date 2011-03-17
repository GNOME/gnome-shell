include $(top_srcdir)/build/autotools/Makefile.am.silent

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_builddir)/clutter/cogl

AM_CFLAGS = \
	$(COGL_DEP_CFLAGS) \
	$(COGL_EXTRA_CFLAGS) \
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API

AM_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED


common_ldadd = \
	$(top_builddir)/cogl/libcogl.la \
	$(top_builddir)/pango/libcoglpango.la

noinst_PROGRAMS = hello

hello_SOURCES = hello.c
hello_LDADD = $(common_ldadd)

if X11_TESTS
noinst_PROGRAMS += x11-foreign
x11_foreign_SOURCES = x11-foreign.c
x11_foreign_LDADD = $(common_ldadd)
endif
