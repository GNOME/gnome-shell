#include <clutter/clutter.h>

guint8 opacity = 255;

gboolean 
timeout_cb (gpointer data)
{
  ClutterElement *element;

  element = CLUTTER_ELEMENT(data);

  if (opacity > 0)
    {
      clutter_element_set_opacity (element, opacity);
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
      clutter_element_set_opacity (CLUTTER_ELEMENT(label), opacity);
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
  clutter_element_set_opacity (CLUTTER_ELEMENT(label), opacity); 

  clutter_element_rotate_z (CLUTTER_ELEMENT(label),
			    frame_num,
			    clutter_element_get_width (CLUTTER_ELEMENT(label))/2,
			    clutter_element_get_height (CLUTTER_ELEMENT(label))/2);
}

int
main (int argc, char *argv[])
{
  ClutterElement *texture, *label;
  ClutterTimeline *timeline;
  GdkPixbuf      *pixbuf;

  clutter_init (&argc, &argv);

  pixbuf = gdk_pixbuf_new_from_file ("clutter-logo-800x600.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  texture = clutter_texture_new_from_pixbuf (pixbuf);

  printf("***********foo***********\n");

  label = clutter_label_new_with_text("Sans Bold 72", "Clutter\nOpened\nHand");

  printf("***********foo***********\n");

  clutter_element_set_opacity (CLUTTER_ELEMENT(label), 0x99);
  clutter_element_set_position (CLUTTER_ELEMENT(label), 100, 200);

  clutter_group_add(clutter_stage(), texture);
  clutter_group_add(clutter_stage(), label);

  clutter_element_set_size (CLUTTER_ELEMENT(clutter_stage()), 800, 600);

  clutter_group_show_all(clutter_stage());

  timeline = clutter_timeline_new (360, 200);
  g_object_set(timeline, "loop", TRUE, 0);
  g_signal_connect(timeline, "new-frame", frame_cb, label);
  clutter_timeline_start (timeline);

  clutter_main();

  return 0;
}
