CLEANFILES =
NULL =

portaldir = $(datadir)/xdg-desktop-portal/portals
portal_DATA = gnome-shell.portal

desktopdir=$(datadir)/applications
desktop_DATA = org.gnome.Shell.desktop gnome-shell-extension-prefs.desktop

if HAVE_NETWORKMANAGER
desktop_DATA += org.gnome.Shell.PortalHelper.desktop

servicedir = $(datadir)/dbus-1/services
service_DATA = org.gnome.Shell.PortalHelper.service

CLEANFILES += \
	org.gnome.Shell.PortalHelper.service 	\
	org.gnome.Shell.PortalHelper.desktop 	\
	org.gnome.Shell.PortalHelper.desktop.in \
	$(NULL)

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

%.desktop:%.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $(builddir)/$< \
	                      -d $(top_srcdir)/po -o $@

introspectiondir = $(datadir)/dbus-1/interfaces
introspection_DATA =				\
	org.gnome.Shell.PadOsd.xml		\
	org.gnome.Shell.Screencast.xml		\
	org.gnome.Shell.Screenshot.xml		\
	org.gnome.ShellSearchProvider.xml	\
	org.gnome.ShellSearchProvider2.xml	\
	$(NULL)

theme_sources = 						\
	theme/gnome-shell-high-contrast.scss			\
	theme/gnome-shell.scss					\
	theme/gnome-shell-sass/_colors.scss			\
	theme/gnome-shell-sass/_common.scss			\
	theme/gnome-shell-sass/_drawing.scss			\
	theme/gnome-shell-sass/_high-contrast-colors.scss	\
	$(NULL)

dist_theme_files =						\
	$(theme_sources)					\
	theme/HACKING						\
	theme/README						\
	theme/gnome-shell-sass/COPYING				\
	theme/gnome-shell-sass/HACKING				\
	theme/gnome-shell-sass/NEWS				\
	theme/gnome-shell-sass/README				\
	theme/gnome-shell-sass/gnome-shell-sass.doap		\
	theme/pad-osd.css					\
	theme/parse-sass.sh					\
	$(NULL)

%.css: %.scss $(theme_sources)
	@if test -n "$(SASSC)"; then \
		if $(AM_V_P); then PS4= set -x; else echo "  GEN      $@"; fi; \
		$(SASSC) -a $< $@; \
	fi

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
keys_DATA = 50-gnome-shell-system.xml

gsettings_SCHEMAS = org.gnome.shell.gschema.xml

%.gschema.xml: %.gschema.xml.in Makefile
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
	org.gnome.Shell.desktop.in.in			\
	gnome-shell-extension-prefs.desktop.in.in	\
	$(portal_DATA)					\
	$(introspection_DATA)				\
	$(menu_DATA)					\
	$(convert_DATA)					\
	$(keys_DATA)					\
	$(dist_theme_files)				\
	perf-background.xml.in				\
	org.gnome.Shell.PortalHelper.desktop.in.in	\
	org.gnome.Shell.PortalHelper.service.in		\
	org.gnome.shell.gschema.xml.in			\
	gnome-shell-theme.gresource.xml 		\
	$(resource_files)				\
	$(NULL)

CLEANFILES +=						\
	org.gnome.Shell.desktop.in			\
	gnome-shell-extension-prefs.in			\
	$(desktop_DATA)					\
	$(gsettings_SCHEMAS)				\
	perf-background.xml				\
	gschemas.compiled				\
	org.gnome.shell.gschema.valid			\
	gnome-shell-theme.gresource			\
	$(NULL)
