#include <stdlib.h>
#include <clutter/clutter.h>

gboolean
_pointer_motion_cb (ClutterActor *actor,
                    ClutterEvent *event,
                    gpointer      user_data)
{
  g_debug ("Pointer movement");
  return TRUE;
}

void
_button_clicked_cb (ClutterClickAction *action,
                    ClutterActor       *actor,
                    gpointer            user_data)
{
  /* get the UI definition passed to the handler */
  ClutterScript *ui = CLUTTER_SCRIPT (user_data);

  ClutterState *transitions;

  clutter_script_get_objects (ui,
                              "transitions", &transitions,
                              NULL);

  clutter_state_set_state (transitions, "faded-in");
}

int
main (int argc, char *argv[])
{
  g_thread_init (NULL);

  clutter_init (&argc, &argv);

  /* path to the directory containing assets (e.g. images) for the script to load */
  const gchar *paths[] = { TESTS_DATA_DIR };

  ClutterScript *ui = clutter_script_new ();
  clutter_script_add_search_paths (ui, paths, 1);

  gchar *filename = "script-ui.json";
  GError *error = NULL;

  clutter_script_load_from_file (ui, filename, &error);

  if (error != NULL)
    {
      g_critical ("Error loading ClutterScript file %s\n%s", filename, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  ClutterActor *stage;

  clutter_script_get_objects (ui,
                              "stage", &stage,
                              NULL);

  /* make the objects in the script available to all signals */
  clutter_script_connect_signals (ui, ui);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
