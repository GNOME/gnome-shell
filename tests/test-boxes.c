#include <stdlib.h>
#include <clutter/clutter.h>

static void
on_button_press_cb (ClutterStage *stage,
                    ClutterEvent *event,
                    gpointer      data)
{
  ClutterActor *rect;
  gint x, y;

  clutter_event_get_coords (event, &x, &y);
  rect = clutter_stage_get_actor_at_pos (stage, x, y);
  if (!rect)
    return;

  if (!CLUTTER_IS_RECTANGLE (rect))
    {
      g_print ("[!] No rectangle selected (%s selected instead)\n",
               g_type_name (G_OBJECT_TYPE (rect)));
      return;
    }

  g_print ("[*] Picked rectangle at (%d, %d)\n", x, y);
}

static void
on_key_press_cb (ClutterStage *stage,
                 ClutterEvent *event,
                 gpointer      data)
{
  ClutterKeyEvent *key_event = (ClutterKeyEvent *) event;

  if (clutter_key_event_symbol (key_event) == CLUTTER_Escape)
    clutter_main_quit ();
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *vbox;
  ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor label_color = { 0xff, 0xff, 0xff, 0x99 };
  gint i, j;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 800, 600);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "button-press-event",
                    G_CALLBACK (on_button_press_cb),
                    NULL);
  g_signal_connect (stage, "key-press-event",
                    G_CALLBACK (on_key_press_cb),
                    NULL);

  vbox = clutter_vbox_new ();
  clutter_actor_set_position (vbox, 100, 100);
  clutter_box_set_spacing (CLUTTER_BOX (vbox), 10);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), vbox);
  clutter_actor_show (vbox);

  for (i = 0; i < 3; i++)
    {
      ClutterActor *hbox;

      hbox = clutter_hbox_new ();
      clutter_box_set_spacing (CLUTTER_BOX (hbox), 10);
      clutter_container_add_actor (CLUTTER_CONTAINER (vbox), hbox);
      clutter_actor_show (hbox);

      for (j = 0; j < 3; j++)
        {
          ClutterActor *rect;

          rect = clutter_rectangle_new_with_color (&label_color);
          clutter_actor_set_size (rect, 100, 100);

          clutter_box_pack_defaults (CLUTTER_BOX (hbox), rect);
          clutter_actor_show (rect);

          g_debug (G_STRLOC ": rect[%d][%d] - (x:%d, y:%d, w:%d, h:%d)",
                   i, j,
                   clutter_actor_get_x (rect),
                   clutter_actor_get_y (rect),
                   clutter_actor_get_width (rect),
                   clutter_actor_get_height (rect));
        }
      
      g_debug (G_STRLOC ": hbox[%d] (x:%d, y%d, w:%d, h:%d)",
               i,
               clutter_actor_get_x (hbox),
               clutter_actor_get_y (hbox),
               clutter_actor_get_width (hbox),
               clutter_actor_get_height (hbox));

    }

  clutter_actor_show_all (stage);

  g_debug (G_STRLOC ": vbox (x:%d, y%d, w:%d, h:%d)",
           clutter_actor_get_x (vbox),
           clutter_actor_get_y (vbox),
           clutter_actor_get_width (vbox),
           clutter_actor_get_height (vbox));

  clutter_main ();

  return EXIT_SUCCESS;
}
