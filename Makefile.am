SUBDIRS = cogl

if BUILD_COGL_PANGO
SUBDIRS += pango
endif

if COGL_STANDALONE_BUILD
SUBDIRS += po examples
endif

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}
