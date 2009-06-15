#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

const struct {
  const gchar *name;
  ClutterAnimationMode mode;
} easing_modes[] = {
  { "linear", CLUTTER_LINEAR },
  { "easeInQuad", CLUTTER_EASE_IN_QUAD },
  { "easeOutQuad", CLUTTER_EASE_OUT_QUAD },
  { "easeInOutQuad", CLUTTER_EASE_IN_OUT_QUAD },
  { "easeInCubic", CLUTTER_EASE_IN_CUBIC },
  { "easeOutCubic", CLUTTER_EASE_OUT_CUBIC },
  { "easeInOutCubic", CLUTTER_EASE_IN_OUT_CUBIC },
  { "easeInQuart", CLUTTER_EASE_IN_QUART },
  { "easeOutQuart", CLUTTER_EASE_OUT_QUART },
  { "easeInOutQuart", CLUTTER_EASE_IN_OUT_QUART },
  { "easeInQuint", CLUTTER_EASE_IN_QUINT },
  { "easeOutQuint", CLUTTER_EASE_OUT_QUINT },
  { "easeInOutQuint", CLUTTER_EASE_IN_OUT_QUINT },
  { "easeInSine", CLUTTER_EASE_IN_SINE },
  { "easeOutSine", CLUTTER_EASE_OUT_SINE },
  { "easeInOutSine", CLUTTER_EASE_IN_OUT_SINE },
  { "easeInExpo", CLUTTER_EASE_IN_EXPO },
  { "easeOutExpo", CLUTTER_EASE_OUT_EXPO },
  { "easeInOutExpo", CLUTTER_EASE_IN_OUT_EXPO },
  { "easeInCirc", CLUTTER_EASE_IN_CIRC },
  { "easeOutCirc", CLUTTER_EASE_OUT_CIRC },
  { "easeInOutCirc", CLUTTER_EASE_IN_OUT_CIRC },
  { "easeInElastic", CLUTTER_EASE_IN_ELASTIC },
  { "easeOutElastic", CLUTTER_EASE_OUT_ELASTIC },
  { "easeInOutElastic", CLUTTER_EASE_IN_OUT_ELASTIC },
  { "easeInBack", CLUTTER_EASE_IN_BACK },
  { "easeOutBack", CLUTTER_EASE_OUT_BACK },
  { "easeInOutBack", CLUTTER_EASE_IN_OUT_BACK },
  { "easeInBounce", CLUTTER_EASE_IN_BOUNCE },
  { "easeOutBounce", CLUTTER_EASE_OUT_BOUNCE },
  { "easeInOutBounce", CLUTTER_EASE_IN_OUT_BOUNCE },
};

static const gint n_easing_modes = G_N_ELEMENTS (easing_modes);
static gint current_mode = 0;

static gint duration = 1;
static gboolean recenter = FALSE;

static ClutterActor *main_stage        = NULL;
static ClutterActor *easing_mode_label = NULL;

static void
on_animation_completed (ClutterAnimation *animation,
                        ClutterActor     *rectangle)
{
  gfloat base_x, base_y;
  gint cur_mode;

  base_x = clutter_actor_get_width (main_stage) / 2;
  base_y = clutter_actor_get_height (main_stage) / 2;

  cur_mode = easing_modes[current_mode].mode;

  clutter_actor_animate (rectangle, cur_mode, 150,
                         "x", base_x,
                         "y", base_y,
                         NULL);
}

static gboolean
on_button_press (ClutterActor       *actor,
                 ClutterButtonEvent *event,
                 ClutterActor       *rectangle)
{
  if (event->button == 3)
    {
      gchar *text;
      gfloat stage_width, stage_height;
      gfloat label_width, label_height;

      current_mode = (current_mode + 1 < n_easing_modes) ? current_mode + 1
                                                         : 0;

      text = g_strdup_printf ("Easing mode: %s (%d of %d)\n"
                              "Right click to change the easing mode",
                              easing_modes[current_mode].name,
                              current_mode + 1,
                              n_easing_modes);

      clutter_text_set_text (CLUTTER_TEXT (easing_mode_label), text);
      g_free (text);

      clutter_actor_get_size (main_stage,
                              &stage_width,
                              &stage_height);
      clutter_actor_get_size (easing_mode_label,
                              &label_width,
                              &label_height);

      clutter_actor_set_position (easing_mode_label,
                                  stage_width  - label_width  - 10,
                                  stage_height - label_height - 10);
    }
  else if (event->button == 1)
    {
      ClutterAnimation *animation;
      ClutterAnimationMode cur_mode;

      cur_mode = easing_modes[current_mode].mode;

      animation =
        clutter_actor_animate (rectangle, cur_mode, duration * 1000,
                               "x", event->x,
                               "y", event->y,
                               NULL);

      if (recenter)
        g_signal_connect_after (animation, "completed",
                                G_CALLBACK (on_animation_completed),
                                rectangle);
    }

  return TRUE;
}

static ClutterActor *
make_bouncer (const ClutterColor *base_color,
              gfloat              width,
              gfloat              height)
{
  ClutterActor *retval;
  cairo_t *cr;
  cairo_pattern_t *pattern;
  gfloat radius = MAX (width, height);

  retval = clutter_cairo_texture_new (width, height);

  cr = clutter_cairo_texture_create (CLUTTER_CAIRO_TEXTURE (retval));

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_ADD);

  cairo_arc (cr, radius / 2, radius / 2, radius / 2, 0.0, 2.0 * G_PI);

  pattern = cairo_pattern_create_radial (radius / 2, radius / 2, 0,
                                         radius, radius, radius);
  cairo_pattern_add_color_stop_rgba (pattern,
                                     0,
                                     base_color->red / 255.0,
                                     base_color->green / 255.0,
                                     base_color->blue / 255.0,
                                     base_color->alpha / 255.0);
  cairo_pattern_add_color_stop_rgba (pattern,
                                     0.9,
                                     base_color->red / 255.0,
                                     base_color->green / 255.0,
                                     base_color->blue / 255.0,
                                     0.1);

  cairo_set_source (cr, pattern);
  cairo_fill_preserve (cr);

  cairo_pattern_destroy (pattern);
  cairo_destroy (cr);

  clutter_actor_set_name (retval, "bouncer");
  clutter_actor_set_size (retval, width, height);
  clutter_actor_set_anchor_point (retval, width / 2, height / 2);
  clutter_actor_set_reactive (retval, TRUE);

  return retval;
}

static GOptionEntry test_easing_entries[] = {
  {
    "re-center", 'r',
    0,
    G_OPTION_ARG_NONE, &recenter,
    "Re-center the actor when the animation ends",
    NULL
  },
  {
    "duration", 'd',
    0,
    G_OPTION_ARG_INT, &duration,
    "Duration of the animation",
    "SECONDS"
  },

  { NULL }
};

G_MODULE_EXPORT int
test_easing_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect, *label;
  ClutterColor stage_color = { 0x88, 0x88, 0xdd, 0xff };
  ClutterColor rect_color = { 0xee, 0x33, 0, 0xff };
  gchar *text;
  gfloat stage_width, stage_height;
  gfloat label_width, label_height;

  clutter_init_with_args (&argc, &argv,
                          NULL,
                          test_easing_entries,
                          NULL,
                          NULL);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  main_stage = stage;

  clutter_actor_get_size (stage, &stage_width, &stage_height);

  rect = make_bouncer (&rect_color, 50, 50);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
  clutter_actor_set_position (rect, stage_width / 2, stage_height / 2);

  text = g_strdup_printf ("Easing mode: %s (%d of %d)\n"
                          "Right click to change the easing mode",
                          easing_modes[current_mode].name,
                          current_mode + 1,
                          n_easing_modes);

  label = clutter_text_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
  clutter_text_set_font_name (CLUTTER_TEXT (label), "Sans 18px");
  clutter_text_set_text (CLUTTER_TEXT (label), text);
  clutter_actor_get_size (label, &label_width, &label_height);
  clutter_actor_set_position (label,
                              stage_width - label_width - 10,
                              stage_height - label_height - 10);
  easing_mode_label = label;

  g_free (text);

  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (on_button_press),
                    rect);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
