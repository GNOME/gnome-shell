#include <clutter/clutter.h>

void
frame_cb (ClutterTimeline *timeline, 
	  gint             frame_num, 
	  gpointer         data)
{
  ClutterActor *label = (ClutterActor*)data;

  clutter_actor_set_depth(label, -400 + (frame_num * 40));
  clutter_actor_set_opacity (label, 255 - frame_num );
}


int
main (int argc, char *argv[])
{
  ClutterTimeline *timeline;
  ClutterActor  *label;
  ClutterActor  *stage;
  gchar           *text;
  gsize            size;
  ClutterColor     stage_color = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor     label_color = { 0x11, 0xdd, 0x11, 0xaa };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  if (!g_file_get_contents ("test-text.c", &text, &size, NULL)) 
    g_error("g_file_get_contents() of test-text.c failed");

  clutter_actor_set_size (stage, 800, 600);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  label = clutter_label_new_with_text ("Mono 8", text);
  clutter_label_set_color (CLUTTER_LABEL (label), &label_color);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
  clutter_actor_show_all (stage);

  timeline = clutter_timeline_new (400, 60); /* num frames, fps */
  g_object_set(timeline, "loop", TRUE, 0);   /* have it loop */

  g_signal_connect(timeline, "new-frame",  G_CALLBACK (frame_cb), label);

  /* and start it */
  clutter_timeline_start (timeline);

  g_signal_connect (stage, "key-press-event",
		    G_CALLBACK (clutter_main_quit), NULL);

  clutter_main();

  return 0;
}
