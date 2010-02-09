desktopdir=$(datadir)/applications
desktop_DATA = gnome-shell.desktop

# We substitute in bindir so it works as an autostart
# file when built in a non-system prefix
gnome-shell.desktop.in: gnome-shell.desktop.in.in
	$(AM_V_GEN) sed -e "s|@bindir[@]|$(bindir)|" \
	    -e "s|@VERSION[@]|$(VERSION)|" \
	    $< > $@ || rm $@

# Placeholder until we add intltool
gnome-shell.desktop: gnome-shell.desktop.in
	$(AM_V_GEN) sed s/^_// < $< > $@ || rm $@

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
	theme/gnome-shell.css			\
	theme/mosaic-view-active.svg          \
	theme/mosaic-view.svg          \
	theme/remove-workspace.svg          \
	theme/scroll-button-down-hover.png	\
	theme/scroll-button-down.png		\
	theme/scroll-button-up-hover.png	\
	theme/scroll-button-up.png		\
	theme/scroll-hhandle.svg        \
	theme/scroll-vhandle.svg        \
	theme/section-back.svg          \
	theme/section-more.svg          \
	theme/single-view-active.svg          \
	theme/single-view.svg          \
	theme/ws-switch-arrow-left.svg		\
	theme/ws-switch-arrow-right.svg


schemadir  = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = gnome-shell.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

EXTRA_DIST =					\
	gnome-shell.desktop.in.in		\
	$(schema_DATA)

CLEANFILES =					\
	gnome-shell.desktop.in			\
	$(desktop_DATA)

