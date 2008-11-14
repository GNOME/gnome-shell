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

# We can't have any undefined symbols when g-ir-scanner dlopens the library
# to introspect it, so we link everything a _second_ time, including a
# stub file that just defines all the symbols that we use from metacity
noinst_LTLIBRARIES = libgnome-shell-introspect.la

libgnome_shell_introspect_la_SOURCES =		\
	$(libgnome_shell_la_SOURCES)		\
	metacity-symbols.c
# The dummy -rpath here is needed to convince libtool to build a
# noinst_LTLIBRARY shared
libgnome_shell_introspect_la_LDFLAGS = -avoid-version -module -rpath $(libdir)
libgnome_shell_introspect_la_LIBADD =	\
	$(MUTTER_PLUGIN_LIBS)		\
	libtidy-1.0.la			\
	libtray.la
libgnome_shell_introspect_la_CPPFLAGS = $(gnome_shell_cflags)

typelibdir = $(pkglibdir)/girepository
typelib_DATA = Shell-0.1.typelib Tidy-1.0.typelib

# After we run g-ir-scanner, we need to change the library name written in
# the .gir file from the "fake" second copy of the library to the real name
Shell-0.1.gir: libgnome-shell-introspect.la $(libgnome_shell_la_SOURCES) Makefile
	g-ir-scanner					\
		--namespace=Shell			\
		--nsversion=0.1				\
		--include=GObject-2.0			\
		--include=Clutter-0.8			\
		--include=Meta-2.25			\
		--library=gnome-shell-introspect	\
		$(libgnome_shell_la_SOURCES)		\
		$(libgnome_shell_la_CPPFLAGS)		\
		-o $@.tmp
	sed 's/gnome-shell-introspect/gnome-shell/' < $@.tmp > $@ && rm $@.tmp

# The dependency on libgnome-shell.la here is because g-ir-compiler opens it
# (not the fake library, since we've already done the rewriting)
Shell-0.1.typelib: libgnome-shell.la Shell-0.1.gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler Shell-0.1.gir -o $@

# After we run g-ir-scanner, we need to change the library name written in
# the .gir file from the "fake" second copy of the library to the real name
Tidy-1.0.gir: libgnome-shell-introspect.la $(libgnome_shell_la_SOURCES) Makefile
	g-ir-scanner					\
		--namespace=Tidy			\
		--nsversion=1.0				\
		--include=GObject-2.0			\
		--include=Clutter-0.8			\
		--library=gnome-shell-introspect	\
		$(tidy_source_h)			\
		$(tidy_source_c)			\
		$(tidy_cflags)				\
		-o $@.tmp
	sed 's/gnome-shell-introspect/gnome-shell/' < $@.tmp > $@ && rm $@.tmp

# The dependency on libgnome-shell.la here is because g-ir-compiler opens it
# (not the fake library, since we've already done the rewriting)
Tidy-1.0.typelib: libgnome-shell.la Tidy-1.0.gir
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. g-ir-compiler Tidy-1.0.gir -o $@
