@INTLTOOL_DESKTOP_RULE@

icondir=$(pkgdatadir)/icons
icon_DATA=metacity-window-demo.png

INCLUDES=@METACITY_WINDOW_DEMO_CFLAGS@ @METACITY_MESSAGE_CFLAGS@ \
	@METACITY_PROPS_CFLAGS@  -DMETACITY_ICON_DIR=\"$(pkgdatadir)/icons\" \
	-DMETACITY_PROPS_GLADEDIR=\"$(pkgdatadir)/glade\" \
	-DMETACITY_PROPS_ICON_DIR=\"$(datadir)/pixmaps\" \
	-DMETACITY_LOCALEDIR=\"$(datadir)/locale\"

metacity_message_SOURCES= 				\
	metacity-message.c

metacity_window_demo_SOURCES=				\
	metacity-window-demo.c

metacity_mag_SOURCES=					\
	metacity-mag.c

metacity_grayscale_SOURCES=				\
	metacity-grayscale.c

metacity_properties_SOURCES=				\
	metacity-properties.c

metacity_properties_LDFLAGS = -export-dynamic

uidir=$(pkgdatadir)/glade
ui_DATA=metacity-properties.glade

propicondir=$(datadir)/pixmaps
propicon_DATA=metacity-properties.png

desktopdir=$(datadir)/control-center-2.0/capplets
Desktop_in_files=metacity-properties.desktop.in
desktop_DATA=$(Desktop_in_files:.desktop.in=.desktop)

bin_PROGRAMS=metacity-message metacity-window-demo metacity-properties

## cheesy hacks I use, don't really have any business existing. ;-)
noinst_PROGRAMS=metacity-mag metacity-grayscale

metacity_message_LDADD= @METACITY_MESSAGE_LIBS@
metacity_window_demo_LDADD= @METACITY_WINDOW_DEMO_LIBS@
metacity_properties_LDADD= @METACITY_PROPS_LIBS@
metacity_mag_LDADD= @METACITY_WINDOW_DEMO_LIBS@
metacity_grayscale_LDADD = @METACITY_WINDOW_DEMO_LIBS@

EXTRA_DIST=$(icon_DATA) $(ui_DATA) $(propicon_DATA) $(Desktop_in_files)

