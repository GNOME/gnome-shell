include $(top_srcdir)/build/autotools/Makefile.am.silent

EXAMPLES				= 	cally-atkcomponent-example 	\
						cally-atkeditabletext-example   \
						cally-atkevents-example		\
						cally-atktext-example		\
						cally-clone-example

common_ldadd			= $(top_builddir)/clutter/libclutter-@CLUTTER_SONAME_INFIX@-@CLUTTER_API_VERSION@.la

common_sources			= cally-examples-util.c cally-examples-util.h

INCLUDES 				= \
					-I$(top_srcdir)			\
					-I$(top_srcdir)/clutter 	\
					-I$(top_srcdir)/clutter/cogl 	\
					-I$(top_builddir)/clutter 	\
					-I$(top_builddir)/clutter/cogl	\
					-I$(top_srcdir)/tests/accessibility

noinst_PROGRAMS				= 	$(EXAMPLES)

EXTRA_DIST				=	$(common_sources)

# atk component example
cally_atkcomponent_example_LDADD	= 	$(CLUTTER_LIBS) $(common_ldadd)
cally_atkcomponent_example_CFLAGS	= 	$(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)	\
						-DPREFIXDIR=\"$(libdir)\"	\
						$(INCLUDES)
cally_atkcomponent_example_SOURCES	= 	cally-atkcomponent-example.c	\
						$(common_sources)

# atk text example
cally_atktext_example_LDADD		= 	$(CLUTTER_LIBS) $(common_ldadd)
cally_atktext_example_CFLAGS		= 	$(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)\
						-DPREFIXDIR=\"$(libdir)\"	\
						$(INCLUDES)
cally_atktext_example_SOURCES		= 	cally-atktext-example.c	\
						$(common_sources)

# atk text example2
cally_atkevents_example_LDADD		= 	$(CLUTTER_LIBS) $(common_ldadd)
cally_atkevents_example_CFLAGS		= 	$(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)\
						-DPREFIXDIR=\"$(libdir)\"	\
						$(INCLUDES)
cally_atkevents_example_SOURCES		= 	cally-atkevents-example.c	\
						$(common_sources)

# atk editable text example
cally_atkeditabletext_example_LDADD	= 	$(CLUTTER_LIBS) $(common_ldadd)
cally_atkeditabletext_example_CFLAGS	= 	$(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS) \
						-DPREFIXDIR=\"$(libdir)\"	\
						$(INCLUDES)
cally_atkeditabletext_example_SOURCES	= 	cally-atkeditabletext-example.c	\
						$(common_sources)

# cally clone example
cally_clone_example_LDADD		= 	$(CLUTTER_LIBS) $(common_ldadd)
cally_clone_example_CFLAGS		= 	$(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS) \
						-DPREFIXDIR=\"$(libdir)\"	\
						$(INCLUDES)
cally_clone_example_SOURCES		= 	cally-clone-example.c		\
						$(common_sources)
