noinst_PROGRAMS = test-textures test-events test-offscreen test-scale \
                  test-actors test-behave test-text test-entry test-project \
		  test-perspective test-rotate test-depth \
		  test-threads test-timeline test-timeline-dup-frames \
                  test-timeline-interpolate test-timeline-rewind \
                  test-timeline-smoothness \
		  test-threads test-timeline test-score test-script \
		  test-model test-grab test-effects test-fullscreen \
		  test-shader test-unproject test-viewport test-fbo \
		  test-opacity test-multistage

INCLUDES = -I$(top_srcdir)/
LDADD = $(top_builddir)/clutter/libclutter-@CLUTTER_FLAVOUR@-@CLUTTER_MAJORMINOR@.la
AM_CFLAGS = $(CLUTTER_CFLAGS)
AM_LDFLAGS = $(CLUTTER_LIBS)

test_textures_SOURCES    = test-textures.c
test_events_SOURCES      = test-events.c
test_offscreen_SOURCES   = test-offscreen.c
test_scale_SOURCES       = test-scale.c
test_actors_SOURCES      = test-actors.c
test_grab_SOURCES        = test-grab.c
test_behave_SOURCES      = test-behave.c
test_text_SOURCES        = test-text.c
test_entry_SOURCES       = test-entry.c
test_project_SOURCES     = test-project.c
test_unproject_SOURCES   = test-unproject.c
test_perspective_SOURCES = test-perspective.c
test_rotate_SOURCES      = test-rotate.c
test_depth_SOURCES       = test-depth.c
test_threads_SOURCES     = test-threads.c
test_timeline_SOURCES    = test-timeline.c
test_timeline_dup_frames_SOURCES  = test-timeline-dup-frames.c
test_timeline_interpolate_SOURCES = test-timeline-interpolate.c
test_timeline_rewind_SOURCES      = test-timeline-rewind.c
test_timeline_smoothness_SOURCES  = test-timeline-smoothness.c
test_shader_SOURCES      = test-shader.c
test_score_SOURCES       = test-score.c
test_script_SOURCES      = test-script.c
test_model_SOURCES       = test-model.c
test_effects_SOURCES     = test-effects.c
test_fullscreen_SOURCES  = test-fullscreen.c
test_viewport_SOURCES    = test-viewport.c
test_fbo_SOURCES         = test-fbo.c
test_opacity_SOURCES     = test-opacity.c
test_multistage_SOURCES     = test-multistage.c

EXTRA_DIST = redhand.png test-script.json
