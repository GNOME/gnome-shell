include $(top_srcdir)/build/autotools/Makefile.am.silent

all_examples = \
	basic-actor \
	box-layout \
	canvas \
	constraints \
	drag-action \
	drop-action \
	easing-modes \
	flow-layout \
	grid-layout \
	layout-manager \
	pan-action \
	rounded-rectangle \
	scroll-actor \
	threads

if PIXBUF_TESTS
all_examples += \
	bin-layout \
	image-content
endif

LDADD = \
	$(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la \
	$(CLUTTER_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	-lm

AM_CFLAGS = $(CLUTTER_CFLAGS) $(GDK_PIXBUF_CFLAGS) $(MAINTAINER_CFLAGS)

AM_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I$(top_srcdir)	\
	-I$(top_builddir) \
	-I$(top_srcdir)/clutter \
	-I$(top_builddir)/clutter

noinst_PROGRAMS = $(all_examples)

EXTRA_DIST = redhand.png

-include $(top_srcdir)/build/autotools/Makefile.am.gitignore
