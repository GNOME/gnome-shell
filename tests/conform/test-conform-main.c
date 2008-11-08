#include <clutter/clutter.h>

#include <glib.h>
#include <stdlib.h>

#include "test-conform-common.h"


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
#define TEST_CONFORM_SIMPLE(NAMESPACE, FUNC) \
  extern void FUNC (TestConformSimpleFixture *fixture, gconstpointer data); \
  g_test_add ("/conform" NAMESPACE "/" #FUNC, \
	      TestConformSimpleFixture, \
	      shared_state, /* data argument for test */ \
	      test_conform_simple_fixture_setup, \
	      FUNC, \
	      test_conform_simple_fixture_teardown);


int
main (int argc, char **argv)
{
  TestConformSharedState *shared_state = g_new0 (TestConformSharedState, 1);

  g_test_init (&argc, &argv, NULL);
  
  g_test_bug_base ("http://bugzilla.openedhand.com/show_bug.cgi?id=%s");

  g_assert (clutter_init (shared_state->argc_addr, shared_state->argv_addr)
	    == CLUTTER_INIT_SUCCESS);
  
  /* Initialise the state you need to share with everything.
   */
  shared_state->argc_addr = &argc;
  shared_state->argv_addr = &argv;
  
  if (g_test_slow ())
    {
      TEST_CONFORM_SIMPLE ("/timeline", test_timeline_dup_frames);
      TEST_CONFORM_SIMPLE ("/timeline", test_timeline_interpolate);
      TEST_CONFORM_SIMPLE ("/timeline", test_timeline_rewind);
      TEST_CONFORM_SIMPLE ("/timeline", test_timeline_smoothness);
    }

  TEST_CONFORM_SIMPLE ("/picking", test_pick);

  TEST_CONFORM_SIMPLE ("/label", test_label_cache);

  TEST_CONFORM_SIMPLE ("/entry", test_entry_utf8_validation);
  TEST_CONFORM_SIMPLE ("/entry", test_entry_empty);
  TEST_CONFORM_SIMPLE ("/entry", test_entry_set_empty);
  TEST_CONFORM_SIMPLE ("/entry", test_entry_set_text);

  TEST_CONFORM_SIMPLE ("/entry", test_entry_append_some);
  TEST_CONFORM_SIMPLE ("/entry", test_entry_prepend_some);
  TEST_CONFORM_SIMPLE ("/entry", test_entry_insert);

  TEST_CONFORM_SIMPLE ("/entry", test_entry_delete_chars);
  TEST_CONFORM_SIMPLE ("/entry", test_entry_delete_text);

  TEST_CONFORM_SIMPLE ("/entry", test_entry_cursor);
  TEST_CONFORM_SIMPLE ("/entry", test_entry_event);

  TEST_CONFORM_SIMPLE ("/rectangle", test_rect_set_size);
  TEST_CONFORM_SIMPLE ("/rectangle", test_rect_set_color);

  TEST_CONFORM_SIMPLE ("/fixed", test_fixed_constants);

  TEST_CONFORM_SIMPLE ("/invariants", test_initial_state);
  TEST_CONFORM_SIMPLE ("/invatiants", test_realized);
  TEST_CONFORM_SIMPLE ("/invariants", test_mapped);
  TEST_CONFORM_SIMPLE ("/invariants", test_show_on_set_parent);

  g_test_run ();
  return EXIT_SUCCESS;
}

