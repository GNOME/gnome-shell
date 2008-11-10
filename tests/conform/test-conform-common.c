#include <clutter/clutter.h>

#include "test-conform-common.h"

/**
 * test_conform_simple_fixture_setup:
 *
 * Initialise stuff before each test is run
 */
void
test_conform_simple_fixture_setup (TestConformSimpleFixture *fixture,
				   gconstpointer data)
{
  /* const TestConformSharedState *shared_state = data; */
  ClutterActor *stage = clutter_stage_get_default ();
  GList *actors = clutter_container_get_children (CLUTTER_CONTAINER (stage));
  GList *tmp;

  /* To help reduce leakage between unit tests, we destroy all children of the stage */
  for (tmp = actors; tmp != NULL; tmp = tmp->next)
    {
      ClutterActor *leaked_actor = tmp->data;
      
      if (g_test_verbose ())
	g_print ("Freeing leaked actor %p\n", leaked_actor);
      clutter_actor_destroy (leaked_actor);
    }
}


/**
 * test_conform_simple_fixture_teardown:
 *
 * Cleanup stuff after each test has finished
 */
void
test_conform_simple_fixture_teardown (TestConformSimpleFixture *fixture,
				      gconstpointer data)
{
  /* const TestConformSharedState *shared_state = data; */
}

