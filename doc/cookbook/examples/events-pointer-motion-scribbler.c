/*
 * Simple scribble application: move mouse over the dark yellow
 * rectangle to draw brighter yellow lines
 */

#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor actor_color = { 0xaa, 0x99, 0x00, 0xff };

typedef struct {
  ClutterPath *path;
  CoglPath    *cogl_path;
} Context;

static void
_convert_clutter_path_node_to_cogl_path (const ClutterPathNode *node,
                                         gpointer               data)
{
  ClutterKnot knot;

  g_return_if_fail (node != NULL);

  switch (node->type)
    {
    case CLUTTER_PATH_MOVE_TO:
      knot = node->points[0];
      cogl_path_move_to (knot.x, knot.y);
      g_debug ("move to %d, %d", knot.x, knot.y);
      break;
    case CLUTTER_PATH_LINE_TO:
      knot = node->points[0];
      cogl_path_line_to (knot.x, knot.y);
      g_debug ("line to %d, %d", knot.x, knot.y);
      break;
    default:
      break;
    }
}

static void
_canvas_paint_cb (ClutterActor *actor,
                  gpointer      user_data)
{
  Context *context = (Context *)user_data;

  cogl_set_source_color4ub (255, 255, 0, 255);

  cogl_set_path (context->cogl_path);

  clutter_path_foreach (context->path, _convert_clutter_path_node_to_cogl_path, NULL);

  cogl_path_stroke_preserve ();

  clutter_path_clear (context->path);

  context->cogl_path = cogl_get_path ();

  g_signal_stop_emission_by_name (actor, "paint");
}

static gboolean
_pointer_motion_cb (ClutterActor *actor,
                    ClutterEvent *event,
                    gpointer      user_data)
{
  ClutterMotionEvent *motion_event = (ClutterMotionEvent *)event;
  Context *context = (Context *)user_data;

  gfloat x, y;
  clutter_actor_transform_stage_point (actor, motion_event->x, motion_event->y, &x, &y);

  g_debug ("motion; x %f, y %f", x, y);

  clutter_path_add_line_to (context->path, x, y);

  clutter_actor_queue_redraw (actor);

  return TRUE;
}

static gboolean
_pointer_enter_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  ClutterCrossingEvent *cross_event = (ClutterCrossingEvent *)event;
  Context *context = (Context *)user_data;

  gfloat x, y;
  clutter_actor_transform_stage_point (actor, cross_event->x, cross_event->y, &x, &y);

  g_debug ("enter; x %f, y %f", x, y);

  clutter_path_add_move_to (context->path, x, y);

  clutter_actor_queue_redraw (actor);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  Context *context = g_new0 (Context, 1);

  ClutterActor *stage;
  ClutterActor *rect;
  ClutterActor *canvas;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  context->path = clutter_path_new ();

  cogl_path_new ();
  context->cogl_path = cogl_get_path ();

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  rect = clutter_rectangle_new_with_color (&actor_color);
  clutter_actor_set_size (rect, 300, 300);
  clutter_actor_add_constraint (rect, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (rect, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  canvas = clutter_texture_new ();
  clutter_actor_set_size (canvas, 300, 300);
  clutter_actor_add_constraint (canvas, clutter_align_constraint_new (rect, CLUTTER_ALIGN_X_AXIS, 0.0));
  clutter_actor_add_constraint (canvas, clutter_align_constraint_new (rect, CLUTTER_ALIGN_Y_AXIS, 0.0));
  clutter_actor_set_reactive (canvas, TRUE);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), canvas);
  clutter_actor_raise_top (canvas);

  g_signal_connect (canvas,
                    "motion-event",
                    G_CALLBACK (_pointer_motion_cb),
                    context);

  g_signal_connect (canvas,
                    "enter-event",
                    G_CALLBACK (_pointer_enter_cb),
                    context);

  g_signal_connect (canvas,
                    "paint",
                    G_CALLBACK (_canvas_paint_cb),
                    context);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (context->path);
  g_free (context);

  return 0;
}
