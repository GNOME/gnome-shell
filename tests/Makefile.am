noinst_SCRIPTS = run-test.sh
EXTRA_DIST = run-test.sh.in

TEST_JS =					\
	testcommon/ui.js			\
	interactive/box-layout.js		\
	interactive/calendar.js
EXTRA_DIST += $(TEST_JS)

TEST_MISC =					\
	testcommon/test.css
EXTRA_DIST += $(TEST_MISC)

run-test.sh: run-test.sh.in
	$(AM_V_GEN) sed \
	    -e "s|@GJS_JS_DIR[@]|$(GJS_JS_DIR)|" \
	    -e "s|@GJS_JS_NATIVE_DIR[@]|$(GJS_JS_NATIVE_DIR)|" \
	    -e "s|@MUTTER_LIB_DIR[@]|$(MUTTER_LIB_DIR)|" \
	    -e "s|@srcdir[@]|$(srcdir)|" \
	    $< > $@ && chmod a+x $@
