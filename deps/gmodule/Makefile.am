
noinst_LTLIBRARIES = libgmodule.la

libgmodule_la_CPPFLAGS =		\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/deps		\
	-I$(top_srcdir)/deps/glib	\
	-I$(top_builddir)/deps/glib	\
	-Wall				\
	$(NULL)

EXTRA_DIST =			\
	gmoduleconf.h.in 	\
	gmodule-dl.c		\
	gmodule-dld.c		\
	gmodule-dyld.c		\
	gmodule-win32.c		\
	gmoduleconf.h.win32 	\
	$(NULL)

BUILT_SOURCES = gmoduleconf.h
gmoduleconf.h:	gmoduleconf.h.in

libgmodule_la_SOURCES = gmodule.c
libgmodule_la_LIBADD =				\
	$(G_MODULE_LIBS_EXTRA)			\
	$(G_MODULE_LIBS)			\
	$(top_builddir)/deps/glib/$(libglib)	\
	$(NULL)
libgmodule_la_LDFLAGS =		\
	$(G_MODULE_LDFLAGS)	\
	$(NULL)
