HTML_DIR = $(datadir)/gtk-doc/html
TARGET_DIR = $(HTML_DIR)/clutter-cookbook

HTML_FILES = $(srcdir)/html/*.html
IMAGE_FILES = $(srcdir)/images/*.png

EXTRA_DIST = $(IMAGE_FILES) clutter-cookbook.xml.in

CLEANFILES = \
	pdf-build.stamp \
	html-build.stamp

pdf-build.stamp: clutter-cookbook.xml
	SP_ENCODING=XML SP_CHARSET_FIXED=YES \
	$(JW) -b pdf $(srcdir)/clutter-cookbook.xml \
	&& mv -f $(srcdir)/clutter-cookbook.pdf clutter-cookbook-@CLUTTER_API_VERSION@.pdf \
	&& echo timestamp > $(@F)

html-build.stamp: clutter-cookbook.xml
	$(mkinstalldirs) html \
	&& $(XSLTPROC) --nonet --xinclude -o $(srcdir)/html/ \
		--path $(top_srcdir)/doc/common \
		--path $(srcdir) \
		--stringparam gtkdoc.bookname "clutter-cookbook" \
		--stringparam gtkdoc.version @CLUTTER_API_VERSION@ \
		$(srcdir)/ref-html-style.xsl \
	$(srcdir)/clutter-cookbook.xml \
	&& echo timestamp > $(@F)

if ENABLE_PDFS
pdf_target = pdf-build.stamp
else
pdf_target =
endif

if ENABLE_DOCS
all-local: html-build.stamp $(pdf_target)
else
all-local:
endif

clean-local:
	@rm -rf html ; \
	rm -f *.pdf ; \
	rm -f *.stamp

uninstall-local:

install-data-local:
	installfiles=`echo $(srcdir)/html/*`; \
	if test "$$installfiles" = '$(srcdir)/html/*'; \
	then echo '-- Nothing to install' ; \
	else \
	  $(mkinstalldirs) $(DESTDIR)$(TARGET_DIR) ; \
	  for file in `ls $(srcdir)/$(HTML_FILES)`; do \
	    if [ -f $$file ]; then \
	      basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	      $(INSTALL_DATA) $$file $(DESTDIR)$(TARGET_DIR)/$$basefile; \
	    fi \
	  done; \
	fi; \
	if [ -d $(srcdir)/images ]; \
	then \
	  $(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)/images ; \
	  for file in `ls $(srcdir)/$(IMAGE_FILES)`; do \
	    if [ -f $$file ]; \
	    then \
	      basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	      $(INSTALL_DATA) $$file $(DESTDIR)$(TARGET_DIR)/images/$$basefile; \
	    fi \
	  done; \
	fi; \
	$(INSTALL_DATA) $(top_srcdir)/doc/common/style.css $(DESTDIR)$(TARGET_DIR)/style.css; \
	$(INSTALL_DATA) $(srcdir)/html/clutter-cookbook.devhelp	$(DESTDIR)$(TARGET_DIR)/clutter-cookbook.devhelp

.PHONY : doc
