
pkglibdir=$(libdir)/@PACKAGE@/plugins/clutter

if WITH_CLUTTER

INCLUDES=@METACITY_CFLAGS@ -I $(top_srcdir)/src/include -DMETACITY_LIBEXECDIR=\"$(libexecdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" -DMETACITY_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\" -DMETACITY_PKGDATADIR=\"$(pkgdatadir)\" -DMETACITY_DATADIR=\"$(datadir)\" -DG_LOG_DOMAIN=\"metacity\" -DSN_API_NOT_YET_FROZEN=1 -DMETACITY_MAJOR_VERSION=$(METACITY_MAJOR_VERSION) -DMETACITY_MINOR_VERSION=$(METACITY_MINOR_VERSION) -DMETACITY_MICRO_VERSION=$(METACITY_MICRO_VERSION) -DMETACITY_CLUTTER_PLUGIN_API_VERSION=$(METACITY_CLUTTER_PLUGIN_API_VERSION) -DMETACITY_PKGLIBDIR=\"$(pkglibdir)\"

simple_la_CFLAGS   = -fPIC
simple_la_SOURCES  = simple.c
simple_la_LDFLAGS  = -module -avoid-version
simple_la_LIBADD   = @CLUTTER_LIBS@
pkglib_LTLIBRARIES = simple.la

# post-install hook to remove the .la and .a files we are not interested in
# (There is no way to stop libtool generating static libs locally, and we
# cannot do this globally because of libmetacity-private.so).
install-exec-hook:
	rm $(pkglibdir)/*.a
	rm $(pkglibdir)/*.la

endif
