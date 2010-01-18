#include "config.h"

#include <clutter/clutter.h>

#include <glib.h>
#include <locale.h>
#include <stdlib.h>

#include "test-conform-common.h"

static void
test_conform_skip_test (TestConformSimpleFixture *fixture,
                        gconstpointer             data)
{
  /* void */
}

static TestConformSharedState *shared_state = NULL;

/* This is a bit of sugar for adding new conformance tests:
 *
 * - It adds an extern function definition just to save maintaining a header
 *   that lists test entry points.
 * - It sets up callbacks for a fixture, which lets us share initialization
 *   *code* between tests. (see test-conform-common.c)
 * - It passes in a shared *data* pointer that is initialised once in main(),
 *   that gets passed to the fixture setup and test functions. (See the
 *   definition in test-conform-common.h)
 */
#define TEST_CONFORM_SIMPLE(NAMESPACE, FUNC)            G_STMT_START {  \
  extern void FUNC (TestConformSimpleFixture *fixture, gconstpointer data); \
  g_test_add ("/conform" NAMESPACE "/" #FUNC, \
	      TestConformSimpleFixture, \
	      shared_state, /* data argument for test */ \
	      test_conform_simple_fixture_setup, \
	      FUNC, \
	      test_conform_simple_fixture_teardown);    } G_STMT_END

/* this is a macro that conditionally executes a test if CONDITION
 * evaluates to TRUE; otherwise, it will put the test under the
 * "/skip" namespace and execute a dummy function that will always
 * pass.
 */
#define TEST_CONFORM_SKIP(CONDITION, NAMESPACE, FUNC)   G_STMT_START {  \
  if (CONDITION) {                                                      \
    g_test_add ("/skipped" NAMESPACE "/" #FUNC,                         \
                TestConformSimpleFixture,                               \
                shared_state, /* data argument for test */              \
                test_conform_simple_fixture_setup,                      \
                test_conform_skip_test,                                 \
                test_conform_simple_fixture_teardown);                  \
  } else { TEST_CONFORM_SIMPLE (NAMESPACE, FUNC); }     } G_STMT_END

#define TEST_CONFORM_TODO(NAMESPACE, FUNC)              G_STMT_START {  \
  g_test_add ("/todo" NAMESPACE "/" #FUNC,                              \
              TestConformSimpleFixture,                                 \
              shared_state,                                             \
              test_conform_simple_fixture_setup,                        \
              test_conform_skip_test,                                   \
              test_conform_simple_fixture_teardown);    } G_STMT_END

gchar *
clutter_test_get_data_file (const gchar *filename)
{
  return g_build_filename (TESTS_DATADIR, filename, NULL);
}

static void
clutter_test_init (gint    *argc,
                   gchar ***argv)
{
#ifdef HAVE_CLUTTER_GLX
  /* on X11 we need a display connection to run the test suite */
  const gchar *display = g_getenv ("DISPLAY");
  if (!display || *display == '\0')
    {
      g_print ("No DISPLAY found. Unable to run the conformance "
               "test suite without a display.");

      exit (EXIT_SUCCESS);
    }
#endif

  /* Turning of sync-to-vblank removes a dependency on the specifics of the
   * test environment. It also means that the timeline-only tests are
   * throttled to a reasonable frame rate rather than running in tight
   * infinite loop.
   */
  g_setenv ("CLUTTER_VBLANK", "none", FALSE);

  g_test_init (argc, argv, NULL);

  g_test_bug_base ("http://bugzilla.openedhand.com/show_bug.cgi?id=%s");

  /* Initialise the state you need to share with everything.
   */
  shared_state = g_new0 (TestConformSharedState, 1);
  shared_state->argc_addr = argc;
  shared_state->argv_addr = argv;

  g_assert (clutter_init (shared_state->argc_addr, shared_state->argv_addr)
	    == CLUTTER_INIT_SUCCESS);
}

int
main (int argc, char **argv)
{
  clutter_test_init (&argc, &argv);

  TEST_CONFORM_SIMPLE ("/timeline", test_timeline);
  TEST_CONFORM_SKIP (!g_test_slow (), "/timeline", test_timeline_interpolate);
  TEST_CONFORM_SKIP (!g_test_slow (), "/timeline", test_timeline_rewind);

  TEST_CONFORM_SIMPLE ("/picking", test_pick);

  TEST_CONFORM_SIMPLE ("/text", test_text_utf8_validation);
  TEST_CONFORM_SIMPLE ("/text", test_text_empty);
  TEST_CONFORM_SIMPLE ("/text", test_text_set_empty);
  TEST_CONFORM_SIMPLE ("/text", test_text_set_text);
  TEST_CONFORM_SIMPLE ("/text", test_text_append_some);
  TEST_CONFORM_SIMPLE ("/text", test_text_prepend_some);
  TEST_CONFORM_SIMPLE ("/text", test_text_insert);
  TEST_CONFORM_SIMPLE ("/text", test_text_delete_chars);
  TEST_CONFORM_SIMPLE ("/text", test_text_delete_text);
  TEST_CONFORM_SIMPLE ("/text", test_text_cursor);
  TEST_CONFORM_SIMPLE ("/text", test_text_event);
  TEST_CONFORM_SIMPLE ("/text", test_text_get_chars);
  TEST_CONFORM_SIMPLE ("/text", test_text_cache);
  TEST_CONFORM_SIMPLE ("/text", test_text_password_char);

  TEST_CONFORM_SIMPLE ("/rectangle", test_rect_set_size);
  TEST_CONFORM_SIMPLE ("/rectangle", test_rect_set_color);

  TEST_CONFORM_SIMPLE ("/invariants", test_initial_state);
  TEST_CONFORM_SIMPLE ("/invariants", test_shown_not_parented);
  TEST_CONFORM_SIMPLE ("/invariants", test_realized);
  TEST_CONFORM_SIMPLE ("/invariants", test_realize_not_recursive);
  TEST_CONFORM_SIMPLE ("/invariants", test_map_recursive);
  TEST_CONFORM_SIMPLE ("/invariants", test_mapped);
  TEST_CONFORM_SIMPLE ("/invariants", test_show_on_set_parent);
  TEST_CONFORM_SIMPLE ("/invariants", test_clone_no_map);

  TEST_CONFORM_SIMPLE ("/opacity", test_label_opacity);
  TEST_CONFORM_SIMPLE ("/opacity", test_rectangle_opacity);
  TEST_CONFORM_SIMPLE ("/opacity", test_paint_opacity);

  TEST_CONFORM_SIMPLE ("/texture", test_texture_fbo);

  TEST_CONFORM_SIMPLE ("/path", test_path);

  TEST_CONFORM_SIMPLE ("/binding-pool", test_binding_pool);

  TEST_CONFORM_SIMPLE ("/actor", test_anchors);
  TEST_CONFORM_SIMPLE ("/actor", test_actor_destruction);

  TEST_CONFORM_SIMPLE ("/model", test_list_model_populate);
  TEST_CONFORM_SIMPLE ("/model", test_list_model_iterate);
  TEST_CONFORM_SIMPLE ("/model", test_list_model_filter);

  TEST_CONFORM_SIMPLE ("/color", test_color_from_string);
  TEST_CONFORM_SIMPLE ("/color", test_color_to_string);
  TEST_CONFORM_SIMPLE ("/color", test_color_hls_roundtrip);
  TEST_CONFORM_SIMPLE ("/color", test_color_operators);

  TEST_CONFORM_SIMPLE ("/units", test_units_constructors);
  TEST_CONFORM_SIMPLE ("/units", test_units_string);
  TEST_CONFORM_SIMPLE ("/units", test_units_cache);

  TEST_CONFORM_SIMPLE ("/group", test_group_depth_sorting);

  TEST_CONFORM_SIMPLE ("/sizing", test_fixed_size);
  TEST_CONFORM_SIMPLE ("/sizing", test_preferred_size);

  TEST_CONFORM_SIMPLE ("/script", test_script_single);
  TEST_CONFORM_SIMPLE ("/script", test_script_child);
  TEST_CONFORM_SIMPLE ("/script", test_script_implicit_alpha);
  TEST_CONFORM_SIMPLE ("/script", test_script_object_property);
  TEST_CONFORM_SIMPLE ("/script", test_script_animation);
  TEST_CONFORM_SIMPLE ("/script", test_script_named_object);

  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_fixed);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_backface_culling);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_materials);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_blend_strings);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_premult);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_readpixels);

  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_npot_texture);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_multitexture);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_texture_mipmaps);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_sub_texture);

  TEST_CONFORM_SIMPLE ("/cogl/vertex-buffer", test_cogl_vertex_buffer_contiguous);
  TEST_CONFORM_SIMPLE ("/cogl/vertex-buffer", test_cogl_vertex_buffer_interleved);
  TEST_CONFORM_SIMPLE ("/cogl/vertex-buffer", test_cogl_vertex_buffer_mutability);

  /* left to the end because they aren't currently very orthogonal and tend to
   * break subsequent tests! */
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_viewport);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_offscreen);

  return g_test_run ();
}
