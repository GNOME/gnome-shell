NULL = 
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = 
noinst_LTLIBRARIES = 

include Makefile-tidy.am
include Makefile-big.am
include Makefile-tray.am

gnome_shell_cflags =				\
	$(MUTTER_PLUGIN_CFLAGS)			\
        $(LIBGNOMEUI_CFLAGS)                    \
	-Itray					\
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

SHELL_STAMP_FILES = stamp-shell-marshal.h
CLEANFILES += $(SHELL_STAMP_FILES)

libgnome_shell_la_SOURCES =			\
	$(shell_built_sources)			\
	gnome-shell-plugin.c			\
	shell-app-monitor.c			\
	shell-app-monitor.h			\
	shell-process.c				\
	shell-process.h				\
	shell-global.c				\
	shell-global.h				\
	shell-tray-manager.c			\
	shell-tray-manager.h			\
	shell-wm.c				\
	shell-wm.h

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
	libtidy-1.0.la		\
	libtray.la
libgnome_shell_la_CPPFLAGS = $(gnome_shell_cflags)

typelibdir = $(pkglibdir)
typelib_DATA = Shell-0.1.typelib Tidy-1.0.typelib Big-1.0.typelib

Shell-0.1.gir: $(metacity) $(G_IR_SCANNER) libgnome-shell.la Makefile
	$(G_IR_SCANNER)		\
		--namespace=Shell			\
		--nsversion=0.1				\
		--add-include-path=$(libdir)/metacity/ \
		--include=Clutter-0.8			\
		--include=Meta-2.25			\
		--program=metacity			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(libgnome_shell_la_SOURCES)		\
		$(libgnome_shell_la_CPPFLAGS)		\
		-o $@
CLEANFILES += Shell-1.0.gir

# The dependency on libgnome-shell.la here is because g-ir-compiler opens it
# (not the fake library, since we've already done the rewriting)
Shell-0.1.typelib: libgnome-shell.la Shell-0.1.gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler --includedir=$(libdir)/metacity/ Shell-0.1.gir -o $@
CLEANFILES += Shell-1.0.typelib

Tidy-1.0.gir: $(metacity) $(G_IR_SCANNER) libgnome-shell.la libtidy-1.0.la Makefile
	$(G_IR_SCANNER)					\
		--namespace=Tidy			\
		--nsversion=1.0				\
		--include=Clutter-0.8			\
		--program=metacity			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(tidy_source_h)			\
		$(tidy_source_c)			\
		tidy-enum-types.h			\
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
		--include=ClutterCairo-0.8			\
		--program=metacity			\
	        --program-arg=--mutter-plugins=$$(pwd)/libgnome-shell.la \
		$(big_source_h)			\
		$(big_source_c)			\
		big-enum-types.h		\
		$(big_cflags)				\
		-o $@
CLEANFILES += Big-1.0.gir

Big-1.0.typelib: libbig-1.0.la Big-1.0.gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler Big-1.0.gir -o $@
CLEANFILES += Big-1.0.typelib
