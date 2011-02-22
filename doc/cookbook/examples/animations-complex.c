#include <stdlib.h>
#include <clutter/clutter.h>

#define UI_FILE "animations-complex.json"

/*
 * start the animation when a key is pressed;
 * see the signals recipe in the Script chapter for more details
 */
gboolean
foo_key_pressed_cb (ClutterActor *actor,
                    ClutterEvent *event,
                    gpointer      user_data)
{
  ClutterScript *script = CLUTTER_SCRIPT (user_data);

  ClutterAnimator *animator;
  clutter_script_get_objects (script,
                              "animator", &animator,
                              NULL);

  if (clutter_timeline_is_playing (clutter_animator_get_timeline (animator)))
    return FALSE;

  clutter_animator_start (animator);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  gchar *filename = UI_FILE;

  ClutterScript *script;
  ClutterActor *stage;

  GError *error = NULL;

  if (argc > 1)
    filename = argv[1];

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  script = clutter_script_new ();
  clutter_script_load_from_file (script, filename, &error);

  if (error != NULL)
   {
     g_critical ("Error loading ClutterScript file %s\n%s", filename, error->message);
     g_error_free (error);
     exit (EXIT_FAILURE);
   }

  /* connect signal handlers as defined in the script */
  clutter_script_connect_signals (script, script);

  clutter_script_get_objects (script,
                              "stage", &stage,
                              NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (script);

  return EXIT_SUCCESS;
}
