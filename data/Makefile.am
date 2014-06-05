desktopfiles_in_files = \
	mutter.desktop.in \
	mutter-wayland.desktop.in
desktopfilesdir = $(datadir)/applications
desktopfiles_DATA = $(desktopfiles_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

xml_in_files = \
        50-mutter-navigation.xml.in \
        50-mutter-system.xml.in \
        50-mutter-windows.xml.in
xmldir = $(GNOME_KEYBINDINGS_KEYSDIR)
xml_DATA = $(xml_in_files:.xml.in=.xml)

gschema_in_files = \
	org.gnome.mutter.gschema.xml.in	\
	org.gnome.mutter.wayland.gschema.xml.in
gsettings_SCHEMAS = $(gschema_in_files:.xml.in=.xml)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = mutter-schemas.convert

CLEANFILES = \
	$(desktopfiles_DATA) \
	$(gsettings_SCHEMAS) \
	$(xml_DATA)

EXTRA_DIST = \
	$(convert_DATA) \
	$(desktopfiles_in_files) \
	$(gschema_in_files) \
	$(xml_in_files)
