#include <stdlib.h>
#include <clutter/clutter.h>

/* callbacks cannot be declared static as they
 * are looked up dynamically by ClutterScript
 */
gboolean
foo_pointer_motion_cb (ClutterActor *actor,
                       ClutterEvent *event,
                       gpointer      user_data)
{
  gfloat x, y;
  clutter_event_get_coords (event, &x, &y);

  g_print ("Pointer movement at %.0f,%.0f\n", x, y);

  return TRUE;
}

void
foo_button_clicked_cb (ClutterClickAction *action,
                       ClutterActor       *actor,
                       gpointer            user_data)
{
  gfloat z_angle;

  /* get the UI definition passed to the handler */
  ClutterScript *ui = CLUTTER_SCRIPT (user_data);

  /* get the rectangle defined in the JSON */
  ClutterActor *rectangle;
  clutter_script_get_objects (ui,
                              "rectangle", &rectangle,
                              NULL);

  /* do nothing if the actor is already animating */
  if (clutter_actor_get_animation (rectangle) != NULL)
    return;

  /* get the current rotation and increment it */
  z_angle = clutter_actor_get_rotation (rectangle,
                                        CLUTTER_Z_AXIS,
                                        NULL, NULL, NULL);

  if (clutter_click_action_get_button (action) == 1)
    z_angle += 90.0;
  else
    z_angle -= 90.0;

  /* animate to new rotation angle */
  clutter_actor_animate (rectangle,
                         CLUTTER_EASE_OUT_CUBIC,
                         1000,
                         "rotation-angle-z", z_angle,
                         NULL);
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterScript *ui;

  gchar *filename = "script-signals.json";
  GError *error = NULL;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  ui = clutter_script_new ();

  clutter_script_load_from_file (ui, filename, &error);

  if (error != NULL)
    {
      g_critical ("Error loading ClutterScript file %s\n%s", filename, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  clutter_script_get_objects (ui,
                              "stage", &stage,
                              NULL);

  /* make the objects in the script available to all signals
   * by passing the script as the second argument
   * to clutter_script_connect_signals()
   */
  clutter_script_connect_signals (ui, ui);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (ui);

  return EXIT_SUCCESS;
}
