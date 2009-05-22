#include "config.h"

#include <clutter/clutter.h>

#include <glib.h>
#include <stdlib.h>

#include "test-conform-common.h"

static void
test_conform_skip_test (TestConformSimpleFixture *fixture,
                        gconstpointer             data)
{
  /* void */
}

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

int
main (int argc, char **argv)
{
  TestConformSharedState *shared_state = g_new0 (TestConformSharedState, 1);

#ifdef HAVE_CLUTTER_GLX
  /* on X11 we need a display connection to run the test suite */
  const gchar *display = g_getenv ("DISPLAY");
  if (!display || *display == '\0')
    {
      g_print ("No DISPLAY found. Unable to run the conformance "
               "test suite without a display.");
      return EXIT_SUCCESS;
    }
#endif

  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.openedhand.com/show_bug.cgi?id=%s");

  /* Initialise the state you need to share with everything.
   */
  shared_state->argc_addr = &argc;
  shared_state->argv_addr = &argv;

  g_assert (clutter_init (shared_state->argc_addr, shared_state->argv_addr)
	    == CLUTTER_INIT_SUCCESS);

  TEST_CONFORM_SIMPLE ("/timeline", test_timeline);
  TEST_CONFORM_SKIP (!g_test_slow (), "/timeline", test_timeline_interpolate);
  TEST_CONFORM_SKIP (!g_test_slow (), "/timeline", test_timeline_rewind);

  TEST_CONFORM_SIMPLE ("/picking", test_pick);

  /* ClutterText */
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

  TEST_CONFORM_SIMPLE ("/fixed", test_fixed_constants);

  TEST_CONFORM_SIMPLE ("/invariants", test_initial_state);
  TEST_CONFORM_SIMPLE ("/invariants", test_shown_not_parented);
  TEST_CONFORM_SIMPLE ("/invariants", test_realized);
  TEST_CONFORM_SIMPLE ("/invariants", test_realize_not_recursive);
  TEST_CONFORM_SIMPLE ("/invariants", test_map_recursive);
  TEST_CONFORM_SIMPLE ("/invariants", test_mapped);
  TEST_CONFORM_SIMPLE ("/invariants", test_show_on_set_parent);

  TEST_CONFORM_SIMPLE ("/vertex-buffer", test_vertex_buffer_contiguous);
  TEST_CONFORM_SIMPLE ("/vertex-buffer", test_vertex_buffer_interleved);
  TEST_CONFORM_SIMPLE ("/vertex-buffer", test_vertex_buffer_mutability);

  TEST_CONFORM_SIMPLE ("/opacity", test_label_opacity);
  TEST_CONFORM_SIMPLE ("/opacity", test_rectangle_opacity);
  TEST_CONFORM_SIMPLE ("/opacity", test_paint_opacity);

  TEST_CONFORM_SIMPLE ("/texture", test_backface_culling);
  TEST_CONFORM_SIMPLE ("/texture", test_npot_texture);

  TEST_CONFORM_SIMPLE ("/path", test_path);

  TEST_CONFORM_SIMPLE ("/binding-pool", test_binding_pool);

  TEST_CONFORM_SIMPLE ("/actor", test_anchors);

  TEST_CONFORM_SIMPLE ("/model", test_list_model_populate);
  TEST_CONFORM_SIMPLE ("/model", test_list_model_iterate);
  TEST_CONFORM_SIMPLE ("/model", test_list_model_filter);

  TEST_CONFORM_SIMPLE ("/material", test_blend_strings);

  TEST_CONFORM_SIMPLE ("/color", test_color_from_string);
  TEST_CONFORM_SIMPLE ("/color", test_color_to_string);

  TEST_CONFORM_SIMPLE ("/units", test_units_constructors);
  TEST_CONFORM_SIMPLE ("/units", test_units_string);

  return g_test_run ();
}
