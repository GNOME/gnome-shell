include $(top_srcdir)/build/autotools/Makefile.am.silent

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clutter/cogl \
	-I$(top_builddir)/clutter/cogl

AM_CFLAGS = \
	$(COGL_DEP_CFLAGS) \
	$(COGL_EXTRA_CFLAGS) \
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API \
	-DCOGL_EXAMPLES_DATA=\""$(abs_top_srcdir)/examples/"\"

AM_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED


common_ldadd = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/cogl/libcogl.la

noinst_PROGRAMS = hello

hello_SOURCES = hello.c
hello_LDADD = $(common_ldadd)

if BUILD_COGL_PANGO
noinst_PROGRAMS += crate
crate_SOURCES = crate.c
crate_LDADD = $(common_ldadd) $(top_builddir)/cogl-pango/libcogl-pango.la
endif

if X11_TESTS
noinst_PROGRAMS += x11-foreign
x11_foreign_SOURCES = x11-foreign.c
x11_foreign_LDADD = $(common_ldadd)
endif

if SUPPORT_WAYLAND_EGL_SERVER
noinst_PROGRAMS += cogland
cogland_SOURCES = cogland.c
cogland_LDADD = $(common_ldadd)
endif

EXTRA_DIST = crate.jpg
