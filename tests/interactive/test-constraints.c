#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

#define H_PADDING       32
#define V_PADDING       32

enum
{
  NorthWest,    North,  NorthEast,
  West,         Center, East,
  SouthWest,    South,  SouthEast,

  N_RECTS
};

static ClutterActor *rects[N_RECTS] = { NULL, };
static const gchar *colors[N_RECTS] = {
  "#8ae234", "#73d216", "#4e9a06",
  "#729fcf", "#3465a4", "#204a87",
  "#ef2929", "#cc0000", "#a40000"
};

static gboolean      is_expanded = FALSE;

static gboolean
on_button_release (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      data G_GNUC_UNUSED)
{
  if (!is_expanded)
    {
      gfloat north_offset, south_offset;
      gfloat west_offset, east_offset;

      north_offset = (clutter_actor_get_height (rects[Center]) + V_PADDING)
                   * -1.0f;
      south_offset = (clutter_actor_get_height (rects[Center]) + V_PADDING);

      west_offset = (clutter_actor_get_width (rects[Center]) + H_PADDING)
                  * -1.0f;
      east_offset = (clutter_actor_get_width (rects[Center]) + H_PADDING);

      clutter_actor_animate (rects[Center], CLUTTER_LINEAR, 500,
                             "opacity", 128,
                             NULL);

      clutter_actor_animate (rects[NorthWest], CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", west_offset,
                             "@constraints.y-bind.offset", north_offset,
                             NULL);
      clutter_actor_animate (rects[North], CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.y-bind.offset", north_offset,
                             NULL);
      clutter_actor_animate (rects[NorthEast], CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", east_offset,
                             "@constraints.y-bind.offset", north_offset,
                             NULL);

      clutter_actor_animate (rects[West], CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", west_offset,
                             NULL);
      clutter_actor_animate (rects[Center], CLUTTER_LINEAR, 500,
                             "opacity", 128,
                             NULL);
      clutter_actor_animate (rects[East], CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", east_offset,
                             NULL);

      clutter_actor_animate (rects[SouthWest], CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", west_offset,
                             "@constraints.y-bind.offset", south_offset,
                             NULL);
      clutter_actor_animate (rects[South], CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.y-bind.offset", south_offset,
                             NULL);
      clutter_actor_animate (rects[SouthEast], CLUTTER_EASE_OUT_CUBIC, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", east_offset,
                             "@constraints.y-bind.offset", south_offset,
                             NULL);
    }
  else
    {
      gint i;

      clutter_actor_animate (rects[Center], CLUTTER_LINEAR, 500,
                             "opacity", 255,
                             NULL);

      for (i = NorthWest; i < N_RECTS; i++)
        {
          if (i == Center)
            continue;

          clutter_actor_animate (rects[i], CLUTTER_EASE_OUT_CUBIC, 500,
                                 "opacity", 0,
                                 "@constraints.x-bind.offset", 0.0f,
                                 "@constraints.y-bind.offset", 0.0f,
                                 NULL);
        }
    }

  is_expanded = !is_expanded;

  return TRUE;
}

G_MODULE_EXPORT int
test_constraints_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect;
  ClutterConstraint *constraint;
  ClutterColor rect_color;
  gint i;

  clutter_init (&argc, &argv);

  stage = clutter_stage_new ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Constraints");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, 800, 600);

  /* main rect */
  clutter_color_from_string (&rect_color, "#3465a4");
  rect = clutter_rectangle_new ();
  g_signal_connect (rect, "button-release-event",
                    G_CALLBACK (on_button_release),
                    NULL);
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &rect_color);
  clutter_actor_set_size (rect, 128, 128);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  constraint = clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "x-align");
  clutter_actor_add_constraint (rect, constraint);

  constraint = clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "y-align");
  clutter_actor_add_constraint (rect, constraint);

  rects[Center] = rect;

  for (i = 0; i < N_RECTS; i++)
    {
      if (i == Center)
        continue;

      clutter_color_from_string (&rect_color, colors[i]);
      rect = clutter_rectangle_new ();
      clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &rect_color);
      clutter_actor_set_size (rect, 128, 128);
      clutter_actor_set_opacity (rect, 0);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

      constraint = clutter_bind_constraint_new (rects[Center], CLUTTER_BIND_X, 0.0);
      clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "x-bind");
      clutter_actor_add_constraint (rect, constraint);

      constraint = clutter_bind_constraint_new (rects[Center], CLUTTER_BIND_Y, 0.0);
      clutter_actor_meta_set_name (CLUTTER_ACTOR_META (constraint), "y-bind");
      clutter_actor_add_constraint (rect, constraint);

      rects[i] = rect;
    }

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
