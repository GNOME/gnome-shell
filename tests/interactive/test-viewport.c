#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gmodule.h>

#include <clutter/clutter.h>

G_MODULE_EXPORT int
test_viewport_main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *r_behave;
  ClutterActor     *stage;
  ClutterActor     *hand;
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  /* Make a hand */
  hand = clutter_texture_new_from_file ("redhand.png", NULL);
  if (!hand)
    g_error("pixbuf load failed");

  clutter_actor_set_position (hand, 300, 200);
  clutter_actor_set_clip (hand, 20, 21, 132, 170);
  clutter_actor_set_anchor_point (hand, 86, 125);
  clutter_actor_show (hand);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), hand);

  /* Make a timeline */
  timeline = clutter_timeline_new (200, 26); /* num frames, fps */
  g_object_set (timeline, "loop", TRUE, NULL);  

  /* Set an alpha func to power behaviour - ramp is constant rise/fall */
  alpha = clutter_alpha_new_full (timeline,
                                  CLUTTER_ALPHA_RAMP_INC,
                                  NULL, NULL);

  /* Create a behaviour for that alpha */
  r_behave = clutter_behaviour_rotate_new (alpha,
					   CLUTTER_Z_AXIS,
					   CLUTTER_ROTATE_CW,
					   0.0, 360.0); 

  /* Apply it to our actor */
  clutter_behaviour_apply (r_behave, hand);

  /* start the timeline and thus the animations */
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  g_object_unref (r_behave);

  return 0;
}
