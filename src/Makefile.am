NULL = 
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =
bin_SCRIPTS =
libexec_PROGRAMS =
noinst_LTLIBRARIES =
noinst_PROGRAMS =

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all --warn-error --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir) --includedir=$(MUTTER_LIB_DIR)/mutter/

typelibdir = $(pkglibdir)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

generated_bin_scripts = gnome-shell-installed gnome-shell-jhbuild gnome-shell-extension-tool
bin_SCRIPTS += gnome-shell $(generated_bin_scripts)

generated_script_substitutions = -e "s|@MUTTER_BIN_DIR[@]|$(MUTTER_BIN_DIR)|" \
	    -e "s|@datadir[@]|$(datadir)|" \
	    -e "s|@libexecdir[@]|$(libexecdir)|" \
	    -e "s|@libdir[@]|$(libdir)|" \
	    -e "s|@pkgdatadir[@]|$(pkgdatadir)|" \
	    -e "s|@ENABLE_DYNAMIC_MOZJS_HACK[@]|$(ENABLE_DYNAMIC_MOZJS_HACK)|" \
	    -e "s|@PYTHON[@]|$(PYTHON)|" \
	    -e "s|@VERSION[@]|$(VERSION)|" \
	    -e "s|@sysconfdir[@]|$(sysconfdir)|"

gnome-shell-jhbuild: gnome-shell-jhbuild.in Makefile
	$(AM_V_GEN) sed $(generated_script_substitutions) $< > $@.tmp && mv $@.tmp $@ && chmod a+x $@

gnome-shell-installed: gnome-shell-installed.in Makefile
	$(AM_V_GEN) sed $(generated_script_substitutions) $< > $@.tmp && mv $@.tmp $@ && chmod a+x $@

gnome-shell-extension-tool: gnome-shell-extension-tool.in Makefile
	$(AM_V_GEN) sed $(generated_script_substitutions) $< > $@.tmp && mv $@.tmp $@ && chmod a+x $@

if USE_JHBUILD_WRAPPER_SCRIPT
gnome-shell: gnome-shell-jhbuild Makefile
	cp $< $@.tmp && mv $@.tmp $@
else
gnome-shell: gnome-shell-installed Makefile
	cp $< $@.tmp && mv $@.tmp $@
endif

CLEANFILES += gnome-shell $(generated_bin_scripts)
EXTRA_DIST += $(generated_bin_scripts:=.in)

include Makefile-gdmuser.am
include Makefile-st.am
include Makefile-tray.am
include Makefile-gvc.am
include Makefile-calendar-server.am

gnome_shell_cflags =				\
	$(MUTTER_PLUGIN_CFLAGS)			\
	$(LIBGNOMEUI_CFLAGS)                    \
	-I$(srcdir)/tray			\
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DGNOME_SHELL_LIBEXECDIR=\"$(libexecdir)\"	\
	-DGNOME_SHELL_DATADIR=\"$(pkgdatadir)\"	\
	-DGNOME_SHELL_PKGLIBDIR=\"$(pkglibdir)\" \
	-DJSDIR=\"$(pkgdatadir)/js\"

plugindir = $(MUTTER_PLUGIN_DIR)
plugin_LTLIBRARIES = libgnome-shell.la

shell_built_sources = \
	shell-marshal.h \
	shell-marshal.c \
	shell-enum-types.h \
	shell-enum-types.c

BUILT_SOURCES += $(shell_built_sources)
EXTRA_DIST += shell-marshal.list

shell_public_headers_h =		\
	shell-app.h			\
	shell-app-system.h		\
	shell-app-usage.h		\
	shell-arrow.h			\
	shell-doc-system.h		\
	shell-embedded-window.h		\
	shell-generic-container.h	\
	shell-gtk-embed.h		\
	shell-global.h			\
	shell-perf-log.h		\
	shell-slicer.h			\
	shell-stack.h			\
	shell-tray-icon.h		\
	shell-tray-manager.h		\
	shell-util.h			\
	shell-window-tracker.h		\
	shell-wm.h			\
	shell-xfixes-cursor.h

libgnome_shell_la_SOURCES =		\
	$(shell_built_sources)		\
	$(shell_public_headers_h)	\
	shell-app-private.h		\
	shell-embedded-window-private.h	\
	shell-global-private.h		\
	shell-jsapi-compat-private.h	\
	shell-window-tracker-private.h	\
	shell-wm-private.h		\
	gnome-shell-plugin.c		\
	shell-app.c			\
	shell-a11y.h			\
	shell-a11y.c			\
	shell-app-system.c		\
	shell-app-usage.c		\
	shell-arrow.c			\
	shell-doc-system.c		\
	shell-embedded-window.c		\
	shell-generic-container.c	\
	shell-gtk-embed.c		\
	shell-global.c			\
	shell-perf-log.c		\
	shell-polkit-authentication-agent.h	\
	shell-polkit-authentication-agent.c	\
	shell-slicer.c			\
	shell-stack.c			\
	shell-tray-icon.c		\
	shell-tray-manager.c		\
	shell-util.c			\
	shell-window-tracker.c		\
	shell-wm.c			\
	shell-xfixes-cursor.c

libgnome_shell_la_gir_sources = \
	$(filter-out %-private.h $(shell_recorder_non_gir_sources), $(shell_public_headers_h) $(libgnome_shell_la_SOURCES))

########################################

shell_recorder_sources =        \
	shell-recorder.c	\
	shell-recorder.h

# Custom element is an internal detail
shell_recorder_non_gir_sources =  \
	shell-recorder-src.c	  \
	shell-recorder-src.h

if BUILD_RECORDER
libgnome_shell_la_SOURCES += $(shell_recorder_sources) $(shell_recorder_non_gir_sources)

noinst_PROGRAMS += test-recorder

test_recorder_CPPFLAGS = $(TEST_SHELL_RECORDER_CFLAGS)
test_recorder_LDADD = $(TEST_SHELL_RECORDER_LIBS)

test_recorder_SOURCES =     \
	$(shell_recorder_sources) $(shell_recorder_non_gir_sources) \
	test-recorder.c
endif BUILD_RECORDER

########################################

# In order to run the interactive tests for GUI components, we need to have
# an executable that exports the St components. Libtool doesn't have a way
# to include all the symbols from a convenience library into a executable
# so what we do is build a small uninstalled library that pulls in the
# St convenience library and link the test running program to that.

noinst_LTLIBRARIES += libjs-test.la

libjs_test_la_LDFLAGS = -rpath $(libdir)
libjs_test_la_CPPFLAGS = $(JS_TEST_CFLAGS)
libjs_test_la_LIBADD = $(JS_TEST_LIBS) libst-1.0.la

# The tests use or reference a couple of Shell classes
libjs_test_la_SOURCES = 		\
	shell-generic-container.c	\
	shell-perf-log.c

noinst_PROGRAMS += run-js-test

run_js_test_CPPFLAGS = $(JS_TEST_CFLAGS)
run_js_test_LDADD = $(JS_TEST_LIBS) libjs-test.la
run_js_test_LDFLAGS = -export-dynamic

run_js_test_SOURCES =			\
	run-js-test.c

########################################

shell-marshal.h: stamp-shell-marshal.h
	@true
stamp-shell-marshal.h: Makefile shell-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--prefix=_shell_marshal \
		--header \
	$(srcdir)/shell-marshal.list > xgen-smh && \
	(cmp -s xgen-smh shell-marshal.h || cp -f xgen-smh shell-marshal.h) && \
	rm -f xgen-smh && \
	echo timestamp > $(@F)
CLEANFILES += stamp-shell-marshal.h

shell-marshal.c: Makefile shell-marshal.list
	$(AM_V_GEN) (echo "#include \"shell-marshal.h\"" ; \
	 $(GLIB_GENMARSHAL) \
		--prefix=_shell_marshal \
		--body \
	 $(srcdir)/shell-marshal.list ) > xgen-smc && \
	cp -f xgen-smc shell-marshal.c && \
	rm -f xgen-smc


shell-enum-types.h: stamp-shell-enum-types.h Makefile
	@true
stamp-shell-enum-types.h: $(srcdir)/shell-enum-types.h.in $(shell_public_headers_h)
	$(AM_V_GEN) ( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template $< \
	  $(shell_public_headers_h) ) > $@.tmp && \
	(cmp -s $@.tmp shell-enum-types.h || mv $@.tmp shell-enum-types.h) && \
	rm -f $@.tmp && \
	echo timestamp > $(@F)
EXTRA_DIST += shell-enum-types.h.in
CLEANFILES += stamp-shell-enum-types.h

shell-enum-types.c: shell-enum-types.c.in stamp-shell-enum-types.h
	$(AM_V_GEN) ( cd $(srcdir) && \
	  $(GLIB_MKENUMS) \
	    --template $< \
	  $(shell_public_headers_h) ) > $@.tmp && \
	mv $@.tmp $@ && \
	rm -f $@.tmp
EXTRA_DIST += shell-enum-types.c.in

libgnome_shell_la_LDFLAGS = -avoid-version -module
libgnome_shell_la_LIBADD =	\
	-lm			\
	$(MUTTER_PLUGIN_LIBS)	\
	$(LIBGNOMEUI_LIBS)      \
	$(BLUETOOTH_LIBS)	\
	libst-1.0.la       	\
	libgdmuser-1.0.la	\
	libtray.la		\
	libgvc.la		\
	$(NULL)

libgnome_shell_la_CPPFLAGS = $(gnome_shell_cflags) $(LIBECAL_CFLAGS)

Shell-0.1.gir: libgnome-shell.la St-1.0.gir
Shell_0_1_gir_INCLUDES = Clutter-1.0 ClutterX11-1.0 Meta-2.91 TelepathyGLib-0.12
Shell_0_1_gir_CFLAGS = $(libgnome_shell_la_CPPFLAGS) -I $(srcdir)
Shell_0_1_gir_LIBS = libgnome-shell.la
Shell_0_1_gir_FILES = $(addprefix $(srcdir)/,$(libgnome_shell_la_gir_sources))
Shell_0_1_gir_SCANNERFLAGS = --include-uninstalled=$(builddir)/St-1.0.gir \
	--add-include-path=$(MUTTER_LIB_DIR)/mutter/ \
	--program=mutter \
	--program-arg=--mutter-plugins=`pwd`/libgnome-shell.la
INTROSPECTION_GIRS += Shell-0.1.gir

St-1.0.gir: libst-1.0.la
St_1_0_gir_INCLUDES = Clutter-1.0 Gtk-3.0
St_1_0_gir_CFLAGS = $(st_cflags) -DST_COMPILATION
St_1_0_gir_LIBS = libst-1.0.la
St_1_0_gir_FILES = $(filter-out %-private.h $(st_non_gir_sources), $(addprefix $(srcdir)/,$(st_source_h))) \
	$(addprefix $(srcdir)/,$(st_source_c))
INTROSPECTION_GIRS += St-1.0.gir

Gdm-1.0.gir: libgdmuser-1.0.la
Gdm_1_0_gir_INCLUDES = GObject-2.0 GdkPixbuf-2.0
Gdm_1_0_gir_CFLAGS = $(gdmuser_cflags)
Gdm_1_0_gir_LIBS = libgdmuser-1.0.la
Gdm_1_0_gir_FILES = $(filter-out %-private.h, $(addprefix $(srcdir)/,$(gdmuser_source_h))) \
	$(addprefix $(srcdir)/,$(gdmuser_source_c))
INTROSPECTION_GIRS += Gdm-1.0.gir
