/* Example of using a custom CbPageFoldEffect to do
 * an animated fold of a texture containing an image
 *
 * Pass the full path to the image on the command line;
 * click on the texture to trigger the folding animation
 */
#include <stdlib.h>
#include <clutter/clutter.h>

#include "cb-page-fold-effect.h"

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };

static gboolean
button_pressed_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  ClutterState *transitions = CLUTTER_STATE (user_data);

  if (g_strcmp0 (clutter_state_get_state (transitions), "folded") == 0)
    clutter_state_set_state (transitions, "unfolded");
  else
    clutter_state_set_state (transitions, "folded");

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *texture;
  ClutterEffect *effect;
  ClutterState *transitions;
  GError *error = NULL;

  gchar *filename;

  if (argc < 2)
    {
      g_print ("Usage: %s <path to image file>\n", argv[0]);
      return EXIT_FAILURE;
    }

  filename = argv[1];

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 300);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  texture = clutter_texture_new ();
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture), TRUE);
  clutter_actor_set_width (texture, 400);
  clutter_actor_set_reactive (texture, TRUE);
  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                 filename,
                                 &error);

  if (error != NULL)
    {
      g_critical ("Error loading texture from file %s; error was:\n%s",
                  filename,
                  error->message);
      return EXIT_FAILURE;
    }

  /* create the page fold effect instance with destination fold angle
   * of 180 degrees and starting period of 0 (no folding)
   */
  effect = cb_page_fold_effect_new (180.0, 0.0);

  /* add the effect to the texture actor */
  clutter_actor_add_effect (texture, effect);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), texture);

  /* animation for the period property of the effect,
   * to animate its value between 0.0 and 1.0 and back
   */
  transitions = clutter_state_new ();
  clutter_state_set_duration (transitions, NULL, NULL, 500);

  clutter_state_set_duration (transitions,
                              "partially-folded",
                              "folded",
                              375);

  clutter_state_set (transitions, NULL, "folded",
                     effect, "period", CLUTTER_LINEAR, 1.0,
                     NULL);

  clutter_state_set (transitions, NULL, "partially-folded",
                     effect, "period", CLUTTER_LINEAR, 0.25,
                     NULL);

  clutter_state_set (transitions, NULL, "unfolded",
                     effect, "period", CLUTTER_LINEAR, 0.0,
                     NULL);

  clutter_state_warp_to_state (transitions, "partially-folded");

  g_signal_connect (texture,
                    "button-press-event",
                    G_CALLBACK (button_pressed_cb),
                    transitions);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (transitions);

  return EXIT_SUCCESS;
}
