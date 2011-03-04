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

static void
test_conform_todo_test (TestConformSimpleFixture *fixture,
                        gconstpointer             data)
{
#ifdef G_OS_UNIX
  const TestConformTodo *todo = data;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      todo->func (fixture, NULL);
      exit (0);
    }

  g_test_trap_assert_failed ();
#endif
}

void
verify_failure (TestConformSimpleFixture *fixture,
                gconstpointer             data)
{
  g_assert (FALSE);
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
  extern void FUNC (TestConformSimpleFixture *, gconstpointer);         \
  g_test_add ("/conform" NAMESPACE "/" #FUNC,                           \
	      TestConformSimpleFixture,                                 \
	      shared_state, /* data argument for test */                \
	      test_conform_simple_fixture_setup,                        \
	      FUNC,                                                     \
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
   extern void FUNC (TestConformSimpleFixture *, gconstpointer);        \
   TestConformTodo *_clos = g_new0 (TestConformTodo, 1);                \
   _clos->name = g_strdup ( #FUNC );                                    \
   _clos->func = FUNC;                                                  \
   g_test_add ("/todo" NAMESPACE "/" #FUNC,                             \
              TestConformSimpleFixture,                                 \
              _clos,                                                    \
              test_conform_simple_fixture_setup,                        \
              test_conform_todo_test,                                   \
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
}

int
main (int argc, char **argv)
{
  clutter_test_init (&argc, &argv);

  /* This file is run through a sed script during the make step so the
     lines containing the tests need to be formatted on a single line
     each. To comment out a test use the SKIP or TODO macros. Using
     #if 0 would break the script. */

  /* sanity check for the test suite itself */
  TEST_CONFORM_TODO ("/suite", verify_failure);

  TEST_CONFORM_SIMPLE ("/actor", actor_destruction);
  TEST_CONFORM_SIMPLE ("/actor", actor_anchors);
  TEST_CONFORM_SIMPLE ("/actor", actor_picking);
  TEST_CONFORM_SIMPLE ("/actor", actor_fixed_size);
  TEST_CONFORM_SIMPLE ("/actor", actor_preferred_size);
  TEST_CONFORM_SIMPLE ("/actor", test_offscreen_redirect);

  TEST_CONFORM_SIMPLE ("/invariants", test_initial_state);
  TEST_CONFORM_SIMPLE ("/invariants", test_shown_not_parented);
  TEST_CONFORM_SIMPLE ("/invariants", test_realized);
  TEST_CONFORM_SIMPLE ("/invariants", test_realize_not_recursive);
  TEST_CONFORM_SIMPLE ("/invariants", test_map_recursive);
  TEST_CONFORM_SIMPLE ("/invariants", test_mapped);
  TEST_CONFORM_SIMPLE ("/invariants", test_show_on_set_parent);
  TEST_CONFORM_SIMPLE ("/invariants", test_clone_no_map);
  TEST_CONFORM_SIMPLE ("/invariants", test_contains);

  TEST_CONFORM_SIMPLE ("/opacity", test_label_opacity);
  TEST_CONFORM_SIMPLE ("/opacity", test_rectangle_opacity);
  TEST_CONFORM_SIMPLE ("/opacity", test_paint_opacity);

  TEST_CONFORM_SIMPLE ("/text", text_utf8_validation);
  TEST_CONFORM_SIMPLE ("/text", text_set_empty);
  TEST_CONFORM_SIMPLE ("/text", text_set_text);
  TEST_CONFORM_SIMPLE ("/text", text_append_some);
  TEST_CONFORM_SIMPLE ("/text", text_prepend_some);
  TEST_CONFORM_SIMPLE ("/text", text_insert);
  TEST_CONFORM_SIMPLE ("/text", text_delete_chars);
  TEST_CONFORM_SIMPLE ("/text", text_delete_text);
  TEST_CONFORM_SIMPLE ("/text", text_cursor);
  TEST_CONFORM_SIMPLE ("/text", text_event);
  TEST_CONFORM_SIMPLE ("/text", text_get_chars);
  TEST_CONFORM_SIMPLE ("/text", text_cache);
  TEST_CONFORM_SIMPLE ("/text", text_password_char);

  TEST_CONFORM_SIMPLE ("/rectangle", test_rect_set_size);
  TEST_CONFORM_SIMPLE ("/rectangle", test_rect_set_color);

  TEST_CONFORM_SIMPLE ("/texture", test_texture_pick_with_alpha);
  TEST_CONFORM_SIMPLE ("/texture", test_texture_fbo);
  TEST_CONFORM_SIMPLE ("/texture/cairo", test_clutter_cairo_texture);

  TEST_CONFORM_SIMPLE ("/path", test_path);

  TEST_CONFORM_SIMPLE ("/binding-pool", test_binding_pool);

  TEST_CONFORM_SIMPLE ("/model", test_list_model_populate);
  TEST_CONFORM_SIMPLE ("/model", test_list_model_iterate);
  TEST_CONFORM_SIMPLE ("/model", test_list_model_filter);
  TEST_CONFORM_SIMPLE ("/model", test_list_model_from_script);

  TEST_CONFORM_SIMPLE ("/color", test_color_from_string);
  TEST_CONFORM_SIMPLE ("/color", test_color_to_string);
  TEST_CONFORM_SIMPLE ("/color", test_color_hls_roundtrip);
  TEST_CONFORM_SIMPLE ("/color", test_color_operators);

  TEST_CONFORM_SIMPLE ("/units", test_units_constructors);
  TEST_CONFORM_SIMPLE ("/units", test_units_string);
  TEST_CONFORM_SIMPLE ("/units", test_units_cache);

  TEST_CONFORM_SIMPLE ("/group", test_group_depth_sorting);

  TEST_CONFORM_SIMPLE ("/script", test_script_single);
  TEST_CONFORM_SIMPLE ("/script", test_script_child);
  TEST_CONFORM_SIMPLE ("/script", test_script_implicit_alpha);
  TEST_CONFORM_SIMPLE ("/script", test_script_object_property);
  TEST_CONFORM_SIMPLE ("/script", test_script_animation);
  TEST_CONFORM_SIMPLE ("/script", test_script_named_object);
  TEST_CONFORM_SIMPLE ("/script", test_animator_base);
  TEST_CONFORM_SIMPLE ("/script", test_animator_properties);
  TEST_CONFORM_SIMPLE ("/script", test_animator_multi_properties);
  TEST_CONFORM_SIMPLE ("/script", test_state_base);
  TEST_CONFORM_SIMPLE ("/script", test_script_layout_property);

  TEST_CONFORM_SIMPLE ("/timeline", test_timeline);
  TEST_CONFORM_SKIP (!g_test_slow (), "/timeline", timeline_interpolation);
  TEST_CONFORM_SKIP (!g_test_slow (), "/timeline", timeline_rewind);

  TEST_CONFORM_SIMPLE ("/score", test_score);

  TEST_CONFORM_SIMPLE ("/behaviours", test_behaviours);

  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_object);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_fixed);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_backface_culling);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_materials);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_pipeline_user_matrix);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_blend_strings);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_premult);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_readpixels);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_path);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_depth_test);

  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_npot_texture);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_multitexture);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_texture_mipmaps);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_sub_texture);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_pixel_array);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_texture_rectangle);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_texture_3d);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_wrap_modes);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_texture_pixmap_x11);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_texture_get_set_data);
  TEST_CONFORM_SIMPLE ("/cogl/texture", test_cogl_atlas_migration);

  TEST_CONFORM_SIMPLE ("/cogl/vertex-buffer", test_cogl_vertex_buffer_contiguous);
  TEST_CONFORM_SIMPLE ("/cogl/vertex-buffer", test_cogl_vertex_buffer_interleved);
  TEST_CONFORM_SIMPLE ("/cogl/vertex-buffer", test_cogl_vertex_buffer_mutability);

  TEST_CONFORM_SIMPLE ("/cogl/vertex-array", test_cogl_primitive);

  TEST_CONFORM_SIMPLE ("/cogl/shaders", test_cogl_just_vertex_shader);

  /* left to the end because they aren't currently very orthogonal and tend to
   * break subsequent tests! */
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_viewport);
  TEST_CONFORM_SIMPLE ("/cogl", test_cogl_offscreen);

  return g_test_run ();
}
