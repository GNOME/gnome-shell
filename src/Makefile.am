
SUBDIRS=wm-tester tools themes

INCLUDES=@METACITY_CFLAGS@ -DMETACITY_LIBEXECDIR=\"$(libexecdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" -DMETACITY_LOCALEDIR=\"$(datadir)/locale\" -DMETACITY_PKGDATADIR=\"$(pkgdatadir)\"

metacity_SOURCES= 				\
	common.h				\
	core.c					\
	core.h					\
	delete.c				\
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
	gradient.c				\
	gradient.h				\
	iconcache.c				\
	iconcache.h				\
	inlinepixbufs.h				\
	keybindings.c				\
	keybindings.h				\
	main.c					\
	main.h					\
	menu.c					\
	menu.h					\
	place.c					\
	place.h					\
	prefs.c					\
	prefs.h					\
	resizepopup.c				\
	resizepopup.h				\
	screen.c				\
	screen.h				\
	session.c				\
	session.h				\
	stack.c					\
	stack.h					\
	tabpopup.c				\
	tabpopup.h				\
	theme.c					\
	theme.h					\
	theme-parser.c				\
	theme-parser.h				\
	themewidget.c				\
	themewidget.h				\
	ui.c					\
	ui.h					\
	util.c					\
	util.h					\
	window.c				\
	window.h				\
	workspace.c				\
	workspace.h				\
	xprops.c				\
	xprops.h

metacity_theme_viewer_SOURCES=  \
	gradient.c		\
	gradient.h		\
	preview-widget.c	\
	preview-widget.h	\
	theme.c			\
	theme.h			\
	theme-parser.c		\
	theme-parser.h		\
	theme-viewer.c		\
	util.c			\
	util.h	

metacity_dialog_SOURCES= 	\
	metacity-dialog.c

bin_PROGRAMS=metacity metacity-theme-viewer
libexec_PROGRAMS=metacity-dialog

metacity_LDADD= @METACITY_LIBS@
metacity_theme_viewer_LDADD= @METACITY_LIBS@
metacity_dialog_LDADD=@METACITY_LIBS@

testgradient_SOURCES=gradient.h gradient.c testgradient.c

noinst_PROGRAMS=testgradient

testgradient_LDADD= @METACITY_LIBS@

desktopfilesdir=$(datadir)/gnome/wm-properties
desktopfiles_in_files=metacity.desktop.in
desktopfiles_DATA = $(desktopfiles_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = metacity.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

IMAGES=default_icon.png
VARIABLES=default_icon_data $(srcdir)/default_icon.png

BUILT_SOURCES = inlinepixbufs.h
CLEANFILES = inlinepixbufs.h

inlinepixbufs.h: $(IMAGES)
	   $(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) >$(srcdir)/inlinepixbufs.h

EXTRA_DIST=$(desktopfiles_DATA) $(IMAGES) $(schema_DATA)
