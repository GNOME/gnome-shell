# completely hacked-up makefile, proceed at your own risk, etc

default:
	@echo "Try 'make tp' or 'make glib'"

tp: tokentest.c
	gcc `pkg-config --cflags --libs glib-2.0 gdk-2.0 atk` -DMUTTER_DATADIR=\"/usr/share/mutter\" -I../.. -I../../src -I../../src/include tokentest.c ../../src/ui/theme.c ../../src/ui/gradient.c -o tp
