NULL =
BUILT_SOURCES =

misc/config.js: misc/config.js.in Makefile
	[ -d $(@D) ] || $(mkdir_p) $(@D) ; \
	sed -e "s|[@]PACKAGE_NAME@|$(PACKAGE_NAME)|g" \
	    -e "s|[@]PACKAGE_VERSION@|$(PACKAGE_VERSION)|g" \
	    -e "s|[@]HAVE_BLUETOOTH@|$(HAVE_BLUETOOTH)|g" \
	    -e "s|[@]GETTEXT_PACKAGE@|$(GETTEXT_PACKAGE)|g" \
	    -e "s|[@]datadir@|$(datadir)|g" \
	    -e "s|[@]libexecdir@|$(libexecdir)|g" \
	    -e "s|[@]sysconfdir@|$(sysconfdir)|g" \
               $< > $@

js_resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/js-resources.gresource.xml)
js-resources.h: js-resources.gresource.xml $(js_resource_files) misc/config.js
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name shell_js_resources $<
js-resources.c: js-resources.gresource.xml $(js_resource_files) misc/config.js
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name shell_js_resources $<

js_built_sources = js-resources.c js-resources.h

BUILT_SOURCES += $(js_built_sources)

all-local: $(js_built_sources)

js_resource_dist_files = $(filter-out misc/config.js, $(js_resource_files))

EXTRA_DIST = \
	$(js_resource_dist_files) \
	js-resources.gresource.xml \
	misc/config.js.in \
	$(NULL)
