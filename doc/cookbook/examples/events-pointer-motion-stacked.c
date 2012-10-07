/*
 * Testing what happens with a stack of actors and pointer events
 * red and green are reactive; blue is not
 *
 * when the pointer is over green (even if green is obscured by blue)
 * signals are emitted by green (not by blue);
 *
 * but when the pointer is over the overlap between red and green,
 * signals are emitted by green
 */
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor green = { 0x00, 0xff, 0x00, 0xff };
static const ClutterColor blue = { 0x00, 0x00, 0xff, 0xff };

static gboolean
_pointer_motion_cb (ClutterActor *actor,
                    ClutterEvent *event,
                    gpointer      user_data)
{
  gfloat stage_x, stage_y;
  gfloat actor_x, actor_y;

  /* get the coordinates where the pointer crossed into the actor */
  clutter_event_get_coords (event, &stage_x, &stage_y);

  /*
   * as the coordinates are relative to the stage, rather than
   * the actor which emitted the signal, it can be useful to
   * transform them to actor-relative coordinates
   */
  clutter_actor_transform_stage_point (actor,
                                       stage_x, stage_y,
                                       &actor_x, &actor_y);

  g_debug ("pointer on actor %s @ x %.0f, y %.0f",
           clutter_actor_get_name (actor),
           actor_x, actor_y);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *r1, *r2, *r3;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 300, 300);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  r1 = clutter_rectangle_new_with_color (&red);
  clutter_actor_set_size (r1, 150, 150);
  clutter_actor_add_constraint (r1, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.25));
  clutter_actor_add_constraint (r1, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.25));
  clutter_actor_set_reactive (r1, TRUE);
  clutter_actor_set_name (r1, "red");

  r2 = clutter_rectangle_new_with_color (&green);
  clutter_actor_set_size (r2, 150, 150);
  clutter_actor_add_constraint (r2, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (r2, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));
  clutter_actor_set_reactive (r2, TRUE);
  clutter_actor_set_depth (r2, -100);
  clutter_actor_set_name (r2, "green");

  r3 = clutter_rectangle_new_with_color (&blue);
  clutter_actor_set_size (r3, 150, 150);
  clutter_actor_add_constraint (r3, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.75));
  clutter_actor_add_constraint (r3, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.75));
  clutter_actor_set_opacity (r3, 125);
  clutter_actor_set_name (r3, "blue");

  clutter_container_add (CLUTTER_CONTAINER (stage), r1, r2, r3, NULL);

  g_signal_connect (r1, "motion-event", G_CALLBACK (_pointer_motion_cb), NULL);
  g_signal_connect (r2, "motion-event", G_CALLBACK (_pointer_motion_cb), NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
