desktopfiles_in_files = \
	mutter.desktop.in
desktopfilesdir = $(datadir)/applications
desktopfiles_DATA = $(desktopfiles_in_files:.desktop.in=.desktop)

%.desktop:%.desktop.in
	$(AM_V_GET) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

xmldir = $(GNOME_KEYBINDINGS_KEYSDIR)
xml_DATA = \
        50-mutter-navigation.xml \
        50-mutter-system.xml \
        50-mutter-windows.xml

if HAVE_WAYLAND
xml_DATA += \
        50-mutter-wayland.xml
endif

gschema_in_files = \
	org.gnome.mutter.gschema.xml.in	\
	org.gnome.mutter.wayland.gschema.xml.in
gsettings_SCHEMAS = $(gschema_in_files:.xml.in=.xml)

%.gschema.xml: %.gschema.xml.in Makefile
	$(AM_V_GEN) sed -e 's|@GETTEXT_DOMAIN[@]|$(GETTEXT_PACKAGE)|g' \
	                -e 's|@XWAYLAND_GRAB_DEFAULT_ACCESS_RULES[@]|$(XWAYLAND_GRAB_DEFAULT_ACCESS_RULES)|g' \
	$< > $@ || rm $@

@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = mutter-schemas.convert

CLEANFILES = \
	$(desktopfiles_DATA) \
	$(gsettings_SCHEMAS)

EXTRA_DIST = \
	$(convert_DATA) \
	$(desktopfiles_in_files) \
	$(gschema_in_files) \
	$(xml_DATA)
