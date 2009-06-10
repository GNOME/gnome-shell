
pkglibdir=@MUTTER_PLUGIN_DIR@

if WITH_CLUTTER

INCLUDES=@MUTTER_CFLAGS@ -I $(top_srcdir)/src/include -DMUTTER_LIBEXECDIR=\"$(libexecdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" -DMUTTER_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\" -DMUTTER_PKGDATADIR=\"$(pkgdatadir)\" -DMUTTER_DATADIR=\"$(datadir)\" -DG_LOG_DOMAIN=\"mutter\" -DSN_API_NOT_YET_FROZEN=1 -DMUTTER_MAJOR_VERSION=$(MUTTER_MAJOR_VERSION) -DMUTTER_MINOR_VERSION=$(MUTTER_MINOR_VERSION) -DMUTTER_MICRO_VERSION=$(MUTTER_MICRO_VERSION) -DMUTTER_PLUGIN_API_VERSION=$(MUTTER_PLUGIN_API_VERSION) -DMUTTER_PLUGIN_DIR=\"@MUTTER_PLUGIN_DIR@\"

default_la_CFLAGS   = -fPIC
default_la_SOURCES  = default.c
default_la_LDFLAGS  = -module -avoid-version -no-undefined
default_la_LIBADD   = @CLUTTER_LIBS@

pkglib_LTLIBRARIES = default.la

# post-install hook to remove the .la and .a files we are not interested in
# (There is no way to stop libtool generating static libs locally, and we
# cannot do this globally because of libmutter-private.so).
install-exec-hook:
	-rm $(DESTDIR)$(pkglibdir)/*.a
	-rm $(DESTDIR)$(pkglibdir)/*.la

endif
