include $(top_srcdir)/build/autotools/Makefile.am.silent

noinst_LTLIBRARIES =

build_shared_libs =

if HAVE_LIBDL

noinst_LTLIBRARIES += libdisable-npots-static.la

build_shared_libs += libdisable-npots.la

endif

libdisable_npots_static_la_SOURCES = disable-npots.c

libdisable_npots_static_la_LIBADD = -ldl

INCLUDES = \
	-I$(top_srcdir)/clutter	                        \
	-I$(top_builddir)/clutter                       \
	$(CLUTTER_CFLAGS)                               \
	-D_GNU_SOURCE

all-local : disable-npots.sh $(build_shared_libs)

clean-local :
	$(QUIET_RM)rm -f disable-npots.sh libdisable-npots.la

disable-npots.sh : $(top_builddir)/tests/tools/disable-npots.sh.in
	$(QUIET_GEN) \
	sed 's|--builddir--|'`cd '$(top_builddir)' && pwd`'|' < $< > $@ \
	&& chmod 755 disable-npots.sh

# Also put the static library into a shared library. We need to do
# this without the automake magic because we don't want it to get
# installed
libdisable-npots.la : libdisable-npots-static.la
	$(LINK) -ldl -rpath /nowhere libdisable-npots-static.la

EXTRA_DIST = README disable-npots.sh.in
