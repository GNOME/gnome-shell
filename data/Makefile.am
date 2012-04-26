desktopdir=$(datadir)/applications
desktop_DATA = gnome-shell.desktop gnome-shell-extension-prefs.desktop

# We substitute in bindir so it works as an autostart
# file when built in a non-system prefix
%.desktop.in:%.desktop.in.in
	$(AM_V_GEN) sed -e "s|@bindir[@]|$(bindir)|" \
	    -e "s|@VERSION[@]|$(VERSION)|" \
	    $< > $@ || rm $@

# Placeholder until we add intltool
%.desktop:%.desktop.in
	$(AM_V_GEN) sed s/^_// < $< > $@ || rm $@

searchprovidersdir = $(pkgdatadir)/open-search-providers
dist_searchproviders_DATA =				\
	open-search-providers/google.xml		\
	open-search-providers/wikipedia.xml

introspectiondir = $(datadir)/dbus-1/interfaces
introspection_DATA = org.gnome.ShellSearchProvider.xml

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
	theme/gdm.css				\
	theme/gnome-shell.css			\
	theme/panel-button-border.svg		\
	theme/panel-button-highlight-narrow.svg	\
	theme/panel-button-highlight-wide.svg	\
	theme/process-working.svg		\
	theme/running-indicator.svg		\
	theme/scroll-hhandle.svg		\
	theme/scroll-vhandle.svg		\
	theme/source-button-border.svg		\
	theme/toggle-off-us.svg			\
	theme/toggle-off-intl.svg		\
	theme/toggle-on-us.svg			\
	theme/toggle-on-intl.svg		\
	theme/ws-switch-arrow-up.svg		\
	theme/ws-switch-arrow-down.svg

gsettings_SCHEMAS = org.gnome.shell.gschema.xml org.gnome.shell.evolution.calendar.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

# We need to compile schemas at make time
# to run from source tree
gschemas.compiled: $(gsettings_SCHEMAS:.xml=.valid)
	$(AM_V_GEN) $(GLIB_COMPILE_SCHEMAS) --targetdir=. .

all-local: gschemas.compiled

convertdir = $(datadir)/GConf/gsettings
convert_DATA = gnome-shell-overrides.convert

shadersdir = $(pkgdatadir)/shaders
shaders_DATA = \
	shaders/dim-window.glsl


EXTRA_DIST =						\
	gnome-shell.desktop.in.in			\
	gnome-shell-extension-prefs.desktop.in.in	\
	$(introspection_DATA)				\
	$(menu_DATA)					\
	$(shaders_DATA)					\
	$(convert_DATA)					\
	org.gnome.shell.evolution.calendar.gschema.xml.in	\
	org.gnome.shell.gschema.xml.in

CLEANFILES =						\
	gnome-shell.desktop.in				\
	gnome-shell-extension-prefs.in			\
	$(desktop_DATA)					\
	$(gsettings_SCHEMAS)				\
	gschemas.compiled

