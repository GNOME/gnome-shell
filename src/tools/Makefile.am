@INTLTOOL_DESKTOP_RULE@

icondir=$(pkgdatadir)/icons
icon_DATA=metacity-window-demo.png

INCLUDES=@METACITY_WINDOW_DEMO_CFLAGS@ @METACITY_MESSAGE_CFLAGS@ \
	-DMETACITY_ICON_DIR=\"$(pkgdatadir)/icons\" \
	-DMETACITY_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\"

metacity_message_SOURCES= 				\
	metacity-message.c

metacity_window_demo_SOURCES=				\
	metacity-window-demo.c

metacity_mag_SOURCES=					\
	metacity-mag.c

metacity_grayscale_SOURCES=				\
	metacity-grayscale.c

bin_PROGRAMS=metacity-message metacity-window-demo

## cheesy hacks I use, don't really have any business existing. ;-)
noinst_PROGRAMS=metacity-mag metacity-grayscale

metacity_message_LDADD= @METACITY_MESSAGE_LIBS@
metacity_window_demo_LDADD= @METACITY_WINDOW_DEMO_LIBS@
metacity_mag_LDADD= @METACITY_WINDOW_DEMO_LIBS@
metacity_grayscale_LDADD = @METACITY_WINDOW_DEMO_LIBS@

EXTRA_DIST=$(icon_DATA)

