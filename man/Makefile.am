.xml.1:
	$(AM_V_GEN) $(XSLTPROC) -o $@ --nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

man_MANS = \
	gnome-shell.1

xml_files = $(man_MANS:.1=.xml)

EXTRA_DIST = $(xml_files)

DISTCLEANFILES = $(man_MANS)
