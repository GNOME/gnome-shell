@INTLTOOL_DESKTOP_RULE@

icondir=$(pkgdatadir)/icons
icon_DATA=mutter-window-demo.png

INCLUDES=@MUTTER_WINDOW_DEMO_CFLAGS@ @MUTTER_MESSAGE_CFLAGS@ \
	-DMUTTER_ICON_DIR=\"$(pkgdatadir)/icons\" \
	-DMUTTER_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\"

mutter_message_SOURCES= 				\
	mutter-message.c

mutter_window_demo_SOURCES=				\
	mutter-window-demo.c

mutter_mag_SOURCES=					\
	mutter-mag.c

mutter_grayscale_SOURCES=				\
	mutter-grayscale.c

bin_PROGRAMS=mutter-message mutter-window-demo

## cheesy hacks I use, don't really have any business existing. ;-)
noinst_PROGRAMS=mutter-mag mutter-grayscale

mutter_message_LDADD= @MUTTER_MESSAGE_LIBS@
mutter_window_demo_LDADD= @MUTTER_WINDOW_DEMO_LIBS@
mutter_mag_LDADD= @MUTTER_WINDOW_DEMO_LIBS@
mutter_grayscale_LDADD = @MUTTER_WINDOW_DEMO_LIBS@

EXTRA_DIST=$(icon_DATA)

