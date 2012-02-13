#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gmodule.h>

#include <clutter/clutter.h>

G_MODULE_EXPORT int
test_shader_effects_main (int argc, char *argv[])
{
  ClutterTimeline *timeline;
  ClutterActor *stage, *hand, *label, *rect;
  gchar *file;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  /* Make a timeline */
  timeline = clutter_timeline_new (7692);
  clutter_timeline_set_repeat_count (timeline, -1);

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Rotations");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Aluminium3);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Make a hand */
  file = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
  hand = clutter_texture_new_from_file (file, NULL);
  if (!hand)
    g_error("Unable to load '%s'", file);

  g_free (file);

  clutter_actor_set_position (hand, 326, 265);
  clutter_actor_add_effect_with_name (hand, "desaturate", clutter_desaturate_effect_new (0.75));
  clutter_actor_add_effect_with_name (hand, "blur", clutter_blur_effect_new ());
  clutter_actor_animate_with_timeline (hand, CLUTTER_LINEAR, timeline,
                                       "@effects.desaturate.factor", 1.0,
                                       "rotation-angle-z", 360.0,
                                       "fixed::anchor-x", 86.0,
                                       "fixed::anchor-y", 125.0,
                                       "opacity", 128,
                                       NULL);

  rect = clutter_rectangle_new_with_color (CLUTTER_COLOR_DarkOrange);
  clutter_actor_add_effect_with_name (rect, "blur", clutter_blur_effect_new ());
  clutter_actor_set_position (rect, 415, 215);
  clutter_actor_set_size (rect, 150, 150);
  clutter_actor_animate_with_timeline (rect, CLUTTER_LINEAR, timeline,
                                       "rotation-angle-z", 360.0,
                                       "fixed::anchor-x", 75.0,
                                       "fixed::anchor-y", 75.0,
                                       NULL);

  label = clutter_text_new_with_text ("Mono 16",
                                      "The Wonder\n"
                                      "of the\n"
                                      "Spinning Hand");
  clutter_text_set_line_alignment (CLUTTER_TEXT (label), PANGO_ALIGN_CENTER);
  clutter_actor_set_position (label, 336, 275);
  clutter_actor_set_size (label, 500, 100);
  clutter_actor_animate_with_timeline (label, CLUTTER_LINEAR, timeline,
                                       "rotation-angle-z", 360.0,
                                       "fixed::anchor-x", 86.0,
                                       "fixed::anchor-y", 125.0,
                                       NULL);

  clutter_container_add (CLUTTER_CONTAINER (stage), rect, hand, label, NULL);
  
  /* start the timeline and thus the animations */
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  g_object_unref (timeline);

  return 0;
}
