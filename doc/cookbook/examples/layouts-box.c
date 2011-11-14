#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor box_color = { 0xff, 0xff, 0xff, 0xff };
static const ClutterColor yellow_color = { 0xaa, 0xaa, 0x00, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor blue_color = { 0x00, 0x00, 0xff, 0xff };

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterLayoutManager *box_layout;
  ClutterActor *box;
  ClutterActor *yellow;
  ClutterActor *red;
  ClutterActor *blue;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* create a ClutterBoxLayout */
  box_layout = clutter_box_layout_new ();

  /* configure it to lay out actors vertically */
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (box_layout), TRUE);

  /* put 5px of spacing between actors */
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (box_layout), 5);

  /* actors are packed into this box; we set its width, but
   * allow its height to be determined by the children it contains
   */
  box = clutter_box_new (box_layout);
  clutter_box_set_color (CLUTTER_BOX (box), &box_color);
  clutter_actor_set_position (box, 100, 50);
  clutter_actor_set_width (box, 200);

  /* pack an actor into the layout and set all layout properties on it
   * at the same time
   */
  yellow = clutter_rectangle_new_with_color (&yellow_color);
  clutter_actor_set_size (yellow, 100, 100);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (box_layout),
                           yellow,
                           FALSE,                         /* expand */
                           TRUE,                          /* x-fill */
                           FALSE,                         /* y-fill */
                           CLUTTER_BOX_ALIGNMENT_START,   /* x-align */
                           CLUTTER_BOX_ALIGNMENT_START);  /* y-align */

  /* pack an actor into the box and set layout properties at the
   * same time; note this is more concise if you mostly want to
   * use the default properties for the layout
   */
  red = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (red, 100, 100);

  clutter_box_pack (CLUTTER_BOX (box),
                    red,
                    "x-fill", TRUE,
                    NULL);

  /* add an actor to the box as a container and set layout properties
   * afterwards; the latter is useful if you want to change properties on
   * actors already inside a layout, but note that you have to
   * pass the function both the layout AND the container
   */
  blue = clutter_rectangle_new_with_color (&blue_color);
  clutter_actor_set_size (blue, 100, 100);

  clutter_container_add_actor (CLUTTER_CONTAINER (box), blue);

  clutter_layout_manager_child_set (box_layout,
                                    CLUTTER_CONTAINER (box),
                                    blue,
                                    "x-fill", TRUE,
                                    NULL);

  /* put the box on the stage */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
