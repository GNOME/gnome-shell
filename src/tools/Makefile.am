@INTLTOOL_DESKTOP_RULE@

icondir=$(pkgdatadir)/icons
icon_DATA=metacity-window-demo.png

INCLUDES=@METACITY_WINDOW_DEMO_CFLAGS@ @METACITY_MESSAGE_CFLAGS@ \
	@METACITY_PROPS_CFLAGS@  -DMETACITY_ICON_DIR=\"$(pkgdatadir)/icons\" \
	-DMETACITY_PROPS_GLADEDIR=\"$(pkgdatadir)/glade\" \
	-DMETACITY_PROPS_ICON_DIR=\"$(datadir)/pixmaps\" \
	-DMETACITY_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\"

metacity_message_SOURCES= 				\
	metacity-message.c

metacity_window_demo_SOURCES=				\
	metacity-window-demo.c

metacity_mag_SOURCES=					\
	metacity-mag.c

metacity_grayscale_SOURCES=				\
	metacity-grayscale.c

if BUILD_CONFIG_DIALOG
metacity_properties_SOURCES=				\
	metacity-properties.c

metacity_properties_LDFLAGS = -export-dynamic
endif

uidir=$(pkgdatadir)/glade
ui_DATA=metacity-properties.glade

propicondir=$(datadir)/pixmaps
propicon_DATA=metacity-properties.png

METACITY_PROPERTIES_DESKTOP_IN=metacity-properties.desktop.in
if BUILD_CONFIG_DIALOG
METACITY_PROPERTIES_DESKTOP=metacity-properties.desktop
else
METACITY_PROPERTIES_DESKTOP=
endif

desktopdir=$(datadir)/control-center-2.0/capplets
desktop_DATA=$(METACITY_PROPERTIES_DESKTOP)

if BUILD_CONFIG_DIALOG
CONFIG_DIALOG=metacity-properties
else
CONFIG_DIALOG=
endif

bin_PROGRAMS=metacity-message metacity-window-demo $(CONFIG_DIALOG)

## cheesy hacks I use, don't really have any business existing. ;-)
noinst_PROGRAMS=metacity-mag metacity-grayscale

metacity_message_LDADD= @METACITY_MESSAGE_LIBS@
metacity_window_demo_LDADD= @METACITY_WINDOW_DEMO_LIBS@
metacity_mag_LDADD= @METACITY_WINDOW_DEMO_LIBS@
metacity_grayscale_LDADD = @METACITY_WINDOW_DEMO_LIBS@
if BUILD_CONFIG_DIALOG
metacity_properties_LDADD= @METACITY_PROPS_LIBS@
endif

EXTRA_DIST=$(icon_DATA) $(ui_DATA) $(propicon_DATA) $(METACITY_PROPERTIES_DESKTOP_IN)

