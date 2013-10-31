# Point to our macro directory and pick up user flags from the environment
ACLOCAL_AMFLAGS  = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = data js src  tests po docs

if BUILD_BROWSER_PLUGIN
SUBDIRS += browser-plugin
endif

if ENABLE_MAN
SUBDIRS += man
endif

EXTRA_DIST =		\
	.project	\
	.settings	\
	autogen.sh	\
	tools/check-for-missing.py

# These are files checked into Git that we don't want to distribute
DIST_EXCLUDE =					\
	.gitignore				\
	.gitmodules				\
	gnome-shell.doap			\
	HACKING					\
	MAINTAINERS				\
	tools/build/*

distcheck-hook:
	@echo "Checking disted files against files in git"
	@$(srcdir)/tools/check-for-missing.py $(srcdir) $(distdir) $(DIST_EXCLUDE)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-man
