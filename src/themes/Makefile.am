THEMES=			\
	Atlanta		\
	Bright		\
	Crux		\
	Esco		\
	AgingGorilla	\
	Metabox		\
	Simple

THEME_DIR=$(datadir)/themes
THEME_SUBDIR=metacity-1

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(THEME_DIR);					     \
	for THEME in $(THEMES); do							     \
		echo '-- Installing theme '$$THEME;					     \
		$(mkinstalldirs) $(DESTDIR)$(THEME_DIR)/$$THEME;			     \
		$(mkinstalldirs) $(DESTDIR)$(THEME_DIR)/$$THEME/$(THEME_SUBDIR);	     \
		(installfiles=`find $(srcdir)/$$THEME -name "*.png" -o -name "*.xml"`;	     \
		 for i in $$installfiles; do						     \
			echo '-- Installing '$$i ;					     \
		    	$(INSTALL_DATA) $$i $(DESTDIR)$(THEME_DIR)/$$THEME/$(THEME_SUBDIR) ; \
	  	 done)									     \
	done

dist-hook:
	mkdir $(distdir)/themes;							\
	for THEME in $(THEMES); do							\
		echo '-- Disting theme '$$THEME;					\
		mkdir $(distdir)/$$THEME;						\
		(installfiles=`find $(srcdir)/$$THEME -name "*.png" -o -name "*.xml"`;	\
		 for i in $$installfiles; do						\
			echo '-- Disting '$$i ;						\
		    	cp $$i $(distdir)/$$THEME;					\
	  	 done)									\
	done
