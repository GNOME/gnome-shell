
INCLUDES=@METACITY_CFLAGS@

wm_tester_SOURCES= 				\
	main.c

test_gravity_SOURCES=				\
	test-gravity.c

focus_window_SOURCES=				\
	focus-window.c

test_resizing_SOURCES=				\
	test-resizing.c

noinst_PROGRAMS=wm-tester test-gravity test-resizing focus-window

wm_tester_LDADD= @METACITY_LIBS@
test_gravity_LDADD= @METACITY_LIBS@
test_resizing_LDADD= @METACITY_LIBS@
focus_window_LDADD= @METACITY_LIBS@