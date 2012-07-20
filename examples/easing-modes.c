#include <stdlib.h>
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
  { "stepStart", CLUTTER_STEP_START },
  { "stepEnd", CLUTTER_STEP_END },
  { "ease", CLUTTER_EASE },
  { "easeIn", CLUTTER_EASE_IN },
  { "easeOut", CLUTTER_EASE_OUT },
  { "easeInOut", CLUTTER_EASE_IN_OUT },
};

#define HELP_TEXT       "<b>Easing mode: %s (%d of %d)</b>\n" \
                        "Left click to tween\n" \
                        "Middle click to jump\n" \
                        "Right click to change the easing mode"

static const gint n_easing_modes = G_N_ELEMENTS (easing_modes);
static gint current_mode = 0;

static gint duration = 1;

static ClutterActor *main_stage = NULL;
static ClutterActor *easing_mode_label = NULL;

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

      clutter_text_set_markup (CLUTTER_TEXT (easing_mode_label), text);
      g_free (text);
    }
  else if (event->button == CLUTTER_BUTTON_MIDDLE)
    {
      clutter_actor_set_position (rectangle, event->x, event->y);
    }
  else if (event->button == CLUTTER_BUTTON_PRIMARY)
    {
      ClutterAnimationMode cur_mode;

      cur_mode = easing_modes[current_mode].mode;

      clutter_actor_save_easing_state (rectangle);

      /* tween the actor using the current easing mode */
      clutter_actor_set_easing_mode (rectangle, cur_mode);
      clutter_actor_set_easing_duration (rectangle, duration * 1000);

      clutter_actor_set_position (rectangle, event->x, event->y);

      clutter_actor_restore_easing_state (rectangle);
    }

  return CLUTTER_EVENT_STOP;
}

static gboolean
draw_bouncer (ClutterCanvas *canvas,
              cairo_t       *cr,
              int            width,
              int            height)
{
  const ClutterColor *bouncer_color;
  cairo_pattern_t *pattern;
  float radius;

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  radius = MAX (width, height);

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
  ClutterContent *canvas;

  retval = clutter_actor_new ();

  canvas = clutter_canvas_new ();
  g_signal_connect (canvas, "draw", G_CALLBACK (draw_bouncer), NULL);
  clutter_canvas_set_size (CLUTTER_CANVAS (canvas), width, height);

  clutter_actor_set_name (retval, "bouncer");
  clutter_actor_set_size (retval, width, height);
  clutter_actor_set_pivot_point (retval, 0.5f, 0.5f);
  clutter_actor_set_translation (retval, width / -2.f, height / -2.f, 0.f);
  clutter_actor_set_reactive (retval, TRUE);
  clutter_actor_set_content (retval, canvas);

  g_object_unref (canvas);

  return retval;
}

static GOptionEntry test_easing_entries[] = {
  {
    "duration", 'd',
    0,
    G_OPTION_ARG_INT, &duration,
    "Duration of the animation",
    "SECONDS"
  },

  { NULL }
};

int
main (int argc, char *argv[])
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
  clutter_actor_add_child (stage, rect);
  clutter_actor_set_position (rect, stage_width / 2, stage_height / 2);

  text = g_strdup_printf (HELP_TEXT,
                          easing_modes[current_mode].name,
                          current_mode + 1,
                          n_easing_modes);

  label = clutter_text_new ();
  clutter_actor_add_child (stage, label);
  clutter_text_set_markup (CLUTTER_TEXT (label), text);
  clutter_text_set_line_alignment (CLUTTER_TEXT (label), PANGO_ALIGN_RIGHT);
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
