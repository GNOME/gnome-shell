#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gmodule.h>

#include <clutter/clutter.h>

G_MODULE_EXPORT int
test_rotate_main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *r_behave;
  ClutterActor     *stage;
  ClutterActor     *hand, *label, *rect;
  gchar            *file;

  clutter_init (&argc, &argv);

  /* Make a timeline */
  timeline = clutter_timeline_new (7692);
  clutter_timeline_set_loop (timeline, TRUE);

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Rotations");
  clutter_stage_set_color (CLUTTER_STAGE (stage), CLUTTER_COLOR_Aluminium3);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Make a hand */
  file = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
  hand = clutter_texture_new_from_file (file, NULL);
  if (!hand)
    g_error("Unable to load '%s'", file);

  g_free (file);

  rect = clutter_rectangle_new_with_color (CLUTTER_COLOR_DarkOrange);
  clutter_actor_add_effect_with_name (rect, "blur", clutter_blur_effect_new ());
  clutter_actor_set_position (rect, 340, 140);
  clutter_actor_set_size (rect, 150, 150);

  clutter_actor_set_position (hand, 240, 140);
  clutter_actor_add_effect_with_name (hand, "desaturate", clutter_desaturate_effect_new (0.75));
  clutter_actor_add_effect_with_name (hand, "blur", clutter_blur_effect_new ());
  clutter_actor_animate_with_timeline (hand, CLUTTER_LINEAR, timeline,
                                       "@effects.desaturate.factor", 1.0,
                                       NULL);

  label = clutter_text_new_with_text ("Mono 16",
                                      "The Wonder\n"
                                      "of the\n"
                                      "Spinning Hand");
  clutter_text_set_line_alignment (CLUTTER_TEXT (label), PANGO_ALIGN_CENTER);
  clutter_actor_set_position (label, 150, 150);
  clutter_actor_set_size (label, 500, 100);

  clutter_container_add (CLUTTER_CONTAINER (stage), rect, hand, label, NULL);
  
  /* Set an alpha func to power behaviour */
  alpha = clutter_alpha_new_full (timeline, CLUTTER_LINEAR);

  /* Create a behaviour for that alpha */
  r_behave = clutter_behaviour_rotate_new (alpha,
					   CLUTTER_Z_AXIS,
					   CLUTTER_ROTATE_CW,
					   0.0, 360.0); 

  clutter_behaviour_rotate_set_center (CLUTTER_BEHAVIOUR_ROTATE (r_behave),
                                       86, 125, 0);
  
  /* Apply it to our actor */
  clutter_behaviour_apply (r_behave, hand);
  clutter_behaviour_apply (r_behave, label);
  clutter_behaviour_apply (r_behave, rect);

  /* start the timeline and thus the animations */
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  g_object_unref (r_behave);

  return 0;
}
