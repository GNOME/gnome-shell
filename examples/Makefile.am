include $(top_srcdir)/build/autotools/Makefile.am.silent

INCLUDES = \
	-I$(top_srcdir)

AM_CFLAGS = \
	$(COGL_DEP_CFLAGS) \
	$(COGL_EXTRA_CFLAGS) \
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API

if INSTALL_EXAMPLES
AM_CFLAGS += -DCOGL_EXAMPLES_DATA=\""$(pkgdatadir)/examples-data/"\"
else
AM_CFLAGS += -DCOGL_EXAMPLES_DATA=\""$(abs_top_srcdir)/examples/"\"
endif

common_ldadd = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/cogl/libcogl.la

programs = cogl-info

cogl_info_SOURCES = cogl-info.c
cogl_info_LDADD = $(common_ldadd)

if USE_GLIB
programs += cogl-hello cogl-msaa  cogl-gles2-context cogl-gles2-gears
examples_datadir = $(pkgdatadir)/examples-data
examples_data_DATA =

cogl_hello_SOURCES = cogl-hello.c
cogl_hello_LDADD = $(common_ldadd)
cogl_info_SOURCES = cogl-info.c
cogl_info_LDADD = $(common_ldadd)
cogl_msaa_SOURCES = cogl-msaa.c
cogl_msaa_LDADD = $(common_ldadd)

if BUILD_COGL_PANGO
programs += cogl-crate
cogl_crate_SOURCES = cogl-crate.c
cogl_crate_LDADD = $(common_ldadd) $(COGL_PANGO_DEP_LIBS) $(top_builddir)/cogl-pango/libcogl-pango.la
cogl_crate_CFLAGS = $(AM_CFLAGS) $(COGL_PANGO_DEP_CFLAGS)
examples_data_DATA += crate.jpg
endif

if X11_TESTS
programs += cogl-x11-foreign cogl-x11-tfp
cogl_x11_foreign_SOURCES = cogl-x11-foreign.c
cogl_x11_foreign_LDADD = $(common_ldadd)
cogl_x11_tfp_SOURCES = cogl-x11-tfp.c
cogl_x11_tfp_LDADD = $(common_ldadd)
endif

if SUPPORT_WAYLAND_EGL_SERVER
programs += cogland
cogland_SOURCES = cogland.c
cogland_LDADD = $(common_ldadd)
endif

if SUPPORT_SDL
programs += cogl-sdl-hello
cogl_sdl_hello_SOURCES = cogl-sdl-hello.c
cogl_sdl_hello_LDADD = $(common_ldadd)
endif

cogl_gles2_context_SOURCES = cogl-gles2-context.c
cogl_gles2_context_LDADD = $(common_ldadd)

cogl_gles2_gears_SOURCES = cogl-gles2-gears.c
cogl_gles2_gears_CFLAGS = -I$(top_srcdir)/cogl-gles2 $(AM_CFLAGS)
cogl_gles2_gears_LDADD = $(common_ldadd) $(top_builddir)/cogl-gles2/libcogl-gles2.la

endif #USE_GLIB

if INSTALL_EXAMPLES
bin_PROGRAMS = $(programs)
else
noinst_PROGRAMS = $(programs)
endif

EXTRA_DIST = \
	crate.jpg \
	android/hello/AndroidManifest.xml \
	android/hello/jni/Application.mk \
	android/hello/jni/Android.mk \
	android/hello/jni/main.c \
	android/hello/res/values/strings.xml \
	android/hello/.gitignore \
	android/hello/build.xml

