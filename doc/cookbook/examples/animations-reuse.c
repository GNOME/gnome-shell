#include <stdlib.h>
#include <clutter/clutter.h>

#define UI_FILE "animations-reuse-ui.json"
#define ANIMATION_FILE "animations-reuse-animation.json"

static gboolean
load_script_from_file (ClutterScript *script,
                       gchar         *filename)
{
  GError *error = NULL;

  clutter_script_load_from_file (script, filename, &error);

  if (error != NULL)
    {
      g_critical ("Error loading ClutterScript file %s\n%s", filename, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  return TRUE;
}

gboolean
foo_button_pressed_cb (ClutterActor *actor,
                       ClutterEvent *event,
                       gpointer      user_data)
{
  ClutterScript *ui = CLUTTER_SCRIPT (user_data);
  ClutterStage *stage = CLUTTER_STAGE (clutter_script_get_object (ui, "stage"));

  ClutterScript *script;
  ClutterActor *rig;
  ClutterAnimator *animator;

  /* load the rig and its animator from a JSON file */
  script = clutter_script_new ();

  /* use a function defined statically in this source file to load the JSON */
  load_script_from_file (script, ANIMATION_FILE);

  clutter_script_get_objects (script,
                              "rig", &rig,
                              "animator", &animator,
                              NULL);

  /* remove the button press handler from the rectangle */
  g_signal_handlers_disconnect_by_func (actor,
                                        G_CALLBACK (foo_button_pressed_cb),
                                        NULL);

  /* add a callback to clean up the script when the rig is destroyed */
  g_object_set_data_full (G_OBJECT (rig), "script", script, g_object_unref);

  /* add the rig to the stage */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rig);

  /* place the rig at the same coordinates on the stage as the rectangle */
  clutter_actor_set_position (rig,
                              clutter_actor_get_x (actor),
                              clutter_actor_get_y (actor));

  /* put the rectangle into the top-left corner of the rig */
  clutter_actor_reparent (actor, rig);

  clutter_actor_set_position (actor, 0, 0);

  /* animate the rig */
  clutter_animator_start (animator);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  ClutterScript *script;
  ClutterActor *stage;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  script = clutter_script_new ();
  load_script_from_file (script, UI_FILE);

  clutter_script_connect_signals (script, script);

  clutter_script_get_objects (script,
                              "stage", &stage,
                              NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (script);

  return EXIT_SUCCESS;
}
