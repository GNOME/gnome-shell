#include "config.h"

#include <clutter/clutter.h>
#include <stdlib.h>

#ifdef CLUTTER_WINDOWING_X11
#include <X11/Xlib.h>
#include <clutter/x11/clutter-x11.h>
#endif

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
  const TestConformSharedState *shared_state = data;
  static int counter = 0;

  if (counter != 0)
    g_critical ("We don't support running more than one test at a time\n"
                "in a single test run due to the state leakage that often\n"
                "causes subsequent tests to fail.\n"
                "\n"
                "If you want to run all the tests you should run\n"
                "$ make test-report");
  counter++;

#ifdef CLUTTER_WINDOWING_X11
  {
    /* on X11 we need a display connection to run the test suite */
    const gchar *display = g_getenv ("DISPLAY");
    if (!display || *display == '\0')
      {
        g_print ("No DISPLAY found. Unable to run the conformance "
                 "test suite without a display.\n");

        exit (EXIT_SUCCESS);
      }
  }

  /* enable XInput support */
  clutter_x11_enable_xinput ();
#endif

  g_assert (clutter_init (shared_state->argc_addr, shared_state->argv_addr)
            == CLUTTER_INIT_SUCCESS);

#ifdef CLUTTER_WINDOWING_X11
  /* A lot of the tests depend on a specific stage / framebuffer size
   * when they read pixels back to verify the results of the test.
   *
   * Normally the asynchronous nature of X means that setting the
   * clutter stage size may really happen an indefinite amount of time
   * later but since the tests are so short lived and may only render
   * a single frame this is not an acceptable semantic.
   */
  XSynchronize (clutter_x11_get_default_display(), TRUE);
#endif
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

void
test_conform_get_gl_functions (TestConformGLFunctions *functions)
{
  functions->glGetString = (void *) cogl_get_proc_address ("glGetString");
  g_assert (functions->glGetString != NULL);
  functions->glGetIntegerv = (void *) cogl_get_proc_address ("glGetIntegerv");
  g_assert (functions->glGetIntegerv != NULL);
  functions->glPixelStorei = (void *) cogl_get_proc_address ("glPixelStorei");
  g_assert (functions->glPixelStorei != NULL);
  functions->glBindTexture = (void *) cogl_get_proc_address ("glBindTexture");
  g_assert (functions->glBindTexture != NULL);
  functions->glGenTextures = (void *) cogl_get_proc_address ("glGenTextures");
  g_assert (functions->glGenTextures != NULL);
  functions->glGetError = (void *) cogl_get_proc_address ("glGetError");
  g_assert (functions->glGetError != NULL);
  functions->glDeleteTextures =
    (void *) cogl_get_proc_address ("glDeleteTextures");
  g_assert (functions->glDeleteTextures != NULL);
  functions->glTexImage2D = (void *) cogl_get_proc_address ("glTexImage2D");
  g_assert (functions->glTexImage2D != NULL);
  functions->glTexParameteri =
    (void *) cogl_get_proc_address ("glTexParameteri");
  g_assert (functions->glTexParameteri != NULL);
}
