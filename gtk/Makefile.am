pcfiles = gtk-clutter-@CLUTTER_MAJORMINOR@.pc
%-@CLUTTER_MAJORMINOR@.pc: %.pc
	cp $< $@

pkgconfig_DATA = $(pcfiles)
pkgconfigdir   = $(libdir)/pkgconfig

EXTRA_DIST = gtk-clutter.pc.in

CLEANFILES = $(pcfiles)

INCLUDES = @GCC_FLAGS@ @CLUTTER_CFLAGS@ $(GST_CFLAGS) $(GTK_CFLAGS) -I$(top_srcdir)

lib_LTLIBRARIES = libgtkclutter-@CLUTTER_MAJORMINOR@.la

libgtkclutter_@CLUTTER_MAJORMINOR@_la_SOURCES = gtk-clutter.c
libgtkclutter_@CLUTTER_MAJORMINOR@_la_LIBADD = \
                          @CLUTTER_LIBS@ $(GST_LIBS) $(GTK_LIBS)
libgtkclutter_@CLUTTER_MAJORMINOR@_la_LDFLAGS = @CLUTTER_LT_LDFLAGS@

gtkclutterheadersdir = \
         $(includedir)/gtk-clutter-@CLUTTER_MAJORMINOR@/gtk-clutter
gtkclutterheaders_HEADERS = gtk-clutter.h

noinst_PROGRAMS = gtk-clutter-test

gtk_clutter_test_SOURCES = gtk-clutter-test.c
gtk_clutter_test_LDADD = libgtkclutter-@CLUTTER_MAJORMINOR@.la	\
	$(CLUTTER_LIBS)						\
	$(GST_LIBS)						\
	$(GTK_LIBS)						\
	$(top_builddir)/clutter/libclutter-@CLUTTER_MAJORMINOR@.la

gtk_clutter_test_CFLAGS = $(CLUTTER_CFLAGS) $(GST_CFLAGS) $(GTK_CFLAGS)
