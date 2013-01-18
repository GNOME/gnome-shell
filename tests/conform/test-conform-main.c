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
 * "/skipped" namespace and execute a dummy function that will always
 * pass.
 */
#define TEST_CONFORM_SKIP(CONDITION, NAMESPACE, FUNC)   G_STMT_START {  \
  if (CONDITION) { TEST_CONFORM_SIMPLE (NAMESPACE, FUNC); }             \
  else {                                                                \
    g_test_add ("/skipped" NAMESPACE "/" #FUNC,                         \
                TestConformSimpleFixture,                               \
                shared_state, /* data argument for test */              \
                test_conform_simple_fixture_setup,                      \
                test_conform_skip_test,                                 \
                test_conform_simple_fixture_teardown);                  \
  }                                                     } G_STMT_END

gchar *
clutter_test_get_data_file (const gchar *filename)
{
  return g_test_build_filename (G_TEST_DIST, "..", "data", filename, NULL);
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

  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

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
     each. To comment out a test use the SKIP macro. Using
     #if 0 would break the script. */

  TEST_CONFORM_SIMPLE ("/actor", actor_add_child);
  TEST_CONFORM_SIMPLE ("/actor", actor_insert_child);
  TEST_CONFORM_SIMPLE ("/actor", actor_raise_child);
  TEST_CONFORM_SIMPLE ("/actor", actor_lower_child);
  TEST_CONFORM_SIMPLE ("/actor", actor_replace_child);
  TEST_CONFORM_SIMPLE ("/actor", actor_remove_child);
  TEST_CONFORM_SIMPLE ("/actor", actor_remove_all);
  TEST_CONFORM_SIMPLE ("/actor", actor_container_signals);
  TEST_CONFORM_SIMPLE ("/actor", actor_destruction);
  TEST_CONFORM_SIMPLE ("/actor", actor_anchors);
  TEST_CONFORM_SIMPLE ("/actor", actor_pick);
  TEST_CONFORM_SIMPLE ("/actor", actor_fixed_size);
  TEST_CONFORM_SIMPLE ("/actor", actor_preferred_size);
  TEST_CONFORM_SIMPLE ("/actor", actor_basic_layout);
  TEST_CONFORM_SIMPLE ("/actor", actor_margin_layout);
  TEST_CONFORM_SIMPLE ("/actor", actor_offscreen_redirect);
  TEST_CONFORM_SIMPLE ("/actor", actor_offscreen_limit_max_size);
  TEST_CONFORM_SIMPLE ("/actor", actor_shader_effect);

  TEST_CONFORM_SIMPLE ("/actor/iter", actor_iter_traverse_children);
  TEST_CONFORM_SIMPLE ("/actor/iter", actor_iter_traverse_remove);
  TEST_CONFORM_SIMPLE ("/actor/iter", actor_iter_assignment);

  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_initial_state);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_shown_not_parented);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_visibility_not_recursive);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_realized);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_realize_not_recursive);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_map_recursive);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_mapped);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_show_on_set_parent);
  TEST_CONFORM_SIMPLE ("/actor/invariants", clone_no_map);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_contains);
  TEST_CONFORM_SIMPLE ("/actor/invariants", default_stage);
  TEST_CONFORM_SIMPLE ("/actor/invariants", actor_pivot_transformation);

  TEST_CONFORM_SIMPLE ("/actor/meta", actor_meta_clear);

  TEST_CONFORM_SIMPLE ("/actor/opacity", opacity_label);
  TEST_CONFORM_SIMPLE ("/actor/opacity", opacity_rectangle);
  TEST_CONFORM_SIMPLE ("/actor/opacity", opacity_paint);

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
  TEST_CONFORM_SIMPLE ("/text", text_idempotent_use_markup);

  TEST_CONFORM_SIMPLE ("/rectangle", rectangle_set_size);
  TEST_CONFORM_SIMPLE ("/rectangle", rectangle_set_color);

  TEST_CONFORM_SIMPLE ("/texture", texture_pick_with_alpha);
  TEST_CONFORM_SIMPLE ("/texture", texture_fbo);
  TEST_CONFORM_SIMPLE ("/texture/cairo", texture_cairo);

  TEST_CONFORM_SIMPLE ("/interval", interval_initial_state);
  TEST_CONFORM_SIMPLE ("/interval", interval_transform);

  TEST_CONFORM_SIMPLE ("/path", path_base);

  TEST_CONFORM_SIMPLE ("/binding-pool", binding_pool);

  TEST_CONFORM_SIMPLE ("/model", list_model_populate);
  TEST_CONFORM_SIMPLE ("/model", list_model_iterate);
  TEST_CONFORM_SIMPLE ("/model", list_model_filter);
  TEST_CONFORM_SIMPLE ("/model", list_model_from_script);
  TEST_CONFORM_SIMPLE ("/model", list_model_row_changed);

  TEST_CONFORM_SIMPLE ("/color", color_from_string_valid);
  TEST_CONFORM_SIMPLE ("/color", color_from_string_invalid);
  TEST_CONFORM_SIMPLE ("/color", color_to_string);
  TEST_CONFORM_SIMPLE ("/color", color_hls_roundtrip);
  TEST_CONFORM_SIMPLE ("/color", color_operators);

  TEST_CONFORM_SIMPLE ("/units", units_constructors);
  TEST_CONFORM_SIMPLE ("/units", units_string);
  TEST_CONFORM_SIMPLE ("/units", units_cache);

  TEST_CONFORM_SIMPLE ("/group", group_depth_sorting);

  TEST_CONFORM_SIMPLE ("/script", script_single);
  TEST_CONFORM_SIMPLE ("/script", script_child);
  TEST_CONFORM_SIMPLE ("/script", script_implicit_alpha);
  TEST_CONFORM_SIMPLE ("/script", script_object_property);
  TEST_CONFORM_SIMPLE ("/script", script_animation);
  TEST_CONFORM_SIMPLE ("/script", script_named_object);
  TEST_CONFORM_SIMPLE ("/script", script_layout_property);
  TEST_CONFORM_SIMPLE ("/script", animator_base);
  TEST_CONFORM_SIMPLE ("/script", animator_properties);
  TEST_CONFORM_SIMPLE ("/script", animator_multi_properties);
  TEST_CONFORM_SIMPLE ("/script", state_base);
  TEST_CONFORM_SIMPLE ("/script", script_margin);
  TEST_CONFORM_SIMPLE ("/script", script_interval);

  TEST_CONFORM_SKIP (g_test_slow (), "/timeline", timeline_base);
  TEST_CONFORM_SIMPLE ("/timeline", timeline_markers_from_script);
  TEST_CONFORM_SKIP (g_test_slow (), "/timeline", timeline_interpolation);
  TEST_CONFORM_SKIP (g_test_slow (), "/timeline", timeline_rewind);
  TEST_CONFORM_SIMPLE ("/timeline", timeline_progress_mode);
  TEST_CONFORM_SIMPLE ("/timeline", timeline_progress_step);

  TEST_CONFORM_SIMPLE ("/score", score_base);

  TEST_CONFORM_SIMPLE ("/behaviours", behaviours_base);

  TEST_CONFORM_SIMPLE ("/events", events_touch);

  return g_test_run ();
}
