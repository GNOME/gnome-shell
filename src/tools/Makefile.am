
INCLUDES=@METACITY_RESTART_CFLAGS@ @METACITY_RELOAD_THEME_CFLAGS@ @METACITY_WINDOW_DEMO_CFLAGS@

metacity_restart_SOURCES= 				\
	metacity-restart.c

metacity_reload_theme_SOURCES= 				\
	metacity-reload-theme.c

metacity_window_demo_SOURCES=				\
	metacity-window-demo.c

bin_PROGRAMS=metacity-restart metacity-reload-theme metacity-window-demo

metacity_restart_LDADD= @METACITY_RESTART_LIBS@
metacity_reload_theme_LDADD= @METACITY_RELOAD_THEME_LIBS@
metacity_window_demo_LDADD= @METACITY_WINDOW_DEMO_LIBS@
