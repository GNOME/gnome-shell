#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

#define RECT_WIDTH      300
#define RECT_HEIGHT     200
#define N_RECTS         7

static const gchar *rect_color[N_RECTS] = {
  "#edd400",
  "#f57900",
  "#c17d11",
  "#73d216",
  "#3465a4",
  "#75507b",
  "#cc0000"
};

static ClutterActor *rectangle[N_RECTS]; 
static ClutterActor *viewport = NULL;

static void
on_drag_end (ClutterDragAction   *action,
             ClutterActor        *actor,
             gfloat               event_x,
             gfloat               event_y,
             guint                button,
             ClutterModifierType  modifiers)
{
  gfloat viewport_x = clutter_actor_get_x (viewport);

  if (viewport_x > 0)
    {
      clutter_actor_animate (viewport, CLUTTER_EASE_OUT_QUAD, 250,
                             "x", 0.0,
                             NULL);
      return;
    }

  if (viewport_x < (-1.0f * (RECT_WIDTH * (N_RECTS - 1))))
    {
      clutter_actor_animate (viewport, CLUTTER_EASE_OUT_QUAD, 250,
                             "x", (-1.0f * (RECT_WIDTH * (N_RECTS - 1))),
                             NULL);
      return;
    }
}

G_MODULE_EXPORT int
test_scrolling_main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *scroll;
  ClutterAction *action;
  gint i;

  clutter_init (&argc, &argv);

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Scrolling");
  clutter_actor_set_size (stage, 800, 600);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* scroll: the group that contains the scrolling viewport; we set its
   * size to be the same as one rectangle, position it in the middle of
   * the stage and sets it to clip its contents to the allocated size
   */
  scroll = clutter_group_new ();
  clutter_actor_set_size (scroll, RECT_WIDTH, RECT_HEIGHT);
  clutter_actor_add_constraint (scroll, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (scroll, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));
  g_object_set (scroll, "clip-to-allocation", TRUE, NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), scroll);

  /* viewport: the actual container for the children; we scroll it using
   * the Drag action constrained to the horizontal axis, and every time
   * the dragging ends we check whether we're dragging past the end of
   * the viewport
   */
  viewport = clutter_box_new (clutter_box_layout_new ());
  clutter_container_add_actor (CLUTTER_CONTAINER (scroll), viewport);

  action = clutter_drag_action_new ();
  clutter_drag_action_set_drag_axis (CLUTTER_DRAG_ACTION (action),
                                     CLUTTER_DRAG_X_AXIS);
  g_signal_connect (action, "drag-end", G_CALLBACK (on_drag_end), NULL);
  clutter_actor_add_action (viewport, action);
  clutter_actor_set_reactive (viewport, TRUE);

  /* children of the viewport */
  for (i = 0; i < N_RECTS; i++)
    {
      ClutterColor color;

      clutter_color_from_string (&color, rect_color[i]);

      rectangle[i] = clutter_rectangle_new_with_color (&color);
      clutter_container_add_actor (CLUTTER_CONTAINER (viewport), rectangle[i]);
      clutter_actor_set_size (rectangle[i], RECT_WIDTH, RECT_HEIGHT);
    }

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
