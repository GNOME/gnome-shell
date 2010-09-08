NULL = 
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =
libexec_PROGRAMS =
noinst_LTLIBRARIES =
noinst_PROGRAMS =

.AUTOPARALLEL:

bin_SCRIPTS = gnome-shell gnome-shell-clock-preferences

gnome-shell: gnome-shell.in
	$(AM_V_GEN) sed -e "s|@MUTTER_BIN_DIR[@]|$(MUTTER_BIN_DIR)|" \
	    -e "s|@datadir[@]|$(datadir)|" \
	    -e "s|@GJS_JS_DIR[@]|$(GJS_JS_DIR)|" \
	    -e "s|@GJS_JS_NATIVE_DIR[@]|$(GJS_JS_NATIVE_DIR)|" \
	    -e "s|@libexecdir[@]|$(libexecdir)|" \
	    -e "s|@libdir[@]|$(libdir)|" \
	    -e "s|@pkgdatadir[@]|$(pkgdatadir)|" \
	    -e "s|@PYTHON[@]|$(PYTHON)|" \
	    -e "s|@VERSION[@]|$(VERSION)|" \
	    -e "s|@sysconfdir[@]|$(sysconfdir)|" \
	    $< > $@ && chmod a+x $@
CLEANFILES += gnome-shell
EXTRA_DIST += gnome-shell.in

gnome-shell-clock-preferences: gnome-shell-clock-preferences.in
	$(AM_V_GEN) sed -e "s|@datadir[@]|$(datadir)|" \
	    -e "s|@pkgdatadir[@]|$(pkgdatadir)|" \
	    -e "s|@localedir[@]|$(datadir)/locale|" \
	    -e "s|@GJS_CONSOLE[@]|$(GJS_CONSOLE)|" \
	    $< > $@ && chmod a+x $@
CLEANFILES += gnome-shell-clock-preferences
EXTRA_DIST += gnome-shell-clock-preferences.in

include Makefile-gdmuser.am
include Makefile-st.am
include Makefile-tray.am

gnome_shell_cflags =				\
	$(MUTTER_PLUGIN_CFLAGS)			\
        $(LIBGNOMEUI_CFLAGS)                    \
	-I$(srcdir)/tray			\
	-DLOCALEDIR=\"$(datadir)/locale\" \
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
	shell-drawing.h			\
	shell-embedded-window.h		\
	shell-generic-container.h	\
	shell-gtk-embed.h		\
	shell-process.h			\
	shell-global.h			\
	shell-perf-log.h		\
	shell-slicer.h			\
	shell-stack.h			\
	shell-tray-icon.h		\
	shell-tray-manager.h		\
	shell-uri-util.h		\
	shell-window-tracker.h		\
	shell-wm.h              \
	shell-xfixes-cursor.h

libgnome_shell_la_SOURCES =		\
	$(shell_built_sources)		\
	$(shell_public_headers_h)	\
	shell-app-private.h		\
	shell-embedded-window-private.h	\
	shell-global-private.h		\
	shell-window-tracker-private.h	\
	shell-wm-private.h	\
	gnome-shell-plugin.c		\
	gtk-compat.h			\
	shell-app.c			\
	shell-app-system.c		\
	shell-app-usage.c		\
	shell-arrow.c			\
	shell-doc-system.c		\
	shell-drawing.c			\
	shell-embedded-window.c		\
	shell-generic-container.c	\
	shell-gtk-embed.c		\
	shell-process.c			\
	shell-global.c			\
	shell-perf-log.c		\
	shell-slicer.c			\
	shell-stack.c			\
	shell-tray-icon.c		\
	shell-tray-manager.c		\
	shell-uri-util.c		\
	shell-window-tracker.c		\
	shell-wm.c              \
	shell-xfixes-cursor.c

libgnome_shell_la_gir_sources = \
	$(filter-out %-private.h $(shell_recorder_non_gir_sources), $(shell_public_headers_h) $(libgnome_shell_la_SOURCES))

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
libgnome_shell_la_LIBADD =-lm \
	$(MUTTER_PLUGIN_LIBS)	\
        $(LIBGNOMEUI_LIBS)      \
	libst-1.0.la       	\
	libgdmuser-1.0.la	\
	libtray.la
libgnome_shell_la_CPPFLAGS = $(gnome_shell_cflags)

typelibdir = $(pkglibdir)
typelib_DATA = Shell-0.1.typelib St-1.0.typelib Gdm-1.0.typelib

Shell-0.1.gir: $(mutter) $(G_IR_SCANNER) St-1.0.gir libgnome-shell.la Makefile
	$(AM_V_GEN) $(G_IR_SCANNER)			\
		--namespace=Shell			\
		--nsversion=0.1				\
		--warn-all				\
		--warn-error				\
		--add-include-path=$(MUTTER_LIB_DIR)/mutter/ \
		--include=Clutter-1.0			\
		--include=ClutterX11-1.0		\
		--include=Meta-2.31			\
		--libtool="$(LIBTOOL)"    \
		--add-include-path=$(builddir)     \
		--include=St-1.0     \
		--program=mutter			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(addprefix $(srcdir)/,$(libgnome_shell_la_gir_sources)) \
		$(libgnome_shell_la_CPPFLAGS)		\
		-I $(srcdir)				\
		-o $@
CLEANFILES += Shell-0.1.gir
EXTRA_DIST += fix-meta-rectangle.py

# The dependency on libgnome-shell.la here is because g-ir-compiler opens it
# (not the fake library, since we've already done the rewriting)
Shell-0.1.typelib: libgnome-shell.la Shell-0.1.gir
	$(AM_V_GEN) \
		$(G_IR_COMPILER)						\
			--includedir=.						\
			--includedir=$(MUTTER_LIB_DIR)/mutter/			\
		Shell-0.1.gir -o $@
CLEANFILES += Shell-0.1.typelib

St-1.0.gir: $(mutter) $(G_IR_SCANNER) libst-1.0.la Makefile
	$(AM_V_GEN) $(G_IR_SCANNER)						\
	        --namespace=St							\
	        --nsversion=1.0							\
		--warn-all							\
		--warn-error							\
	        --include=Clutter-1.0						\
	        --include=Gtk-3.0						\
		--add-include-path=$(builddir)     				\
	        --libtool="$(LIBTOOL)"						\
	        --library=libst-1.0.la						\
	        -DST_COMPILATION						\
	        $(filter-out %-private.h, $(addprefix $(srcdir)/,$(st_source_h))) \
	        $(addprefix $(srcdir)/,$(st_source_c))				\
	        $(srcdir)/st-enum-types.h					\
	        $(st_cflags)							\
	        -o $@
CLEANFILES += St-1.0.gir

St-1.0.typelib: St-1.0.gir
	$(AM_V_GEN) $(G_IR_COMPILER)						\
		$< -o $@

CLEANFILES += St-1.0.typelib

Gdm-1.0.gir: $(mutter) $(G_IR_SCANNER) libgdmuser-1.0.la Makefile
	$(AM_V_GEN) $(G_IR_SCANNER)			\
		--namespace=Gdm				\
		--nsversion=1.0				\
		--warn-all				\
		--warn-error				\
		--include=GObject-2.0			\
		--include=GdkPixbuf-2.0			\
		--libtool="$(LIBTOOL)"			\
		--library=libgdmuser-1.0.la		\
		$(filter-out %-private.h, $(addprefix $(srcdir)/,$(gdmuser_source_h)))	\
		$(addprefix $(srcdir)/,$(gdmuser_source_c))	\
		$(gdmuser_cflags)				\
		-o $@
CLEANFILES += Gdm-1.0.gir

Gdm-1.0.typelib: Gdm-1.0.gir
	$(AM_V_GEN) $(G_IR_COMPILER) Gdm-1.0.gir -o $@
CLEANFILES += Gdm-1.0.typelib

