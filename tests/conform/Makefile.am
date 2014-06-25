include $(top_srcdir)/build/autotools/glib-tap.mk

AM_CFLAGS = -g $(CLUTTER_CFLAGS) $(MAINTAINER_CFLAGS)
LDADD = $(top_builddir)/clutter/libclutter-1.0.la $(CLUTTER_LIBS) -lm
AM_LDFLAGS = -export-dynamic
AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Clutter-Conform\"	\
	-I$(top_srcdir) 			\
	-I$(top_builddir)			\
	-DCOGL_DISABLE_DEPRECATION_WARNINGS	\
	$(CLUTTER_DEPRECATED_CFLAGS)		\
	$(CLUTTER_DEBUG_CFLAGS)			\
	$(CLUTTER_PROFILE_CFLAGS)

# Basic actor API
actor_tests = \
	actor-anchors \
	actor-destroy \
	actor-graph \
	actor-invariants \
	actor-iter \
	actor-layout \
	actor-meta \
	actor-offscreen-limit-max-size \
	actor-offscreen-redirect \
	actor-paint-opacity \
	actor-pick \
	actor-shader-effect \
	actor-size \
	$(NULL)

# Actor classes
classes_tests = \
	text \
	$(NULL)

# General API
general_tests = \
	binding-pool \
	color \
	events-touch \
	interval \
	model \
	script-parser \
	units \
	$(NULL)

# Test for deprecated functionality
deprecated_tests = \
	animator \
	behaviours \
	group \
	rectangle \
	texture \
	$(NULL)

test_programs = $(actor_tests) $(general_tests) $(classes_tests) $(deprecated_tests)

dist_test_data = $(script_ui_files)
script_ui_files = $(addprefix scripts/,$(script_tests))
script_tests = \
	test-animator-1.json \
	test-animator-2.json \
	test-animator-3.json \
	test-script-animation.json \
	test-script-child.json \
	test-script-implicit-alpha.json \
	test-script-interval.json \
	test-script-layout-property.json \
	test-script-margin.json \
	test-script-model.json \
	test-script-named-object.json \
	test-script-object-property.json \
	test-script-single.json \
	test-script-timeline-markers.json \
	test-state-1.json

TESTS_ENVIRONMENT += G_ENABLE_DIAGNOSTIC=0 CLUTTER_ENABLE_DIAGNOSTIC=0

# simple rules for generating a Git ignore file for the conformance test suite
$(srcdir)/.gitignore: Makefile
	$(AM_V_GEN)( echo "/*.trs" ; \
		     echo "/*.log" ; \
		     echo "/*.test" ; \
		     echo "/.gitignore" ; \
		     for p in $(test_programs); do \
			echo "/$$p" ; \
		     done ) > $(@F)

gitignore: $(srcdir)/.gitignore

all-am: gitignore

DISTCLEANFILES += .gitignore
