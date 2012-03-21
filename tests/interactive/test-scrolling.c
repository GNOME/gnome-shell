#include <stdlib.h>
#include <math.h>
#include <gmodule.h>
#include <clutter/clutter.h>

#define RECT_WIDTH      400
#define RECT_HEIGHT     300
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
             ClutterModifierType  modifiers)
{
  gfloat viewport_x = clutter_actor_get_x (viewport);
  gfloat offset_x;
  gint child_visible;

  /* check if we're at the viewport edges */
  if (viewport_x > 0)
    {
      clutter_actor_save_easing_state (viewport);
      clutter_actor_set_easing_mode (viewport, CLUTTER_EASE_OUT_BOUNCE);
      clutter_actor_set_x (viewport, 0);
      clutter_actor_restore_easing_state (viewport);
      return;
    }

  if (viewport_x < (-1.0f * (RECT_WIDTH * (N_RECTS - 1))))
    {
      clutter_actor_save_easing_state (viewport);
      clutter_actor_set_easing_mode (viewport, CLUTTER_EASE_OUT_BOUNCE);
      clutter_actor_set_x (viewport, -1.0f * (RECT_WIDTH * (N_RECTS - 1)));
      clutter_actor_restore_easing_state (viewport);
      return;
    }

  /* animate the viewport to fully show the child once we pass
   * a certain threshold with the dragging action
   */
  offset_x = fabsf (viewport_x) / RECT_WIDTH + 0.5f;
  if (offset_x > (RECT_WIDTH * 0.33))
    child_visible = (int) offset_x + 1;
  else
    child_visible = (int) offset_x;

  /* sanity check on the children number */
  child_visible = CLAMP (child_visible, 0, N_RECTS);

  clutter_actor_save_easing_state (viewport);
  clutter_actor_set_x (viewport, -1.0f * RECT_WIDTH * child_visible);
  clutter_actor_restore_easing_state (viewport);
}

G_MODULE_EXPORT int
test_scrolling_main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *scroll;
  ClutterAction *action;
  gint i;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Scrolling");
  clutter_actor_set_size (stage, 800, 600);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* scroll: the group that contains the scrolling viewport; we set its
   * size to be the same as one rectangle, position it in the middle of
   * the stage and set it to clip its contents to the allocated size
   */
  scroll = clutter_actor_new ();
  clutter_actor_add_child (stage, scroll);
  clutter_actor_set_size (scroll, RECT_WIDTH, RECT_HEIGHT);
  clutter_actor_add_constraint (scroll, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_set_clip_to_allocation (scroll, TRUE);

  /* viewport: the actual container for the children; we scroll it using
   * the Drag action constrained to the horizontal axis, and every time
   * the dragging ends we check whether we're dragging past the end of
   * the viewport
   */
  viewport = clutter_actor_new ();
  clutter_actor_set_layout_manager (viewport, clutter_box_layout_new ());
  clutter_actor_add_child (scroll, viewport);

  /* add dragging capabilities to the viewport; the heavy lifting is
   * all done by the DragAction itself, plus the ::drag-end signal
   * handler in our code
   */
  action = clutter_drag_action_new ();
  clutter_actor_add_action (viewport, action);
  clutter_drag_action_set_drag_axis (CLUTTER_DRAG_ACTION (action),
                                     CLUTTER_DRAG_X_AXIS);
  g_signal_connect (action, "drag-end", G_CALLBACK (on_drag_end), NULL);
  clutter_actor_set_reactive (viewport, TRUE);

  /* children of the viewport */
  for (i = 0; i < N_RECTS; i++)
    {
      ClutterColor color;

      clutter_color_from_string (&color, rect_color[i]);

      rectangle[i] = clutter_actor_new ();
      clutter_actor_set_background_color (rectangle[i], &color);
      clutter_actor_add_child (viewport, rectangle[i]);
      clutter_actor_set_size (rectangle[i], RECT_WIDTH, RECT_HEIGHT);
    }

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
