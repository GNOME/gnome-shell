CLEANFILES =

desktopdir=$(datadir)/applications
desktop_DATA = gnome-shell.desktop gnome-shell-wayland.desktop  gnome-shell-extension-prefs.desktop

if HAVE_NETWORKMANAGER
desktop_DATA += org.gnome.Shell.PortalHelper.desktop

servicedir = $(datadir)/dbus-1/services
service_DATA = org.gnome.Shell.PortalHelper.service

CLEANFILES += \
	org.gnome.Shell.PortalHelper.service \
	org.gnome.Shell.PortalHelper.desktop

endif

%.service: %.service.in
	$(AM_V_GEN) sed -e "s|@libexecdir[@]|$(libexecdir)|" \
	    $< > $@ || rm $@

# We substitute in bindir so it works as an autostart
# file when built in a non-system prefix
%.desktop.in:%.desktop.in.in
	$(AM_V_GEN) sed -e "s|@bindir[@]|$(bindir)|" \
	    -e "s|@VERSION[@]|$(VERSION)|" \
	    $< > $@ || rm $@

@INTLTOOL_DESKTOP_RULE@

introspectiondir = $(datadir)/dbus-1/interfaces
introspection_DATA =				\
	org.gnome.Shell.Screencast.xml		\
	org.gnome.Shell.Screenshot.xml		\
	org.gnome.ShellSearchProvider.xml	\
	org.gnome.ShellSearchProvider2.xml

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/theme --generate-dependencies $(srcdir)/gnome-shell-theme.gresource.xml)
gnome-shell-theme.gresource: gnome-shell-theme.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir)/theme $<
resourcedir = $(pkgdatadir)
resource_DATA = gnome-shell-theme.gresource

backgrounddir = $(pkgdatadir)
background_DATA = perf-background.xml

perf-background.xml: perf-background.xml.in
	$(AM_V_GEN) sed -e "s|@datadir[@]|$(datadir)|" \
	    $< > $@ || rm $@

keysdir = @GNOME_KEYBINDINGS_KEYSDIR@
keys_in_files = 50-gnome-shell-system.xml.in
keys_DATA = $(keys_in_files:.xml.in=.xml)

gsettings_SCHEMAS = org.gnome.shell.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) sed -e 's|@GETTEXT_PACKAGE[@]|$(GETTEXT_PACKAGE)|g' \
	$< > $@ || rm $@

@GSETTINGS_RULES@

# We need to compile schemas at make time
# to run from source tree
gschemas.compiled: $(gsettings_SCHEMAS:.xml=.valid)
	$(AM_V_GEN) $(GLIB_COMPILE_SCHEMAS) --targetdir=. .

all-local: gschemas.compiled

convertdir = $(datadir)/GConf/gsettings
convert_DATA = gnome-shell-overrides.convert

EXTRA_DIST =						\
	gnome-shell.desktop.in.in			\
	gnome-shell-wayland.desktop.in.in		\
	gnome-shell-extension-prefs.desktop.in.in	\
	$(introspection_DATA)				\
	$(menu_DATA)					\
	$(convert_DATA)					\
	$(keys_in_files)				\
	perf-background.xml.in				\
	org.gnome.Shell.PortalHelper.desktop.in		\
	org.gnome.Shell.PortalHelper.service.in		\
	org.gnome.shell.gschema.xml.in.in		\
	gnome-shell-theme.gresource.xml 		\
	$(resource_files)

CLEANFILES +=						\
	gnome-shell.desktop.in				\
	gnome-shell-wayland.desktop.in			\
	gnome-shell-extension-prefs.in			\
	$(desktop_DATA)					\
	$(keys_DATA)					\
	$(gsettings_SCHEMAS)				\
	perf-background.xml				\
	gschemas.compiled				\
	org.gnome.shell.gschema.valid			\
	org.gnome.shell.gschema.xml.in			\
	gnome-shell-theme.gresource
