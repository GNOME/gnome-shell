#include "config.h"

#include "clutter-test-utils.h"

#include <stdlib.h>
#include <glib-object.h>

#include "clutter-actor.h"
#include "clutter-color.h"
#include "clutter-event.h"
#include "clutter-keysyms.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage.h"

typedef struct {
  ClutterActor *stage;
} ClutterTestEnvironment;

static ClutterTestEnvironment *test_environ = NULL;

/**
 * clutter_test_init:
 * @argc:
 * @argv:
 *
 * Initializes the Clutter test environment.
 *
 * Since: 1.18
 */
void
clutter_test_init (int    *argc,
                   char ***argv)
{
  if (G_UNLIKELY (test_environ != NULL))
    g_error ("Attempting to initialize the test suite more than once, "
             "aborting...\n");

#ifdef CLUTTER_WINDOWING_X11
  /* on X11 backends we need the DISPLAY environment set.
   *
   * check_windowing_backend() will pre-initialize the Clutter
   * backend object.
   */
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_X11))
    {
      const char *display = g_getenv ("DISPLAY");

      if (display == NULL || *display == '\0')
        {
          g_print ("No DISPLAY environment variable found, but we require a "
                   "DISPLAY set in order to run the conformance test suite.");
          exit (0);
        }
    }
#endif

  /* we explicitly disable the synchronisation to the vertical refresh
   * rate, and run the master clock using a 60 fps timer instead.
   */
  _clutter_set_sync_to_vblank (FALSE);

  g_test_init (argc, argv, NULL);
  g_test_bug_base ("https://bugzilla.gnome.org/show_bug.cgi?id=%s");

  /* perform the actual initialization */
  g_assert (clutter_init (NULL, NULL) == CLUTTER_INIT_SUCCESS);

  /* our global state, accessible from each test unit */
  test_environ = g_new0 (ClutterTestEnvironment, 1);
}

/**
 * clutter_test_get_stage:
 *
 * Retrieves the #ClutterStage used for testing.
 *
 * Return value: (transfer none): the stage used for testing
 *
 * Since: 1.18
 */
ClutterActor *
clutter_test_get_stage (void)
{
  g_assert (test_environ != NULL);

  if (test_environ->stage == NULL)
    {
      /* create a stage, and ensure that it goes away at the end */
      test_environ->stage = clutter_stage_new ();
      clutter_actor_set_name (test_environ->stage, "Test Stage");
      g_object_add_weak_pointer (G_OBJECT (test_environ->stage),
                                 (gpointer *) &test_environ->stage);
    }

  return test_environ->stage;
}

typedef struct {
  gpointer test_func;
  gpointer test_data;
  GDestroyNotify test_notify;
} ClutterTestData;

static void
clutter_test_func_wrapper (gconstpointer data_)
{
  const ClutterTestData *data = data_;

  /* ensure that the previous test state has been cleaned up */
  g_assert_null (test_environ->stage);

  if (data->test_data != NULL)
    {
      GTestDataFunc test_func = data->test_func;

      test_func (data->test_data);
    }
  else
    {
      GTestFunc test_func = data->test_func;

      test_func ();
    }

  if (data->test_notify != NULL)
    data->test_notify (data->test_data);

  if (test_environ->stage != NULL)
    {
      clutter_actor_destroy (test_environ->stage);
      g_assert_null (test_environ->stage);
    }
}

/**
 * clutter_test_add: (skip)
 * @test_path:
 * @test_func:
 *
 * Adds a test unit to the Clutter test environment.
 *
 * See also: g_test_add()
 *
 * Since: 1.18
 */
void
clutter_test_add (const char *test_path,
                  GTestFunc   test_func)
{
  clutter_test_add_data_full (test_path, (GTestDataFunc) test_func, NULL, NULL);
}

/**
 * clutter_test_add_data: (skip)
 * @test_path:
 * @test_func:
 * @test_data:
 *
 * Adds a test unit to the Clutter test environment.
 *
 * See also: g_test_add_data_func()
 *
 * Since: 1.18
 */
void
clutter_test_add_data (const char    *test_path,
                       GTestDataFunc  test_func,
                       gpointer       test_data)
{
  clutter_test_add_data_full (test_path, test_func, test_data, NULL);
}

/**
 * clutter_test_add_data_full:
 * @test_path:
 * @test_func: (scope notified)
 * @test_data:
 * @test_notify:
 *
 * Adds a test unit to the Clutter test environment.
 *
 * See also: g_test_add_data_func_full()
 *
 * Since: 1.18
 */
void
clutter_test_add_data_full (const char     *test_path,
                            GTestDataFunc   test_func,
                            gpointer        test_data,
                            GDestroyNotify  test_notify)
{
  ClutterTestData *data;

  g_return_if_fail (test_path != NULL);
  g_return_if_fail (test_func != NULL);

  g_assert (test_environ != NULL);

  data = g_new (ClutterTestData, 1);
  data->test_func = test_func;
  data->test_data = test_data;
  data->test_notify = test_notify;

  g_test_add_data_func_full (test_path, data,
                             clutter_test_func_wrapper,
                             g_free);
}

/**
 * clutter_test_run:
 *
 * Runs the test suite using the units added by calling
 * clutter_test_add().
 *
 * The typical test suite is composed of a list of functions
 * called by clutter_test_run(), for instance:
 *
 * |[
 * static void unit_foo (void) { ... }
 *
 * static void unit_bar (void) { ... }
 *
 * static void unit_baz (void) { ... }
 *
 * int
 * main (int argc, char *argv[])
 * {
 *   clutter_test_init (&amp;argc, &amp;argv);
 *
 *   clutter_test_add ("/unit/foo", unit_foo);
 *   clutter_test_add ("/unit/bar", unit_bar);
 *   clutter_test_add ("/unit/baz", unit_baz);
 *
 *   return clutter_test_run ();
 * }
 * ]|
 *
 * Return value: the exit code for the test suite
 *
 * Since: 1.18
 */
int
clutter_test_run (void)
{
  int res;

  g_assert (test_environ != NULL);
  
  res = g_test_run ();

  g_free (test_environ);

  return res;
}

typedef struct {
  ClutterActor *stage;

  ClutterPoint point;

  gpointer result;

  guint check_actor : 1;
  guint check_color : 1;

  guint was_painted : 1;
} ValidateData;

static gboolean
validate_stage (gpointer data_)
{
  ValidateData *data = data_;

  if (data->check_actor)
    {
      data->result =
        clutter_stage_get_actor_at_pos (CLUTTER_STAGE (data->stage),
                                        CLUTTER_PICK_ALL,
                                        data->point.x,
                                        data->point.y);
    }

  if (data->check_color)
    {
      data->result =
        clutter_stage_read_pixels (CLUTTER_STAGE (data->stage),
                                   data->point.x,
                                   data->point.y,
                                   1, 1);
    }

  if (!g_test_verbose ())
    {
      clutter_actor_hide (data->stage);
      data->was_painted = TRUE;
    }

  return G_SOURCE_REMOVE;
}

static gboolean
on_key_press_event (ClutterActor *stage,
                    ClutterEvent *event,
                    gpointer      data_)
{
  ValidateData *data = data_;

  if (data->stage == stage &&
      clutter_event_get_key_symbol (event) == CLUTTER_KEY_Escape)
    {
      clutter_actor_hide (stage);

      data->was_painted = TRUE;
    }

  return CLUTTER_EVENT_PROPAGATE;
}

gboolean
clutter_test_check_actor_at_point (ClutterActor        *stage,
                                   const ClutterPoint  *point,
                                   ClutterActor        *actor,
                                   ClutterActor       **result)
{
  ValidateData *data;
  guint press_id = 0;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (point != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (stage), FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  data = g_new0 (ValidateData, 1);
  data->stage = stage;
  data->point = *point;
  data->check_actor = TRUE;

  if (g_test_verbose ())
    {
      g_printerr ("Press ESC to close the stage and resume the test\n");
      press_id = g_signal_connect (stage, "key-press-event",
                                   G_CALLBACK (on_key_press_event),
                                   data);
    }

  clutter_actor_show (stage);

  clutter_threads_add_repaint_func_full (CLUTTER_REPAINT_FLAGS_POST_PAINT,
                                         validate_stage,
                                         data,
                                         NULL);

  while (!data->was_painted)
    g_main_context_iteration (NULL, TRUE);

  *result = data->result;

  if (press_id != 0)
    g_signal_handler_disconnect (stage, press_id);

  g_free (data);

  return *result == actor;
}

gboolean
clutter_test_check_color_at_point (ClutterActor       *stage,
                                   const ClutterPoint *point,
                                   const ClutterColor *color,
                                   ClutterColor       *result)
{
  ValidateData *data;
  gboolean retval;
  guint8 *buffer;
  guint press_id = 0;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (point != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  data = g_new0 (ValidateData, 1);
  data->stage = stage;
  data->point = *point;
  data->check_color = TRUE;

  if (g_test_verbose ())
    {
      g_printerr ("Press ESC to close the stage and resume the test\n");
      press_id = g_signal_connect (stage, "key-press-event",
                                   G_CALLBACK (on_key_press_event),
                                   data);
    }

  clutter_actor_show (stage);

  clutter_threads_add_repaint_func_full (CLUTTER_REPAINT_FLAGS_POST_PAINT,
                                         validate_stage,
                                         data,
                                         NULL);

  while (!data->was_painted)
    g_main_context_iteration (NULL, TRUE);

  if (press_id != 0)
    g_signal_handler_disconnect (stage, press_id);

  buffer = data->result;

  clutter_color_init (result, buffer[0], buffer[1], buffer[2], 255);

  /* we only check the color channels, so we can't use clutter_color_equal() */
  retval = buffer[0] == color->red &&
           buffer[1] == color->green &&
           buffer[2] == color->blue;

  g_free (data->result);
  g_free (data);

  return retval;
}
