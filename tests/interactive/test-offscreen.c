#include <gmodule.h>
#include <clutter/clutter.h>

/* Very simple test just to see what happens setting up offscreen rendering */

G_MODULE_EXPORT int
test_offscreen_main (int argc, char *argv[])
{
  ClutterActor    *stage;
  gboolean         offscreen;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  /* Attempt to set up rendering offscreen */
  g_object_set (stage, "offscreen", TRUE, NULL);

  /* See if it worked */
  g_object_get (stage, "offscreen", &offscreen, NULL);

  if (offscreen == FALSE)
    printf ("FAIL: Unable to setup offscreen rendering\n.");
  else
    printf ("SUCCESS: Able to setup offscreen rendering\n.");

  clutter_actor_show_all (CLUTTER_ACTOR (stage));

  clutter_main();

  return 0;
}
