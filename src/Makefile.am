lib_LTLIBRARIES = libmetacity-private.la

SUBDIRS=wm-tester tools themes

INCLUDES=@METACITY_CFLAGS@ -DMETACITY_LIBEXECDIR=\"$(libexecdir)\" -DMETACITY_LIBDIR=\"$(libdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" -DMETACITY_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\" -DMETACITY_PKGDATADIR=\"$(pkgdatadir)\" -DMETACITY_DATADIR=\"$(datadir)\" -DG_LOG_DOMAIN=\"metacity\" -DSN_API_NOT_YET_FROZEN=1

EGGFILES=			\
	eggaccelerators.c	\
	eggaccelerators.h

metacity_SOURCES= 				\
	async-getprop.c				\
	async-getprop.h				\
	bell.h					\
	bell.c					\
	boxes.h					\
	boxes.c					\
	common.h				\
	c-screen.c				\
	c-screen.h				\
	compositor.c				\
	compositor.h				\
	constraints.c				\
	constraints.h				\
	core.c					\
	core.h					\
	delete.c				\
	display.c				\
	display.h				\
	draw-workspace.c			\
	draw-workspace.h			\
	edge-resistance.c			\
	edge-resistance.h			\
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
	group.c					\
	group.h					\
	group-private.h				\
	group-props.c				\
	group-props.h				\
	iconcache.c				\
	iconcache.h				\
	inlinepixbufs.h				\
	keybindings.c				\
	keybindings.h				\
	main.c					\
	main.h					\
	menu.c					\
	menu.h					\
	metaaccellabel.c			\
	metaaccellabel.h			\
	metacity-Xatomtype.h			\
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
	spring-model.c				\
	spring-model.h				\
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
	window-props.c				\
	window-props.h				\
	workspace.c				\
	workspace.h				\
	xprops.c				\
	xprops.h				\
	$(EGGFILES)

# by setting libmetacity_private_la_CFLAGS, the files shared with
# metacity proper will be compiled with different names.
libmetacity_private_la_CFLAGS =
libmetacity_private_la_SOURCES=	\
	gradient.c		\
	gradient.h		\
	preview-widget.c	\
	preview-widget.h	\
	theme.c			\
	theme.h			\
	theme-parser.c		\
	theme-parser.h		\
	util.c			\
	util.h			\
	common.h

libmetacity_private_la_LDFLAGS = -no-undefined
libmetacity_private_la_LIBADD  = @METACITY_LIBS@

libmetacityincludedir = $(includedir)/metacity-1/metacity-private

libmetacityinclude_HEADERS =	\
	common.h		\
	gradient.h		\
	preview-widget.h	\
	theme.h			\
	theme-parser.h		\
	util.h

metacity_theme_viewer_SOURCES=  \
	theme-viewer.c

metacity_dialog_SOURCES= 	\
	metacity-dialog.c

bin_PROGRAMS=metacity metacity-theme-viewer
libexec_PROGRAMS=metacity-dialog

EFENCE=
metacity_LDADD=@METACITY_LIBS@ $(EFENCE)
metacity_theme_viewer_LDADD= @METACITY_LIBS@ libmetacity-private.la
metacity_dialog_LDADD=@METACITY_LIBS@

testboxes_SOURCES=util.h util.c boxes.h boxes.c testboxes.c
testgradient_SOURCES=gradient.h gradient.c testgradient.c
testasyncgetprop_SOURCES=async-getprop.h async-getprop.c testasyncgetprop.c

noinst_PROGRAMS=testboxes testgradient testasyncgetprop

testboxes_LDADD= @METACITY_LIBS@
testgradient_LDADD= @METACITY_LIBS@
testasyncgetprop_LDADD= @METACITY_LIBS@

desktopfilesdir=$(datadir)/gnome/wm-properties
desktopfiles_in_files=metacity.desktop.in
desktopfiles_files=$(desktopfiles_in_files:.desktop.in=.desktop)
desktopfiles_DATA = $(desktopfiles_files)
@INTLTOOL_DESKTOP_RULE@

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = metacity.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA)
else
install-data-local:
endif

IMAGES=default_icon.png stock_maximize.png stock_minimize.png stock_delete.png
VARIABLES=default_icon_data $(srcdir)/default_icon.png \
          stock_maximize_data $(srcdir)/stock_maximize.png \
          stock_minimize_data $(srcdir)/stock_minimize.png \
          stock_delete_data $(srcdir)/stock_delete.png

BUILT_SOURCES = inlinepixbufs.h
CLEANFILES = inlinepixbufs.h metacity.desktop metacity.schemas

inlinepixbufs.h: $(IMAGES)
	   $(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) >$(srcdir)/inlinepixbufs.h

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libmetacity-private.pc

EXTRA_DIST=$(desktopfiles_files) 	\
	$(IMAGES) $(schema_DATA) 	\
	update-from-egg.sh		\
	$(desktopfiles_in_files)	\
	$(schema_in_files)		\
	libmetacity-private.pc.in


EGGDIR=$(srcdir)/../../libegg/libegg

regenerate-built-sources:
	EGGFILES="$(EGGFILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh
