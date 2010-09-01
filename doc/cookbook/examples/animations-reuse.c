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
  ClutterAnimator *bounce;

  script = clutter_script_new ();

  load_script_from_file (script, ANIMATION_FILE);

  clutter_script_get_objects (script,
                              "rig", &rig,
                              "bounce", &bounce,
                              NULL);

  /* remove the button press handler */
  g_signal_handlers_disconnect_matched (actor,
                                        G_SIGNAL_MATCH_FUNC,
                                        0,
                                        0,
                                        NULL,
                                        foo_button_pressed_cb,
                                        NULL);

  /* add a callback to clean up the script when the rig is destroyed */
  g_object_set_data_full (G_OBJECT (rig), "script", script, g_object_unref);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rig);

  clutter_actor_reparent (actor, rig);

  clutter_animator_start (bounce);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  ClutterScript *script;
  ClutterActor *stage;

  clutter_init (&argc, &argv);

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
