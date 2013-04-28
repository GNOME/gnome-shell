include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir)

if !USE_GLIB
AM_CPPFLAGS += -I$(top_builddir)/deps/glib
endif

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
	$(top_builddir)/cogl/libcogl.la \
	$(LIBM)

if !USE_GLIB
common_ldadd += $(top_builddir)/deps/glib/libglib.la
endif

programs = cogl-info

cogl_info_SOURCES = cogl-info.c
cogl_info_LDADD = $(common_ldadd)

if USE_GLIB
programs += cogl-hello cogl-msaa cogl-gles2-context
examples_datadir = $(pkgdatadir)/examples-data
examples_data_DATA =

cogl_hello_SOURCES = cogl-hello.c
cogl_hello_LDADD = $(common_ldadd)
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
# Note: Cogland currently only builds for Wayland 0.85
programs += cogland
cogland_SOURCES = cogland.c
cogland_LDADD = $(common_ldadd)
endif

cogl_gles2_context_SOURCES = cogl-gles2-context.c
cogl_gles2_context_LDADD = $(common_ldadd)

if BUILD_COGL_GLES2
programs += cogl-gles2-gears
cogl_gles2_gears_SOURCES = cogl-gles2-gears.c
cogl_gles2_gears_CFLAGS = -I$(top_srcdir)/cogl-gles2 $(AM_CFLAGS)
cogl_gles2_gears_LDADD = $(common_ldadd) $(top_builddir)/cogl-gles2/libcogl-gles2.la
endif

endif #USE_GLIB

# XXX although emscripten "supports sdl" we can't build cogl-sdl-hello
# un-modified for emscripten since emscripten doesn't support
# SDL_WaitEvent() and we need to use some special emscripten apis
# to create a mainloop....
if USING_EMSCRIPTEN

programs += cogl-emscripten-hello
cogl_emscripten_hello_SOURCES = cogl-emscripten-hello.c
cogl_emscripten_hello_LDADD = $(common_ldadd)

else # USING_EMSCRIPTEN

if SUPPORT_SDL
programs += cogl-sdl-hello
cogl_sdl_hello_SOURCES = cogl-sdl-hello.c
cogl_sdl_hello_LDADD = $(common_ldadd)
endif

endif # USING_EMSCRIPTEN

if SUPPORT_SDL2
programs += cogl-sdl2-hello
cogl_sdl2_hello_SOURCES = cogl-sdl2-hello.c
cogl_sdl2_hello_LDADD = $(common_ldadd)
endif

if USING_EMSCRIPTEN
%.html: %.o $(top_builddir)/cogl/.libs/libcogl2.so $(top_builddir)/deps/glib/.libs/libglib.a
	$(CC) $(AM_CFLAGS) $(CFLAGS) -o $@ $^

all-local: $(addsuffix .html, $(programs))
endif

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

