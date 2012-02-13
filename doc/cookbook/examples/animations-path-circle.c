#include <stdlib.h>
#include <clutter/clutter.h>

#define STAGE_SIDE 400.0

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };

/* Build a "circular" path out of 4 Bezier curves
 *
 * code modified from
 * http://git.clutter-project.org/dax/tree/dax/dax-traverser-clutter.c#n328
 *
 * see http://www.whizkidtech.redprince.net/bezier/circle/
 * for further explanation
 */
static ClutterPath *
build_circular_path (gfloat cx,
                     gfloat cy,
                     gfloat r)
{
  ClutterPath *path;
  static gfloat kappa = 4 * (G_SQRT2 - 1) / 3;

  path = clutter_path_new ();

  clutter_path_add_move_to (path, cx + r, cy);
  clutter_path_add_curve_to (path,
                             cx + r, cy + r * kappa,
                             cx + r * kappa, cy + r,
                             cx, cy + r);
  clutter_path_add_curve_to (path,
                             cx - r * kappa, cy + r,
                             cx - r, cy + r * kappa,
                             cx - r, cy);
  clutter_path_add_curve_to (path,
                             cx - r, cy - r * kappa,
                             cx - r * kappa, cy - r,
                             cx, cy - r);
  clutter_path_add_curve_to (path,
                             cx + r * kappa, cy - r,
                             cx + r, cy - r * kappa,
                             cx + r, cy);
  clutter_path_add_close (path);

  return path;
}

static gboolean
key_pressed_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer      user_data)
{
  ClutterTimeline *timeline = CLUTTER_TIMELINE (user_data);

  if (!clutter_timeline_is_playing (timeline))
    clutter_timeline_start (timeline);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterPath *path;
  ClutterConstraint *constraint;
  ClutterAnimator *animator;
  ClutterTimeline *timeline;
  ClutterActor *stage;
  ClutterActor *rectangle;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, STAGE_SIDE, STAGE_SIDE);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  rectangle = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (rectangle, STAGE_SIDE / 8, STAGE_SIDE / 8);
  clutter_actor_set_position (rectangle,
                              STAGE_SIDE / 2,
                              STAGE_SIDE / 2);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               rectangle);

  /* set up a path and make a constraint with it */
  path = build_circular_path (STAGE_SIDE / 2,
                              STAGE_SIDE / 2,
                              STAGE_SIDE / 4);
  constraint = clutter_path_constraint_new (path, 0.0);

  /* apply the constraint to the rectangle; note that there
   * is no need to name the constraint, as we will be animating
   * the constraint's offset property directly using ClutterAnimator
   */
  clutter_actor_add_constraint (rectangle, constraint);

  /* animation to animate the path offset */
  animator = clutter_animator_new ();
  clutter_animator_set_duration (animator, 5000);

  /* use ClutterAnimator to animate the constraint directly */
  clutter_animator_set (animator,
                        constraint, "offset", CLUTTER_LINEAR, 0.0, 0.0,
                        constraint, "offset", CLUTTER_LINEAR, 1.0, 1.0,
                        NULL);

  timeline = clutter_animator_get_timeline (animator);
  clutter_timeline_set_repeat_count (timeline, -1);
  clutter_timeline_set_auto_reverse (timeline, TRUE);

  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (key_pressed_cb),
                    timeline);

  clutter_actor_show (stage);

  clutter_main ();

  /* clean up */
  g_object_unref (animator);

  return EXIT_SUCCESS;
}
