imagedir = $(pkgdatadir)/images

dist_image_DATA =		\
	add-workspace.svg	\
	close.svg		\
	info.svg		\
	remove-workspace.svg	\
	view-more-activated.svg	\
	view-more.svg

schemadir  = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = gnome-shell.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

EXTRA_DIST =			\
	$(schema_DATA)
