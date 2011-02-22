#include <stdlib.h>
#include <clutter/clutter.h>

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterScript *ui;

  gchar *filename = "script-ui.json";
  GError *error = NULL;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  ui = clutter_script_new ();

  /* load a JSON file into the script */
  clutter_script_load_from_file (ui, filename, &error);

  if (error != NULL)
    {
      g_critical ("Error loading ClutterScript file %s\n%s", filename, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  /* retrieve objects from the script */
  clutter_script_get_objects (ui,
                              "stage", &stage,
                              NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
