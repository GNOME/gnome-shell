
INCLUDES=@UISLAVE_CFLAGS@ -DHOST_ALIAS=\"@HOST_ALIAS@\"

metacity_uislave_SOURCES = 	\
	fixedtip.h		\
	fixedtip.c		\
	main.c			\
	menu.c			\
	menu.h			\
	messages.c		\
	messages.h		\
	messagequeue.c		\
	messagequeue.h

libexec_PROGRAMS=metacity-uislave

metacity_uislave_LDADD = @UISLAVE_LIBS@