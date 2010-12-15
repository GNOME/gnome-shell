noinst_SCRIPTS = run-test.sh
EXTRA_DIST = run-test.sh.in

TEST_JS =					\
	interactive/borders.js			\
	interactive/border-radius.js		\
	interactive/border-width.js		\
	interactive/box-layout.js		\
	interactive/calendar.js			\
	interactive/css-fonts.js		\
	interactive/entry.js			\
	interactive/icons.js			\
	interactive/inline-style.js		\
	interactive/scrolling.js		\
	interactive/scroll-view-sizing.js	\
	interactive/table.js			\
	testcommon/border-image.png		\
	testcommon/ui.js                        \
	unit/format.js
EXTRA_DIST += $(TEST_JS)

TEST_MISC =					\
	testcommon/test.css
EXTRA_DIST += $(TEST_MISC)

run-test.sh: run-test.sh.in
	$(AM_V_GEN) sed \
	    -e "s|@MUTTER_LIB_DIR[@]|$(MUTTER_LIB_DIR)|" \
	    -e "s|@srcdir[@]|$(srcdir)|" \
	    $< > $@ && chmod a+x $@

CLEANFILES = run-test.sh
