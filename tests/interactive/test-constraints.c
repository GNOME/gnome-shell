#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

#define H_PADDING       32

static ClutterActor *center_rect = NULL;
static ClutterActor *left_rect   = NULL;
static ClutterActor *right_rect  = NULL;

static gboolean      is_expanded = FALSE;

static gboolean
on_button_release (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      data G_GNUC_UNUSED)
{
  if (!is_expanded)
    {
      gfloat left_offset = (clutter_actor_get_width (left_rect) + H_PADDING)
                         * -1.0f;
      gfloat right_offset = clutter_actor_get_width (right_rect) + H_PADDING;

      clutter_actor_animate (left_rect, CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", left_offset,
                             NULL);
      clutter_actor_animate (right_rect, CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", right_offset,
                             NULL);
    }
  else
    {
      clutter_actor_animate (left_rect, CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 0,
                             "@constraints.x-bind.offset", 0.0f,
                             NULL);
      clutter_actor_animate (right_rect, CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 0,
                             "@constraints.x-bind.offset", 0.0f,
                             NULL);
    }

  is_expanded = !is_expanded;

  return TRUE;
}

G_MODULE_EXPORT int
test_constraints_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect;
  ClutterColor rect_color;
  ClutterConstraint *constraint;

  clutter_init (&argc, &argv);

  stage = clutter_stage_new ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Constraints");
  clutter_actor_set_size (stage, 800, 600);

  /* main rect */
  clutter_color_from_string (&rect_color, "#73d216ff");
  rect = clutter_rectangle_new ();
  g_signal_connect (rect, "button-release-event",
                    G_CALLBACK (on_button_release),
                    NULL);
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &rect_color);
  clutter_actor_set_size (rect, 256, 256);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  constraint = clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "x-align");
  clutter_actor_add_constraint (rect, constraint);

  constraint = clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "y-align");
  clutter_actor_add_constraint (rect, constraint);

  center_rect = rect;

  /* left rectangle */
  clutter_color_from_string (&rect_color, "#cc0000ff");
  rect = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &rect_color);
  clutter_actor_set_size (rect, 256, 256);
  clutter_actor_set_opacity (rect, 0);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  constraint = clutter_bind_constraint_new (center_rect, CLUTTER_BIND_X_AXIS, 0.0f);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "x-bind");
  clutter_actor_add_constraint (rect, constraint);

  constraint = clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "y-align");
  clutter_actor_add_constraint (rect, constraint);

  left_rect = rect;

  /* right rectangle */
  clutter_color_from_string (&rect_color, "#3465a4ff");
  rect = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &rect_color);
  clutter_actor_set_size (rect, 256, 256);
  clutter_actor_set_opacity (rect, 0);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  constraint = clutter_bind_constraint_new (center_rect, CLUTTER_BIND_X_AXIS, 0.0f);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "x-bind");
  clutter_actor_add_constraint (rect, constraint);

  constraint = clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "y-align");
  clutter_actor_add_constraint (rect, constraint);

  right_rect = rect;

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
