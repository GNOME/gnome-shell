
mozillalibdir = $(BROWSER_PLUGIN_DIR)

mozillalib_LTLIBRARIES = libgnome-shell-browser-plugin.la

# Browsers can unload and reload the module while browsing, which is not supported by GObject.
# We pass -Wl,-z,nodelete to the linker to ensure the module is never unloaded.
# https://bugzilla.gnome.org/show_bug.cgi?id=737932
libgnome_shell_browser_plugin_la_LDFLAGS = -module -avoid-version -no-undefined -Wl,-z,nodelete

libgnome_shell_browser_plugin_la_LIBADD = 	\
	$(BROWSER_PLUGIN_LIBS)

libgnome_shell_browser_plugin_la_SOURCES = 	\
	browser-plugin.c \
	npapi/npapi.h \
	npapi/npfunctions.h \
	npapi/npruntime.h \
	npapi/nptypes.h

libgnome_shell_browser_plugin_la_CFLAGS = 	\
	$(BROWSER_PLUGIN_CFLAGS)		\
	-DG_LOG_DOMAIN=\"GnomeShellBrowserPlugin\"
