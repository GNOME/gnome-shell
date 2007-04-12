#include <clutter/clutter.h>

#define MAGIC 0.551784
#define RADIUS 200

int
main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *p_behave;
  ClutterActor     *stage;
  ClutterActor     *hand;
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  GdkPixbuf        *pixbuf;
  guint             i;
  
#if 1
  /* Circular path defined using a spline consisting of 4 bezier curves */
  ClutterKnot       knots[] = {{ -RADIUS, 0 },
			       { -RADIUS, RADIUS*MAGIC },
			       { -RADIUS*MAGIC, RADIUS },
                               { 0, RADIUS },
			       { RADIUS*MAGIC, RADIUS },
			       { RADIUS, RADIUS*MAGIC },
			       { RADIUS, 0 },
			       { RADIUS, -RADIUS*MAGIC },
			       { RADIUS*MAGIC, -RADIUS },
			       { 0, -RADIUS },
			       { -RADIUS*MAGIC, -RADIUS },
			       { -RADIUS, -RADIUS*MAGIC },
			       { -RADIUS, 0}};

  ClutterKnot       origin = { 0, RADIUS };
#else
  /* Approximation of a sine wave over <0,pi> */
  ClutterKnot       knots[] = {{ 0, 0 },
			       { 2*RADIUS/3, 2*RADIUS*1.299 },
			       { 4*RADIUS/3, 2*RADIUS*1.299 },
                               { 2*RADIUS, 0 }};

  ClutterKnot       origin = { 0, 0};
#endif
  
  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_hide_cursor (CLUTTER_STAGE (stage));

  g_signal_connect (stage, "key-press-event",
                    G_CALLBACK (clutter_main_quit),
                    NULL);

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  /* Make a hand */
  hand = clutter_texture_new_from_pixbuf (pixbuf);
  clutter_actor_set_position (hand, 0, RADIUS);
  clutter_actor_show (hand);
  clutter_group_add (CLUTTER_GROUP (stage), hand);

  /* Make a timeline */
  timeline = clutter_timeline_new (100, 26); /* num frames, fps */
  g_object_set (timeline, "loop", TRUE, 0);  

  /* Set an alpha func to power behaviour - ramp_inc is constant rise */
  alpha = clutter_alpha_new_full (timeline,
                                  CLUTTER_ALPHA_RAMP_INC,
                                  NULL, NULL);

  /* Make a path behaviour and apply that too */
  p_behave =
      clutter_behaviour_bspline_new (alpha, knots,
				     sizeof (knots)/sizeof(ClutterKnot));

  clutter_behaviour_bspline_set_origin (CLUTTER_BEHAVIOUR_BSPLINE (p_behave),
 					&origin);

  clutter_behaviour_apply (p_behave, hand);

  /* start the timeline and thus the animations */
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  g_object_unref (p_behave);

  return 0;
}
