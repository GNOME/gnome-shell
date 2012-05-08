#include <stdlib.h>
#include <clutter/clutter.h>

#define RECT_SIZE       128

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
static const gchar *names[N_RECTS] = {
  "North West", "North",  "North East",
  "West",       "Center", "East",
  "South West", "South",  "South East"
};

static const gchar *desaturare_glsl_shader =
"uniform sampler2D tex;\n"
"uniform float factor;\n"
"\n"
"vec3 desaturate (const vec3 color, const float desaturation)\n"
"{\n"
"  const vec3 gray_conv = vec3 (0.299, 0.587, 0.114);\n"
"  vec3 gray = vec3 (dot (gray_conv, color));\n"
"  return vec3 (mix (color.rgb, gray, desaturation));\n"
"}\n"
"\n"
"void main ()\n"
"{\n"
"  vec4 color = cogl_color_in * texture2D (tex, vec2 (cogl_tex_coord_in[0].xy));\n"
"  color.rgb = desaturate (color.rgb, factor);\n"
"  cogl_color_out = color;\n"
"}\n";

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

      /* expand the 8 rectangles by animating the offset of the
       * bind constraints
       */

      north_offset = (clutter_actor_get_height (rects[Center]) + V_PADDING)
                   * -1.0f;
      south_offset = (clutter_actor_get_height (rects[Center]) + V_PADDING);

      west_offset = (clutter_actor_get_width (rects[Center]) + H_PADDING)
                  * -1.0f;
      east_offset = (clutter_actor_get_width (rects[Center]) + H_PADDING);

      clutter_actor_animate (rects[NorthWest], CLUTTER_EASE_OUT_EXPO, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", west_offset,
                             "@constraints.y-bind.offset", north_offset,
                             "reactive", TRUE,
                             NULL);
      clutter_actor_animate (rects[North], CLUTTER_EASE_OUT_EXPO, 500,
                             "opacity", 255,
                             "@constraints.y-bind.offset", north_offset,
                             "reactive", TRUE,
                             NULL);
      clutter_actor_animate (rects[NorthEast], CLUTTER_EASE_OUT_EXPO, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", east_offset,
                             "@constraints.y-bind.offset", north_offset,
                             "reactive", TRUE,
                             NULL);

      clutter_actor_animate (rects[West], CLUTTER_EASE_OUT_EXPO, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", west_offset,
                             "reactive", TRUE,
                             NULL);
      /* turn on the desaturation effect and set the center
       * rectangle not reactive
       */
      clutter_actor_animate (rects[Center], CLUTTER_LINEAR, 500,
                             "@effects.desaturate.enabled", TRUE,
                             "reactive", FALSE,
                             NULL);
      clutter_actor_animate (rects[East], CLUTTER_EASE_OUT_EXPO, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", east_offset,
                             "reactive", TRUE,
                             NULL);

      clutter_actor_animate (rects[SouthWest], CLUTTER_EASE_OUT_EXPO, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", west_offset,
                             "@constraints.y-bind.offset", south_offset,
                             "reactive", TRUE,
                             NULL);
      clutter_actor_animate (rects[South], CLUTTER_EASE_OUT_EXPO, 500,
                             "opacity", 255,
                             "@constraints.y-bind.offset", south_offset,
                             "reactive", TRUE,
                             NULL);
      clutter_actor_animate (rects[SouthEast], CLUTTER_EASE_OUT_EXPO, 500,
                             "opacity", 255,
                             "@constraints.x-bind.offset", east_offset,
                             "@constraints.y-bind.offset", south_offset,
                             "reactive", TRUE,
                             NULL);
    }
  else
    {
      gint i;

      clutter_actor_animate (rects[Center], CLUTTER_LINEAR, 500,
                             "@effects.desaturate.enabled", FALSE,
                             "reactive", TRUE,
                             NULL);

      for (i = NorthWest; i < N_RECTS; i++)
        {
          if (i == Center)
            continue;

          /* put the 8 rectangles back into their initial state */
          clutter_actor_animate (rects[i], CLUTTER_EASE_OUT_EXPO, 500,
                                 "opacity", 0,
                                 "@constraints.x-bind.offset", 0.0f,
                                 "@constraints.y-bind.offset", 0.0f,
                                 "reactive", FALSE,
                                 NULL);
        }
    }

  is_expanded = !is_expanded;

  g_print ("Selected: [%s]\n", clutter_actor_get_name (actor));

  return TRUE;
}

G_MODULE_EXPORT const char *
test_bind_constraint_describe (void)
{
  return "Demonstrate the usage of ClutterBindConstraint";
}

G_MODULE_EXPORT int
test_bind_constraint_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect;
  ClutterConstraint *constraint;
  ClutterEffect *effect;
  ClutterColor rect_color;
  gint i;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Constraints");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, 800, 600);

  /* main rectangle */
  clutter_color_from_string (&rect_color, "#3465a4");
  rect = clutter_actor_new ();
  g_signal_connect (rect, "button-release-event",
                    G_CALLBACK (on_button_release),
                    NULL);
  clutter_actor_set_background_color (rect, &rect_color);
  clutter_actor_set_size (rect, RECT_SIZE, RECT_SIZE);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_actor_set_name (rect, names[Center]);
  clutter_actor_add_child (stage, rect);

  /* align the center rectangle to the center of the stage */
  constraint = clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5);
  clutter_actor_add_constraint_with_name (rect, "align", constraint);

  /* this is the equivalent of the DesaturateEffect; we cannot animate
   * the factor because the animation API only understands GObject
   * properties; so we use the ActorMeta:enabled property to toggle
   * the shader
   */
  effect = clutter_shader_effect_new (CLUTTER_FRAGMENT_SHADER);
  clutter_shader_effect_set_shader_source (CLUTTER_SHADER_EFFECT (effect),
                                           desaturare_glsl_shader);
  clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect),
                                     "tex", G_TYPE_INT, 1, 0);
  clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect),
                                     "factor", G_TYPE_FLOAT, 1, 0.66);
  clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (effect), FALSE);
  clutter_actor_add_effect_with_name (rect, "desaturate", effect);

  rects[Center] = rect;

  /* build the other rectangles, and bind their position and size
   * to the center rectangle. we are going to animate the offset
   * of the BindConstraints
   */
  for (i = 0; i < N_RECTS; i++)
    {
      if (i == Center)
        continue;

      clutter_color_from_string (&rect_color, colors[i]);

      rect = clutter_actor_new ();
      clutter_actor_set_background_color (rect, &rect_color);
      clutter_actor_set_opacity (rect, 0);
      clutter_actor_set_name (rect, names[i]);
      clutter_actor_add_child (stage, rect);

      constraint = clutter_bind_constraint_new (rects[Center], CLUTTER_BIND_X, 0.0);
      clutter_actor_add_constraint_with_name (rect, "x-bind", constraint);

      constraint = clutter_bind_constraint_new (rects[Center], CLUTTER_BIND_Y, 0.0);
      clutter_actor_add_constraint_with_name (rect, "y-bind", constraint);

      constraint = clutter_bind_constraint_new (rects[Center], CLUTTER_BIND_SIZE, 0.0);
      clutter_actor_add_constraint_with_name (rect, "size-bind", constraint);

      g_signal_connect (rect, "button-release-event",
                        G_CALLBACK (on_button_release),
                        NULL);

      rects[i] = rect;
    }

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
