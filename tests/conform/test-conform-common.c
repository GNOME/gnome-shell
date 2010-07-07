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
  static int counter = 0;

  if (counter != 0)
    g_critical ("We don't support running more than one test at a time\n"
                "in a single test run due to the state leakage that often\n"
                "causes subsequent tests to fail.\n"
                "\n"
                "If you want to run all the tests you should run\n"
                "$ make test-report");
  counter++;
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

