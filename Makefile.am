SUBDIRS = cogl

if BUILD_COGL_PANGO
SUBDIRS += cogl-pango
endif

SUBDIRS += examples doc po

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	README.in \
	config-custom.h
