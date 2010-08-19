#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor rectangle_color = { 0xaa, 0x99, 0x00, 0xff };

static gboolean
_pointer_moved_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  ClutterMotionEvent *motion_event = (ClutterMotionEvent *)event;

  gfloat stage_x = motion_event->x;
  gfloat stage_y = motion_event->y;

  gfloat actor_x, actor_y;
  clutter_actor_transform_stage_point (actor,
                                       stage_x, stage_y,
                                       &actor_x, &actor_y);

  g_debug ("pointer @ stage x %.0f, y %.0f; actor x %.0f, y %.0f",
           stage_x, stage_y,
           actor_x, actor_y);
  return TRUE;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *rectangle;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  rectangle = clutter_rectangle_new_with_color (&rectangle_color);
  clutter_actor_set_size (rectangle, 300, 300);
  clutter_actor_set_position (rectangle, 50, 50);
  clutter_actor_set_reactive (rectangle, TRUE);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rectangle);

  g_signal_connect (rectangle,
                    "motion-event",
                    G_CALLBACK (_pointer_moved_cb),
                    NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
