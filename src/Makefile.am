
if HAVE_SM
SM_SUBDIRS=msm
else
SM_SUBDIRS=
endif

SUBDIRS=wm-tester $(SM_SUBDIRS)

INCLUDES=@METACITY_CFLAGS@ -DMETACITY_LIBEXECDIR=\"$(libexecdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" 

metacity_SOURCES= 				\
	common.h				\
	core.c					\
	core.h					\
	display.c				\
	display.h				\
	effects.c				\
	effects.h				\
	errors.c				\
	errors.h				\
	eventqueue.c				\
	eventqueue.h				\
	fixedtip.c				\
	fixedtip.h				\
	frame.c					\
	frame.h					\
	frames.c				\
	frames.h				\
	keybindings.c				\
	keybindings.h				\
	main.c					\
	main.h					\
	menu.c					\
	menu.h					\
	place.c					\
	place.h					\
	screen.c				\
	screen.h				\
	session.c				\
	session.h				\
	stack.c					\
	stack.h					\
	tabpopup.c				\
	tabpopup.h				\
	ui.c					\
	ui.h					\
	util.c					\
	util.h					\
	window.c				\
	window.h				\
	workspace.c				\
	workspace.h

bin_PROGRAMS=metacity

metacity_LDADD= @METACITY_LIBS@

desktopfilesdir=$(datadir)/gnome/wm-properties
desktopfiles_DATA=metacity.desktop

EXTRA_DIST=$(desktopfiles_DATA)

