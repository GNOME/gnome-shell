NULL = 
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =
libexec_PROGRAMS =
noinst_LTLIBRARIES =
noinst_PROGRAMS =

.AUTOPARALLEL:

bin_SCRIPTS = gnome-shell

gnome-shell: gnome-shell.in
	$(AM_V_GEN) sed -e "s|@MUTTER_BIN_DIR[@]|$(MUTTER_BIN_DIR)|" \
	    -e "s|@GJS_JS_DIR[@]|$(GJS_JS_DIR)|" \
	    -e "s|@GJS_JS_NATIVE_DIR[@]|$(GJS_JS_NATIVE_DIR)|" \
	    -e "s|@libexecdir[@]|$(libexecdir)|" \
	    -e "s|@libdir[@]|$(libdir)|" \
	    -e "s|@pkgdatadir[@]|$(pkgdatadir)|" \
	    -e "s|@PYTHON[@]|$(PYTHON)|" \
	    -e "s|@sysconfdir[@]|$(sysconfdir)|" \
	    $< > $@ && chmod a+x $@
CLEANFILES += gnome-shell
EXTRA_DIST += gnome-shell.in

include Makefile-big.am
include Makefile-gdmuser.am
include Makefile-st.am
include Makefile-tray.am

gnome_shell_cflags =				\
	$(MUTTER_PLUGIN_CFLAGS)			\
        $(LIBGNOMEUI_CFLAGS)                    \
	-I$(srcdir)/tray			\
	-DGETTEXT_PACKAGE=\"gnome-shell\"		\
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DGNOME_SHELL_DATADIR=\"$(pkgdatadir)\"	\
	-DGNOME_SHELL_PKGLIBDIR=\"$(pkglibdir)\" \
	-DJSDIR=\"$(pkgdatadir)/js\"

plugindir = $(MUTTER_PLUGIN_DIR)
plugin_LTLIBRARIES = libgnome-shell.la

shell_built_sources = \
	shell-marshal.h \
	shell-marshal.c

BUILT_SOURCES += $(shell_built_sources)
EXTRA_DIST += shell-marshal.list

SHELL_STAMP_FILES = stamp-shell-marshal.h
CLEANFILES += $(SHELL_STAMP_FILES)

libgnome_shell_la_SOURCES =			\
	$(shell_built_sources)			\
	gnome-shell-plugin.c			\
	shell-app.c         \
	shell-app.h         \
	shell-app-private.h \
	shell-app-system.c			\
	shell-app-system.h			\
	shell-app-usage.c			\
	shell-app-usage.h			\
	shell-arrow.c			\
	shell-arrow.h			\
	shell-button-box.c           \
	shell-button-box.h           \
	shell-drawing.c            \
	shell-drawing.h            \
	shell-drawing-area.c			\
	shell-drawing-area.h			\
	shell-embedded-window.c			\
	shell-embedded-window.h			\
	shell-embedded-window-private.h		\
	shell-gconf.c				\
	shell-gconf.h				\
	shell-generic-container.c           \
	shell-generic-container.h           \
	shell-gtk-embed.c			\
	shell-gtk-embed.h			\
	shell-menu.c       \
	shell-menu.h       \
	shell-overflow-list.c		\
	shell-overflow-list.h		\
	shell-process.c				\
	shell-process.h				\
	shell-global.c				\
	shell-global.h				\
	shell-global-private.h			\
	shell-status-menu.c				\
	shell-status-menu.h				\
	shell-stack.c				\
	shell-stack.h				\
	shell-tray-manager.c			\
	shell-tray-manager.h			\
	shell-texture-cache.c			\
	shell-texture-cache.h			\
	shell-uri-util.c           \
	shell-uri-util.h           \
	shell-window-tracker.c		\
	shell-window-tracker.h  \
	shell-wm.c				\
	shell-wm.h

non_gir_sources =				\
	shell-embedded-window-private.h		\
	shell-global-private.h

shell_recorder_sources =        \
	shell-recorder.c	\
	shell-recorder.h	\
	shell-recorder-src.c	\
	shell-recorder-src.h

# Custom element is an internal detail
shell_recorder_non_gir_sources =  \
	shell-recorder-src.c	  \
	shell-recorder-src.h

if BUILD_RECORDER
libgnome_shell_la_SOURCES += $(shell_recorder_sources)
non_gir_sources += $(shell_recorder_non_gir_sources)

noinst_PROGRAMS += test-recorder

test_recorder_CPPFLAGS = $(TEST_SHELL_RECORDER_CFLAGS)
test_recorder_LDADD = $(TEST_SHELL_RECORDER_LIBS)

test_recorder_SOURCES =     \
	$(shell_recorder_sources) \
	test-recorder.c
endif BUILD_RECORDER

libgnome_shell_la_gir_sources = \
	$(filter-out $(non_gir_sources), $(libgnome_shell_la_SOURCES))

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

shell-marshal.c: Makefile shell-marshal.list
	$(AM_V_GEN) (echo "#include \"shell-marshal.h\"" ; \
	 $(GLIB_GENMARSHAL) \
		--prefix=_shell_marshal \
		--body \
	 $(srcdir)/shell-marshal.list ) > xgen-smc && \
	cp -f xgen-smc shell-marshal.c && \
	rm -f xgen-smc

libgnome_shell_la_LDFLAGS = -avoid-version -module
libgnome_shell_la_LIBADD =	\
	$(MUTTER_PLUGIN_LIBS)	\
        $(LIBGNOMEUI_LIBS)      \
	libbig-1.0.la		\
	libst-1.0.la       	\
	libgdmuser-1.0.la	\
	libtray.la
libgnome_shell_la_CPPFLAGS = $(gnome_shell_cflags)

typelibdir = $(pkglibdir)
typelib_DATA = Shell-0.1.typelib Big-1.0.typelib St-1.0.typelib

Shell-0.1.gir: $(mutter) $(G_IR_SCANNER) Big-1.0.gir St-1.0.gir libgnome-shell.la Makefile
	$(AM_V_GEN) $(G_IR_SCANNER)			\
		--namespace=Shell			\
		--nsversion=0.1				\
		--add-include-path=$(MUTTER_LIB_DIR)/mutter/ \
		--include=Clutter-1.0			\
		--include=Meta-2.28			\
		--libtool="$(LIBTOOL)"    \
		--add-include-path=$(builddir)     \
		--include=Big-1.0     \
		--include=St-1.0     \
		--program=mutter			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(addprefix $(srcdir)/,$(libgnome_shell_la_gir_sources)) \
		$(libgnome_shell_la_CPPFLAGS)		\
		-o $@
CLEANFILES += Shell-0.1.gir

# The dependency on libgnome-shell.la here is because g-ir-compiler opens it
# (not the fake library, since we've already done the rewriting)
Shell-0.1.typelib: libgnome-shell.la Shell-0.1.gir Big-1.0.gir
	$(AM_V_GEN) \
		$(G_IR_COMPILER)						\
			--includedir=.						\
			--includedir=$(MUTTER_LIB_DIR)/mutter/			\
		Shell-0.1.gir -o $@
CLEANFILES += Shell-0.1.typelib

Big-1.0.gir: $(mutter) $(G_IR_SCANNER) libgnome-shell.la libbig-1.0.la Makefile
	$(AM_V_GEN) $(G_IR_SCANNER)			\
		--namespace=Big				\
		--nsversion=1.0				\
		--include=Clutter-1.0			\
		--include=GdkPixbuf-2.0			\
		--libtool="$(LIBTOOL)"    \
		--program=mutter			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(addprefix $(srcdir)/,$(big_source_h))	\
		$(addprefix $(srcdir)/,$(big_source_c))	\
		$(srcdir)/big-enum-types.h		\
		$(big_cflags)				\
		-o $@
CLEANFILES += Big-1.0.gir

Big-1.0.typelib: libbig-1.0.la Big-1.0.gir
	$(AM_V_GEN) $(G_IR_COMPILER) Big-1.0.gir -o $@
CLEANFILES += Big-1.0.typelib

St-1.0.gir: $(mutter) $(G_IR_SCANNER) libgnome-shell.la libst-1.0.la Makefile
	$(AM_V_GEN) $(G_IR_SCANNER)						\
	        --namespace=St							\
	        --nsversion=1.0							\
	        --include=Clutter-1.0						\
		--add-include-path=$(builddir)     				\
	        --libtool="$(LIBTOOL)"						\
	        --program=mutter						\
		--program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la	\
	        -DST_COMPILATION						\
	        $(addprefix $(srcdir)/,$(st_source_h))				\
	        $(addprefix $(srcdir)/,$(st_source_c))				\
	        $(srcdir)/st-enum-types.h					\
	        $(st_cflags)							\
	        -o $@
CLEANFILES += St-1.0.gir

St-1.0.typelib: St-1.0.gir
	$(AM_V_GEN) $(G_IR_COMPILER)						\
		$< -o $@

CLEANFILES += St-1.0.typelib
