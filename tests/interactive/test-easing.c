#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

/* all the easing modes provided by Clutter */
static const struct {
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

#define HELP_TEXT       "Easing mode: %s (%d of %d)\n" \
                        "Left click to tween\n" \
                        "Right click to change the easing mode"

static const gint n_easing_modes = G_N_ELEMENTS (easing_modes);
static gint current_mode = 0;

static gint duration = 1;
static gboolean recenter = FALSE;

static ClutterActor *main_stage = NULL;
static ClutterActor *easing_mode_label = NULL;

static ClutterAnimation *last_animation = NULL;

/* recenter_bouncer:
 *
 * repositions (through an animation) the bouncer at the center of the stage
 */
static void
recenter_bouncer (ClutterAnimation *animation,
                  ClutterActor     *rectangle)
{
  gfloat base_x, base_y;
  gint cur_mode;

  base_x = clutter_actor_get_width (main_stage) / 2;
  base_y = clutter_actor_get_height (main_stage) / 2;

  cur_mode = easing_modes[current_mode].mode;

  clutter_actor_animate (rectangle, cur_mode, 250,
                         "x", base_x,
                         "y", base_y,
                         NULL);
}

static gboolean
on_button_press (ClutterActor       *actor,
                 ClutterButtonEvent *event,
                 ClutterActor       *rectangle)
{
  if (event->button == CLUTTER_BUTTON_SECONDARY)
    {
      gchar *text;

      /* cycle through the various easing modes */
      current_mode = (current_mode + 1 < n_easing_modes)
                   ? current_mode + 1
                   : 0;

      /* update the text of the label */
      text = g_strdup_printf (HELP_TEXT,
                              easing_modes[current_mode].name,
                              current_mode + 1,
                              n_easing_modes);

      clutter_text_set_text (CLUTTER_TEXT (easing_mode_label), text);
      g_free (text);
    }
  else if (event->button == CLUTTER_BUTTON_PRIMARY)
    {
      ClutterAnimation *animation;
      ClutterAnimationMode cur_mode;

      cur_mode = easing_modes[current_mode].mode;

      /* tween the actor using the current easing mode */
      animation =
        clutter_actor_animate (rectangle, cur_mode, duration * 1000,
                               "x", event->x,
                               "y", event->y,
                               NULL);

      /* if we were asked to, recenter the bouncer at the end of the
       * animation. we keep track of the animation to avoid connecting
       * the signal handler to the same Animation twice.
       */
      if (recenter && last_animation != animation)
        g_signal_connect_after (animation, "completed",
                                G_CALLBACK (recenter_bouncer),
                                rectangle);

      last_animation = animation;
    }

  return TRUE;
}

static gboolean
draw_bouncer (ClutterCairoTexture *texture,
              cairo_t             *cr)
{
  const ClutterColor *bouncer_color;
  cairo_pattern_t *pattern;
  guint width, height;
  float radius;

  clutter_cairo_texture_get_surface_size (texture, &width, &height);

  radius = MAX (width, height);

  clutter_cairo_texture_clear (texture);

  cairo_arc (cr, radius / 2, radius / 2, radius / 2, 0.0, 2.0 * G_PI);

  bouncer_color = CLUTTER_COLOR_DarkScarletRed;

  pattern = cairo_pattern_create_radial (radius / 2, radius / 2, 0,
                                         radius, radius, radius);
  cairo_pattern_add_color_stop_rgba (pattern,
                                     0,
                                     bouncer_color->red / 255.0,
                                     bouncer_color->green / 255.0,
                                     bouncer_color->blue / 255.0,
                                     bouncer_color->alpha / 255.0);
  cairo_pattern_add_color_stop_rgba (pattern,
                                     0.85,
                                     bouncer_color->red / 255.0,
                                     bouncer_color->green / 255.0,
                                     bouncer_color->blue / 255.0,
                                     0.25);

  cairo_set_source (cr, pattern);
  cairo_fill_preserve (cr);

  cairo_pattern_destroy (pattern);

  return TRUE;
}

static ClutterActor *
make_bouncer (gfloat width,
              gfloat height)
{
  ClutterActor *retval;

  retval = clutter_cairo_texture_new (width, height);
  g_signal_connect (retval, "draw", G_CALLBACK (draw_bouncer), NULL);

  clutter_actor_set_name (retval, "bouncer");
  clutter_actor_set_size (retval, width, height);
  clutter_actor_set_anchor_point (retval, width / 2, height / 2);
  clutter_actor_set_reactive (retval, TRUE);

  /* make sure we draw the bouncer immediately */
  clutter_cairo_texture_invalidate (CLUTTER_CAIRO_TEXTURE (retval));

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
  gchar *text;
  gfloat stage_width, stage_height;
  GError *error = NULL;

  if (clutter_init_with_args (&argc, &argv,
                              NULL,
                              test_easing_entries,
                              NULL,
                              &error) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Easing Modes");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_LightSkyBlue);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  main_stage = stage;

  clutter_actor_get_size (stage, &stage_width, &stage_height);

  /* create the actor that we want to tween */
  rect = make_bouncer (50, 50);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
  clutter_actor_set_position (rect, stage_width / 2, stage_height / 2);

  text = g_strdup_printf (HELP_TEXT,
                          easing_modes[current_mode].name,
                          current_mode + 1,
                          n_easing_modes);

  label = clutter_text_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
  clutter_text_set_text (CLUTTER_TEXT (label), text);
  clutter_actor_add_constraint (label, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.95));
  clutter_actor_add_constraint (label, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.95));
  easing_mode_label = label;

  g_free (text);

  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (on_button_press),
                    rect);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_easing_describe (void)
{
  return "Visualize all easing modes provided by Clutter";
}
