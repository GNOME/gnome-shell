#include <stdlib.h>

#include <gmodule.h>

#include <clutter/clutter.h>

#define TEST_STATE_SCRIPT_FILE  "test-script-signals.json"

gboolean
on_button_press (ClutterActor *actor,
                 ClutterEvent *event,
                 gpointer      dummy G_GNUC_UNUSED)
{
  g_print ("Button pressed!\n");

  return FALSE;
}

G_MODULE_EXPORT int
test_state_script_main (int argc, char *argv[])
{
  ClutterActor *stage, *button;
  ClutterScript *script;
  GError *error = NULL;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  script = clutter_script_new ();
  clutter_script_load_from_file (script, TEST_STATE_SCRIPT_FILE, &error);
  if (error != NULL)
    g_error ("Unable to load '%s': %s\n",
             TEST_STATE_SCRIPT_FILE,
             error->message);

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "State Script");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_actor_show (stage);

  button = CLUTTER_ACTOR (clutter_script_get_object (script, "button"));
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), button);
  clutter_actor_add_constraint (button, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));

  clutter_script_connect_signals (script, NULL);

  clutter_main ();

  g_object_unref (script);

  return EXIT_SUCCESS;
}
