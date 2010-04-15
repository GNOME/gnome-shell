AM_CFLAGS = $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)

noinst_LTLIBRARIES = libtesselator.la

INCLUDES = \
	-I$(top_srcdir)/clutter/cogl             \
	-I$(srcdir)/winsys                       \
	-I$(srcdir)/driver/$(COGL_DRIVER)        \
	-I$(srcdir)/clutter/cogl/cogl/tesselator \
	-I$(top_builddir)/clutter/cogl

libtesselator_la_SOURCES = \
	dict-list.h \
	dict.c \
	dict.h \
	geom.c \
	geom.h \
	gluos.h \
	memalloc.h \
	mesh.c \
	mesh.h \
	normal.c \
	normal.h \
	priorityq-heap.h \
	priorityq-sort.h \
	priorityq.c \
	priorityq.h \
	render.c \
	render.h \
	sweep.c \
	sweep.h \
	tess.c \
	tess.h \
	tesselator.h \
	tessmono.c \
	tessmono.h \
	GL/glu.h

EXTRA_DIST = \
	README \
	priorityq-heap.c
