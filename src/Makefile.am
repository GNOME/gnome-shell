
INCLUDES=@METACITY_CFLAGS@ -DMETACITY_LIBEXECDIR=\"$(libexecdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" 

metacity_SOURCES= 				\
	display.c				\
	display.h				\
	errors.c				\
	errors.h				\
	eventqueue.c				\
	eventqueue.h				\
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
	screen.c				\
	screen.h				\
	session.c				\
	session.h				\
	stack.c					\
	stack.h					\
	util.c					\
	util.h					\
	window.c				\
	window.h				\
	workspace.c				\
	workspace.h

bin_PROGRAMS=metacity

metacity_LDADD= @METACITY_LIBS@
