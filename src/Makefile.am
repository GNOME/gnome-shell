NULL = 
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = 
libexec_PROGRAMS =
noinst_LTLIBRARIES = 

bin_SCRIPTS = gnome-shell

gnome-shell: gnome-shell.in
	sed -e "s|@META_BIN_DIR[@]|$(META_BIN_DIR)|" \
	    -e "s|@GJS_JS_DIR[@]|$(GJS_JS_DIR)|" \
	    -e "s|@GJS_JS_NATIVE_DIR[@]|$(GJS_JS_NATIVE_DIR)|" \
	    -e "s|@libexecdir[@]|$(libexecdir)|" \
	    -e "s|@libdir[@]|$(libdir)|" \
	    -e "s|@pkgdatadir[@]|$(pkgdatadir)|" \
	    $< > $@ && chmod a+x $@
CLEANFILES += gnome-shell
EXTRA_DIST += gnome-shell.in

include Makefile-tidy.am
include Makefile-big.am
include Makefile-gdmuser.am
include Makefile-tray.am
include Makefile-taskpanel.am

gnome_shell_cflags =				\
	$(MUTTER_PLUGIN_CFLAGS)			\
        $(LIBGNOMEUI_CFLAGS)                    \
	-I$(srcdir)/tray			\
	-DGETTEXT_PACKAGE=gnome-shell		\
	-DGNOME_SHELL_DATADIR=\"$(pkgdatadir)\"	\
	-DGNOME_SHELL_PKGLIBDIR=\"$(pkglibdir)\" \
	-DJSDIR=\"$(pkgdatadir)/js\"

plugindir = $(libdir)/metacity/plugins/clutter
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
	shell-app-monitor.c			\
	shell-app-monitor.h			\
	shell-arrow.c			\
	shell-arrow.h			\
	shell-gtkwindow-actor.c			\
	shell-gtkwindow-actor.h			\
	shell-process.c				\
	shell-process.h				\
	shell-global.c				\
	shell-global.h				\
	shell-status-menu.c				\
	shell-status-menu.h				\
	shell-tray-manager.c			\
	shell-tray-manager.h			\
	shell-wm.c				\
	shell-wm.h

# ClutterGLXTexturePixmap is currently not wrapped
non_gir_sources = shell-gtkwindow-actor.h

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

noinst_PROGRAMS = test-recorder

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
	$(GLIB_GENMARSHAL) \
		--prefix=_shell_marshal \
		--header \
	$(srcdir)/shell-marshal.list > xgen-tmh && \
	(cmp -s xgen-tmh shell-marshal.h || cp -f xgen-tmh shell-marshal.h) && \
	rm -f xgen-tmh && \
	echo timestamp > $(@F)

shell-marshal.c: Makefile shell-marshal.list
	(echo "#include \"shell-marshal.h\"" ; \
	 $(GLIB_GENMARSHAL) \
		--prefix=_shell_marshal \
		--body \
	 $(srcdir)/shell-marshal.list ) > xgen-tmc && \
	cp -f xgen-tmc shell-marshal.c && \
	rm -f xgen-tmc

libgnome_shell_la_LDFLAGS = -avoid-version -module
libgnome_shell_la_LIBADD =	\
	$(MUTTER_PLUGIN_LIBS)	\
        $(LIBGNOMEUI_LIBS)      \
	libbig-1.0.la		\
	libgdmuser-1.0.la	\
	libtidy-1.0.la		\
	libtray.la
libgnome_shell_la_CPPFLAGS = $(gnome_shell_cflags)

typelibdir = $(pkglibdir)
typelib_DATA = Shell-0.1.typelib Tidy-1.0.typelib Big-1.0.typelib

Shell-0.1.gir: $(metacity) $(G_IR_SCANNER) Big-1.0.gir libgnome-shell.la Makefile
	$(G_IR_SCANNER)		\
		--namespace=Shell			\
		--nsversion=0.1				\
		--add-include-path=$(META_LIB_DIR)/metacity/ \
		--include=Clutter-0.9			\
		--include=Meta-2.25			\
		--add-include-path=$(builddir)     \
		--include=Big-1.0     \
		--program=metacity			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(addprefix $(srcdir)/,$(libgnome_shell_la_gir_sources)) \
		$(libgnome_shell_la_CPPFLAGS)		\
		-o $@
CLEANFILES += Shell-0.1.gir

# The dependency on libgnome-shell.la here is because g-ir-compiler opens it
# (not the fake library, since we've already done the rewriting)
Shell-0.1.typelib: libgnome-shell.la Shell-0.1.gir Big-1.0.gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler --includedir=$(builddir) --includedir=$(META_LIB_DIR)/metacity/ Shell-0.1.gir -o $@
CLEANFILES += Shell-0.1.typelib

Tidy-1.0.gir: $(metacity) $(G_IR_SCANNER) libgnome-shell.la libtidy-1.0.la Makefile
	$(G_IR_SCANNER)					\
		--namespace=Tidy			\
		--nsversion=1.0				\
		--include=Clutter-0.9			\
		--program=metacity			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(addprefix $(srcdir)/,$(tidy_source_h)) \
		$(addprefix $(srcdir)/,$(tidy_source_c)) \
		$(srcdir)/tidy-enum-types.h		\
		$(tidy_cflags)				\
		-o $@
CLEANFILES += Tidy-1.0.gir

Tidy-1.0.typelib: libtidy-1.0.la Tidy-1.0.gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler Tidy-1.0.gir -o $@
CLEANFILES += Tidy-1.0.typelib

Big-1.0.gir: $(metacity) $(G_IR_SCANNER) libgnome-shell.la libbig-1.0.la Makefile
	$(G_IR_SCANNER)					\
		--namespace=Big			\
		--nsversion=1.0				\
		--include=Clutter-0.9			\
		--include=GdkPixbuf-2.0			\
		--program=metacity			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(addprefix $(srcdir)/,$(big_source_h))	\
		$(addprefix $(srcdir)/,$(big_source_c))	\
		$(srcdir)/big-enum-types.h		\
		$(big_cflags)				\
		-o $@
CLEANFILES += Big-1.0.gir

Big-1.0.typelib: libbig-1.0.la Big-1.0.gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler Big-1.0.gir -o $@
CLEANFILES += Big-1.0.typelib
