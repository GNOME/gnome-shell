imagedir = $(pkgdatadir)/images

dist_image_DATA =		\
	add-workspace.svg	\
	info.svg			\
	remove-workspace.svg

schemadir  = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = gnome-shell.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$(schema_DATA)

EXTRA_DIST =			\
	$(schema_DATA)
