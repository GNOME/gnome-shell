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

imagedir = $(pkgdatadir)/images

dist_image_DATA =		\
	add-workspace.svg	\
	close.svg		\
	info.svg		\
	magnifier.svg  \
	remove-workspace.svg

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

