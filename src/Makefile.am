
SUBDIRS=uislave

INCLUDES=@METACITY_CFLAGS@ -DMETACITY_LIBEXECDIR=\"$(libexecdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" -DMETACITY_COMPILE=1

## convenience libraries lead to annoying slow compiles, plus we want
## to be extra-hacky by using #ifdef METACITY_COMPILE, ergo cheesy
## hack that follows.
copied_sources= \
	messagequeue.h \
	messagequeue.c
copied_sources_deps= \
	$(srcdir)/uislave/messagequeue.h \
	$(srcdir)/uislave/messagequeue.c

BUILT_SOURCES=$(copied_sources)

$(copied_sources): $(copied_sources_deps)
	for I in $(copied_sources); do				\
		rm -f $$I ;					\
		cp $(srcdir)/uislave/$$I . ;			\
	done

metacity_SOURCES= 				\
	api.c					\
	api.h					\
	colors.c				\
	colors.h				\
	display.c				\
	display.h				\
	errors.c				\
	errors.h				\
	eventqueue.c				\
	eventqueue.h				\
	frame.c					\
	frame.h					\
	main.c					\
	main.h					\
	screen.c				\
	screen.h				\
	theme.c					\
	theme.h					\
	uislave.c				\
	uislave.h				\
	util.c					\
	util.h					\
	window.c				\
	window.h				\
	$(copied_sources)

bin_PROGRAMS=metacity

metacity_LDADD= @METACITY_LIBS@
