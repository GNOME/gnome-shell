NULL =

json_files = \
	test-script-animation.json		\
	test-script-child.json			\
	test-script-layout-property.json	\
	test-script-implicit-alpha.json		\
	test-script.json			\
	test-script-named-object.json		\
	test-script-object-property.json	\
	test-script-signals.json		\
	test-script-single.json			\
	test-script-model.json			\
	test-animator-1.json			\
	test-animator-2.json			\
	test-animator-3.json			\
	test-state-1.json			\
	test-script-timeline-markers.json	\
	test-script-margin.json 		\
	$(NULL)

png_files = \
	redhand.png 		\
	redhand_alpha.png	\
	light0.png 		\
	$(NULL)

EXTRA_DIST = $(json_files) $(png_files) clutter-2.0.suppressions

if ENABLE_INSTALLED_TESTS
insttestdir = $(libexecdir)/installed-tests/$(PACKAGE)/data
insttest_DATA = $(json_files) $(png_files)
endif
