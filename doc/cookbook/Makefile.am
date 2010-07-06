NULL =

HTML_DIR = $(datadir)/gtk-doc/html
TARGET_DIR = $(HTML_DIR)/clutter-cookbook

XML_FILES = \
	actors.xml		\
	animations.xml		\
	events.xml		\
	introduction.xml	\
	recipe-template.xml	\
	textures.xml		\
	$(NULL)

XSLTOPTS = \
	--stringparam html.stylesheet style.css 		\
	--stringparam  chapter.autolabel 1 			\
	--stringparam  appendix.autolabel 1 			\
	--stringparam  section.autolabel 1 			\
	--stringparam gtkdoc.bookname "clutter-cookbook" 	\
	--stringparam gtkdoc.version @CLUTTER_API_VERSION@ 	\
	--path $(top_srcdir)/doc/common				\
	--xinclude

XSL_BASE_URI  = http://docbook.sourceforge.net/release/xsl/current
XSL_XHTML_URI = $(XSL_BASE_URI)/xhtml/docbook.xsl

HTML_FILES = html/*.html
IMAGE_FILES = images/clutter-logo.png

EXTRA_DIST = clutter-cookbook.xml.in $(IMAGE_FILES) $(XML_FILES)

CLEANFILES = \
	pdf-build.stamp \
	html-build.stamp

pdf-build.stamp: clutter-cookbook.xml
	SP_ENCODING=XML SP_CHARSET_FIXED=YES \
	$(JW) -b pdf $(top_builddir)/doc/cookbook/clutter-cookbook.xml \
	&& mv -f clutter-cookbook.pdf clutter-cookbook-@CLUTTER_API_VERSION@.pdf \
	&& echo timestamp > $(@F)

html-build.stamp: clutter-cookbook.xml
	$(XSLTPROC) $(XSLTOPTS) -o clutter-cookbook.html $(XSL_XHTML_URI) $(top_builddir)/doc/cookbook/clutter-cookbook.xml && \
	$(XSLTPROC) $(XSLTOPTS) -o html/ ref-html-style.xsl $(top_builddir)/doc/cookbook/clutter-cookbook.xml && \
  cp $(top_srcdir)/doc/common/style.css html/ && \
	echo timestamp > $(@F)

if ENABLE_PDFS
#pdf_target = pdf-build.stamp
pdf_target =
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
	installfiles=`echo ./html/*`; \
	if test "$$installfiles" = './html/*'; \
	then echo '-- Nothing to install' ; \
	else \
	  $(mkinstalldirs) $(DESTDIR)$(TARGET_DIR) ; \
	  for file in `ls $(HTML_FILES)`; do \
	    if [ -f $$file ]; then \
	      basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	      $(INSTALL_DATA) $$file $(DESTDIR)$(TARGET_DIR)/$$basefile; \
	    fi \
	  done; \
	fi; \
	if [ -d ./images ]; \
	then \
	  $(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)/images ; \
	  for file in `ls $(IMAGE_FILES)`; do \
	    if [ -f $$file ]; \
	    then \
	      basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	      $(INSTALL_DATA) $$file $(DESTDIR)$(TARGET_DIR)/images/$$basefile; \
	    fi \
	  done; \
	fi; \
	$(INSTALL_DATA) html/clutter-cookbook.devhelp $(DESTDIR)$(TARGET_DIR)/clutter-cookbook.devhelp

.PHONY : doc
