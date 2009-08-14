DOCDIR = $(DESTDIR)/$(datadir)/doc/@PACKAGE@-@CLUTTER_API_VERSION@
HTML_FILES =  html/*.html
IMAGE_FILES = images/*.png
EXTRA_DIST =  clutter-manual.xml.in manual.xsl style.css $(srcdir)/$(IMAGE_FILES)
CLEANFILES = pdf-build.stamp txt-build.stamp htmldoc-build.stamp clutter-manual.xml

pdf-build.stamp: clutter-manual.xml
	SP_ENCODING=XML SP_CHARSET_FIXED=YES \
	$(JW) -b pdf $(srcdir)/clutter-manual.xml && \
	mv $(srcdir)/clutter-manual.pdf clutter-manual-@CLUTTER_API_VERSION@.pdf && \
	touch pdf-build.stamp

txt-build.stamp: clutter-manual.xml
	$(XMLTO) txt $(srcdir)/clutter-manual.xml && \
	mv $(srcdir)/clutter-manual.txt clutter-manual-@CLUTTER_API_VERSION@.txt && \
	touch txt-build.stamp

htmldoc-build.stamp: clutter-manual.xml
	$(mkinstalldirs) html && \
	$(XMLTO) -m manual.xsl -o html/ xhtml $(srcdir)/clutter-manual.xml && \
	cp $(srcdir)/style.css html/ && \
	touch htmldoc-build.stamp

doc: txt-build.stamp htmldoc-build.stamp pdf-build.stamp

if ENABLE_MANUAL
all-local: doc
else
all-local:
endif

clean-local:
	rm -rf html/*.html
	rm -f html/style.css
	rm -f clutter-manual-*.txt
	rm -f *.pdf

uninstall-local:
	rm -rf $(DOCDIR);

install-data-local:
	installfiles=`echo $(srcdir)/html/*`; \
	if test "$$installfiles" = '$(srcdir)/html/*'; then \
	echo '-- Nothing to install' ; \
	else \
	  $(mkinstalldirs) $(DOCDIR)/html/images ; \
	  for file in `ls $(srcdir)/$(HTML_FILES)`; do \
	    if [ -f $$file ]; then \
	      basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	      $(INSTALL_DATA) $$file $(DOCDIR)/html/$$basefile; \
	    fi \
	  done; \
	  for file in `ls $(srcdir)/$(IMAGE_FILES)`; do \
	    if [ -f $$file ]; then \
	      basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	      $(INSTALL_DATA) $$file $(DOCDIR)/html/images/$$basefile; \
	    fi \
	  done; \
	  $(INSTALL_DATA) $(srcdir)/style.css $(DOCDIR)/html/style.css; \
	  $(INSTALL_DATA) $(srcdir)/clutter-manual-@CLUTTER_API_VERSION@.txt $(DOCDIR)/clutter-manual-@CLUTTER_API_VERSION@.txt; \
	  $(INSTALL_DATA) $(srcdir)/clutter-manual-@CLUTTER_API_VERSION@.pdf $(DOCDIR)/clutter-manual-@CLUTTER_API_VERSION@.pdf; \
	fi

.PHONY : doc
