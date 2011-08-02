
INCLUDES=@MUTTER_CFLAGS@

wm_tester_SOURCES= 				\
	main.c

test_gravity_SOURCES=				\
	test-gravity.c

focus_window_SOURCES=				\
	focus-window.c

test_resizing_SOURCES=				\
	test-resizing.c

test_size_hints_SOURCES=			\
	test-size-hints.c

test_attached_SOURCES=				\
	test-attached.c

test_focus_SOURCES=				\
	test-focus.c

noinst_PROGRAMS=wm-tester test-gravity test-resizing focus-window test-size-hints test-attached test-focus

wm_tester_LDADD= @MUTTER_LIBS@
test_gravity_LDADD= @MUTTER_LIBS@
test_resizing_LDADD= @MUTTER_LIBS@
test_size_hints_LDADD= @MUTTER_LIBS@
focus_window_LDADD= @MUTTER_LIBS@
test_attached_LDADD= @MUTTER_LIBS@
test_focus_LDADD= @MUTTER_LIBS@
