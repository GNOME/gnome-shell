#include <stdlib.h>
#include <clutter/clutter.h>

int
main (int argc, char *argv[])
{
  clutter_init (&argc, &argv);

  ClutterScript *ui = clutter_script_new ();

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

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
