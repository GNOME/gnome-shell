BUILT_SOURCES =
CLEANFILES =

include Makefile-tidy.am
include Makefile-tray.am

gnome_shell_cflags =				\
	$(MUTTER_PLUGIN_CFLAGS)			\
	-Itray					\
	-DGETTEXT_PACKAGE=gnome-shell		\
	-DJSDIR=\"$(pkgdatadir)/js\"

plugindir = $(libdir)/metacity/plugins/clutter
plugin_LTLIBRARIES = libgnome-shell.la

libgnome_shell_la_SOURCES =			\
	gnome-shell-plugin.c			\
	shell-process.c				\
	shell-process.h				\
	shell-global.c				\
	shell-global.h				\
	shell-tray-manager.c			\
	shell-tray-manager.h

libgnome_shell_la_LDFLAGS = -avoid-version -module
libgnome_shell_la_LIBADD =	\
	$(MUTTER_PLUGIN_LIBS)	\
	libtidy-1.0.la		\
	libtray.la
libgnome_shell_la_CPPFLAGS = $(gnome_shell_cflags)

typelibdir = $(pkglibdir)/girepository
typelib_DATA = Shell-0.1.typelib Tidy-1.0.typelib

# After we run g-ir-scanner, we need to change the library name written in
# the .gir file from the "fake" second copy of the library to the real name
Shell-0.1.gir: $(metacity) $(G_IR_SCANNER) $(libgnome_shell_la_SOURCES) Makefile
	$(G_IR_SCANNER)		\
		--namespace=Shell			\
		--nsversion=0.1				\
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
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler Shell-0.1.gir -o $@
CLEANFILES += Shell-1.0.typelib

# After we run g-ir-scanner, we need to change the library name written in
# the .gir file from the "fake" second copy of the library to the real name
Tidy-1.0.gir: $(libgnome_shell_la_SOURCES) Makefile
	$(G_IR_SCANNER)					\
		--namespace=Tidy			\
		--nsversion=1.0				\
		--include=Clutter-0.8			\
		--library=tidy-1.0			\
		--pkg=clutter-0.8			\
		$(tidy_source_h)			\
		$(tidy_source_c)			\
		$(tidy_cflags)				\
		-o $@
CLEANFILES += Tidy-1.0.gir

Tidy-1.0.typelib: libtidy-1.0.la Tidy-1.0.gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler Tidy-1.0.gir -o $@
CLEANFILES += Tidy-1.0.typelib