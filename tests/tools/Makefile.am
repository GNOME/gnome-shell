lib_LTLIBRARIES =

if HAVE_LIBDL

lib_LTLIBRARIES += libdisable-npots.la

endif

libdisable_npots_la_SOURCES = disable-npots.c

libdisable_npots_la_LIBADD = -ldl

all-local : disable-npots.sh

clean-local :
	rm -f disable-npots.sh

disable-npots.sh : $(top_builddir)/tests/tools/disable-npots.sh.in
	sed 's|--builddir--|'`cd '$(top_builddir)' && pwd`'|' < $< > $@ && \
	chmod 755 disable-npots.sh

EXTRA_DIST = README disable-npots.sh.in
