
icondir=$(pkgdatadir)/icons
icon_DATA=metacity-window-demo.png

INCLUDES=@METACITY_WINDOW_DEMO_CFLAGS@ @METACITY_MESSAGE_CFLAGS@ -DMETACITY_ICON_DIR=\"$(pkgdatadir)/icons\"

metacity_message_SOURCES= 				\
	metacity-message.c

metacity_window_demo_SOURCES=				\
	metacity-window-demo.c

bin_PROGRAMS=metacity-message metacity-window-demo

metacity_message_LDADD= @METACITY_MESSAGE_LIBS@
metacity_window_demo_LDADD= @METACITY_WINDOW_DEMO_LIBS@
