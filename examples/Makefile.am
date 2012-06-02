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
	layout-manager \
	rounded-rectangle \
	scroll-actor \
	threads

if PIXBUF_TESTS
all_examples += \
	bin-layout \
	image-content
endif

INCLUDES = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/clutter 	\
	-I$(top_builddir)/clutter

LDADD = \
	$(top_builddir)/clutter/libclutter-@CLUTTER_API_VERSION@.la \
	$(CLUTTER_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	-lm

AM_CFLAGS = $(CLUTTER_CFLAGS) $(GDK_PIXBUF_CFLAGS)

AM_CPPFLAGS = \
	-DTESTS_DATADIR=\""$(abs_top_srcdir)/tests/data"\" \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-DCLUTTER_DISABLE_DEPRECATION_WARNINGS

noinst_PROGRAMS = $(all_examples)

-include $(top_srcdir)/build/autotools/Makefile.am.gitignore
