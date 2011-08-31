#include <stdlib.h>
#include <clutter/clutter.h>

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterScript *ui;

  gchar *filename = "script-states.json";
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
