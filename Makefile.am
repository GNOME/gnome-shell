SUBDIRS = data js src po

EXTRA_DIST =		\
	.project	\
	.settings

distcheck-hook:
	@echo "Checking disted javascript against files in git"
	@failed=false; \
	for f in `cd $(srcdir) && git ls-files js` ; do \
		if ! test -e $(distdir)/$$f ; then \
			echo File missing from distribution: $$f ; \
			failed=true ; \
		fi \
	done ; \
	if $$failed ; then \
		exit 1 ; \
	fi
