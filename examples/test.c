#include <clutter/clutter.h>

guint8 opacity = 255;

gboolean 
timeout_cb (gpointer data)
{
  ClutterActor *actor;

  actor = CLUTTER_ACTOR(data);

  if (opacity > 0)
    {
      clutter_actor_set_opacity (actor, opacity);
      opacity -= 2;
    }
  else opacity = 0xff;

  return TRUE;
}

gboolean 
timeout_text_cb (gpointer data)
{
  ClutterLabel *label;
  gchar buf[32];

  label = CLUTTER_LABEL(data);

  g_snprintf(buf, 32, "--> %i <--", opacity);

  if (opacity > 0)
    {
      clutter_label_set_text(label, buf);
      clutter_actor_set_opacity (CLUTTER_ACTOR(label), opacity);
      opacity -= 2;
    }
  else opacity = 0xff;



  return TRUE;
}

void
frame_cb (ClutterTimeline *timeline, 
	  gint             frame_num, 
	  gpointer         data)
{
  ClutterLabel *label;
  gchar buf[32];

  label = CLUTTER_LABEL(data);

  opacity = frame_num/2;

  g_snprintf(buf, 32, "--> %i <--", frame_num);

  clutter_label_set_text (label, buf);
  clutter_actor_set_opacity (CLUTTER_ACTOR(label), opacity); 

  clutter_actor_rotate_z (CLUTTER_ACTOR(label),
			    frame_num,
			    clutter_actor_get_width (CLUTTER_ACTOR(label))/2,
			    clutter_actor_get_height (CLUTTER_ACTOR(label))/2);
}

int
main (int argc, char *argv[])
{
  ClutterActor  *texture, *label;
  ClutterActor  *stage;
  ClutterTimeline *timeline;
  GdkPixbuf       *pixbuf;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  pixbuf = gdk_pixbuf_new_from_file ("clutter-logo-800x600.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  texture = clutter_texture_new_from_pixbuf (pixbuf);

  printf("***********foo***********\n");

  label = clutter_label_new_with_text("Sans Bold 72", "Clutter\nOpened\nHand");

  printf("***********foo***********\n");

  clutter_actor_set_opacity (CLUTTER_ACTOR(label), 0x99);
  clutter_actor_set_position (CLUTTER_ACTOR(label), 100, 200);

  clutter_group_add (CLUTTER_GROUP (stage), texture);
  clutter_group_add (CLUTTER_GROUP (stage), label);

  clutter_actor_set_size (CLUTTER_ACTOR (stage), 800, 600);

  clutter_group_show_all (CLUTTER_GROUP (stage));

  timeline = clutter_timeline_new (360, 200);
  g_object_set (timeline, "loop", TRUE, 0);
  g_signal_connect (timeline, "new-frame", G_CALLBACK (frame_cb), label);
  clutter_timeline_start (timeline);

  clutter_main();

  g_object_unref (stage);

  return 0;
}
