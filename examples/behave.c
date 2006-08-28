#include <clutter/clutter.h>

int
main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *behave;
  ClutterActor     *stage, *hand;
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  GdkPixbuf        *pixbuf;


  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  /* Make a hand */
  hand = clutter_texture_new_from_pixbuf (pixbuf);
  clutter_actor_set_position (hand, 100, 100);
  clutter_group_add (CLUTTER_GROUP (stage), hand);

  /* Make a timeline */
  timeline = clutter_timeline_new (100, 30); /* num frames, fps */
  g_object_set(timeline, "loop", TRUE, 0);  

  /* Set an alpha func to power behaviour - ramp is constant rise/fall */
  alpha = clutter_alpha_new (timeline, CLUTTER_ALPHA_RAMP);

  /* Create a behaviour for that time line */
  behave = clutter_behaviour_opacity_new (alpha, 0X33 ,0xff); 

  /* Apply it to our actor */
  clutter_behaviour_apply (behave, hand);

  /* start the timeline */
  clutter_timeline_start (timeline);

  clutter_group_show_all (CLUTTER_GROUP (stage));

  clutter_main();

  return 0;
}
