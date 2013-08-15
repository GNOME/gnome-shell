wandadir = $(pkgdatadir)
dist_wanda_DATA = wanda.png

desktopdir=$(datadir)/applications
desktop_DATA = gnome-shell.desktop gnome-shell-extension-prefs.desktop

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

themedir = $(pkgdatadir)/theme
dist_theme_DATA =				\
	theme/calendar-arrow-left.svg		\
	theme/calendar-arrow-right.svg		\
	theme/calendar-today.svg		\
	theme/checkbox-focused.svg		\
	theme/checkbox-off-focused.svg		\
	theme/checkbox-off.svg			\
	theme/checkbox.svg			\
	theme/close-window.svg			\
	theme/close.svg				\
	theme/corner-ripple-ltr.png		\
	theme/corner-ripple-rtl.png		\
	theme/dash-placeholder.svg		\
	theme/filter-selected-ltr.svg		\
	theme/filter-selected-rtl.svg		\
	theme/gnome-shell.css			\
	theme/logged-in-indicator.svg		\
	theme/message-tray-background.png	\
	theme/more-results.svg			\
	theme/noise-texture.png			\
	theme/panel-button-border.svg		\
	theme/panel-button-highlight-narrow.svg	\
	theme/panel-button-highlight-wide.svg	\
	theme/process-working.svg		\
	theme/running-indicator.svg		\
	theme/source-button-border.svg		\
	theme/summary-counter.svg		\
	theme/toggle-off-us.svg			\
	theme/toggle-off-intl.svg		\
	theme/toggle-on-us.svg			\
	theme/toggle-on-intl.svg		\
	theme/ws-switch-arrow-up.png		\
	theme/ws-switch-arrow-down.png

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
	gnome-shell-extension-prefs.desktop.in.in	\
	$(introspection_DATA)				\
	$(menu_DATA)					\
	$(convert_DATA)					\
	$(keys_in_files)				\
	org.gnome.shell.gschema.xml.in.in

CLEANFILES =						\
	gnome-shell.desktop.in				\
	gnome-shell-extension-prefs.in			\
	$(desktop_DATA)					\
	$(keys_DATA)					\
	$(gsettings_SCHEMAS)				\
	gschemas.compiled				\
	org.gnome.shell.gschema.valid			\
	org.gnome.shell.gschema.xml.in
