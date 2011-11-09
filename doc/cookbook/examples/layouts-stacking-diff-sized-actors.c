#include <clutter/clutter.h>

static const ClutterColor dark_grey = { 0x66, 0x66, 0x66, 0xff };
static const ClutterColor light_grey = { 0xcc, 0xcc, 0xcc, 0xff };

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterLayoutManager *layout;
  ClutterActor *box;
  ClutterActor *rect1, *rect2;
  guint align_x, align_y, diff_x, diff_y;
  ClutterColor *color;
  ClutterActor *rect;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_START,
                                   CLUTTER_BIN_ALIGNMENT_START);

  box = clutter_box_new (layout);

  rect1 = clutter_rectangle_new_with_color (&dark_grey);
  clutter_actor_set_size (rect1, 400, 200);

  rect2 = clutter_rectangle_new_with_color (&light_grey);
  clutter_actor_set_size (rect2, 200, 400);

  clutter_container_add (CLUTTER_CONTAINER (box),
                         rect1,
                         rect2,
                         NULL);

  /*
   * 2 = CLUTTER_BIN_ALIGNMENT_START
   * 3 = CLUTTER_BIN_ALIGNMENT_END
   * 4 = CLUTTER_BIN_ALIGNMENT_CENTER
   */
  for (align_x = 2; align_x < 5; align_x++)
    {
      for (align_y = 2; align_y < 5; align_y++)
        {
          diff_x = align_x - 1;
          if (align_x == 3)
            diff_x = 3;
          else if (align_x == 4)
            diff_x = 2;

          diff_y = align_y - 1;
          if (align_y == 3)
            diff_y = 3;
          else if (align_y == 4)
            diff_y = 2;

          color = clutter_color_new (255 - diff_x * 50,
                                                   100 + diff_y * 50,
                                                   0,
                                                   255);
          rect = clutter_rectangle_new_with_color (color);
          clutter_actor_set_size (rect, 100, 100);
          clutter_bin_layout_set_alignment (CLUTTER_BIN_LAYOUT (layout),
                                            rect,
                                            align_x,
                                            align_y);
          clutter_container_add_actor (CLUTTER_CONTAINER (box), rect);
        }
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
