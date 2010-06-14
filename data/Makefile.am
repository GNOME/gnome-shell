desktopdir=$(datadir)/applications
desktop_DATA = gnome-shell.desktop gnome-shell-clock-preferences.desktop

# We substitute in bindir so it works as an autostart
# file when built in a non-system prefix
%.desktop.in:%.desktop.in.in
	$(AM_V_GEN) sed -e "s|@bindir[@]|$(bindir)|" \
	    -e "s|@VERSION[@]|$(VERSION)|" \
	    $< > $@ || rm $@

# Placeholder until we add intltool
%.desktop:%.desktop.in
	$(AM_V_GEN) sed s/^_// < $< > $@ || rm $@

dist_pkgdata_DATA = clock-preferences.ui

imagesdir = $(pkgdatadir)/images
dist_images_DATA =				\
	close-black.svg				\
	magnifier.svg

themedir = $(pkgdatadir)/theme
dist_theme_DATA =				\
	theme/add-workspace.svg			\
	theme/close-window.svg			\
	theme/close.svg                 \
	theme/corner-ripple.png			\
	theme/dialog-error.svg			\
	theme/gnome-shell.css			\
	theme/mosaic-view-active.svg          \
	theme/mosaic-view.svg          \
	theme/move-window-on-new.svg          \
	theme/remove-workspace.svg          \
	theme/scroll-button-down-hover.png	\
	theme/scroll-button-down.png		\
	theme/scroll-button-up-hover.png	\
	theme/scroll-button-up.png		\
	theme/scroll-hhandle.svg        \
	theme/scroll-vhandle.svg        \
	theme/section-more.svg          \
	theme/section-more-open.svg          \
	theme/separator-white.png          \
	theme/single-view-active.svg          \
	theme/single-view.svg          \
	theme/ws-switch-arrow-left.svg		\
	theme/ws-switch-arrow-right.svg


schemadir  = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = gnome-shell.schemas

menudir = $(sysconfdir)/xdg/menus

menu_DATA = \
	gs-applications.menu

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

EXTRA_DIST =						\
	gnome-shell.desktop.in.in			\
	gnome-shell-clock-preferences.desktop.in.in	\
	$(menu_DATA)					\
	$(schema_DATA)

CLEANFILES =						\
	gnome-shell.desktop.in				\
	gnome-shell-clock-preferences.desktop.in	\
	$(desktop_DATA)

