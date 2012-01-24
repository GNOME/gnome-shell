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

  /* actors are packed into this actor; we set its width, but
   * allow its height to be determined by the children it contains
   */
  box = clutter_actor_new ();
  clutter_actor_set_layout_manager (box, box_layout);
  clutter_actor_set_background_color (box, CLUTTER_COLOR_White);
  clutter_actor_set_position (box, 100, 50);
  clutter_actor_set_width (box, 200);

  /* pack an actor into the layout and set all layout properties on it
   * at the same time
   */
  yellow = clutter_actor_new ();
  clutter_actor_set_background_color (yellow, CLUTTER_COLOR_Yellow);
  clutter_actor_set_size (yellow, 100, 100);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (box_layout),
                           yellow,
                           FALSE,                         /* expand */
                           TRUE,                          /* x-fill */
                           FALSE,                         /* y-fill */
                           CLUTTER_BOX_ALIGNMENT_START,   /* x-align */
                           CLUTTER_BOX_ALIGNMENT_START);  /* y-align */

  /* add an actor to the box as a container and set layout properties
   * afterwards; the latter is useful if you want to change properties on
   * actors already inside a layout, but note that you have to
   * pass the function both the layout AND the container
   */
  red = clutter_actor_new ();
  clutter_actor_set_background_color (red, CLUTTER_COLOR_Red);
  clutter_actor_set_size (red, 100, 100);

  clutter_actor_add_child (box, red);
  clutter_layout_manager_child_set (box_layout,
                                    CLUTTER_CONTAINER (box),
                                    red,
                                    "x-fill", TRUE,
                                    NULL);

  blue = clutter_actor_new ();
  clutter_actor_set_background_color (blue, CLUTTER_COLOR_Blue);
  clutter_actor_set_size (blue, 100, 100);

  clutter_actor_add_child (box, blue);
  clutter_layout_manager_child_set (box_layout,
                                    CLUTTER_CONTAINER (box),
                                    blue,
                                    "x-fill", TRUE,
                                    NULL);

  /* put the box on the stage */
  clutter_actor_add_child (stage, box);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
