#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

const struct {
  const gchar *name;
  ClutterAnimationMode mode;
} easing_modes[] = {
  { "linear", CLUTTER_LINEAR },
  { "sine-in", CLUTTER_SINE_IN },
  { "sine-out", CLUTTER_SINE_OUT },
  { "sine-in-out", CLUTTER_SINE_IN_OUT },
  { "ease-in", CLUTTER_EASE_IN },
  { "ease-out", CLUTTER_EASE_OUT },
  { "ease-in-out", CLUTTER_EASE_IN_OUT },
  { "expo-in", CLUTTER_EXPO_IN },
  { "expo-out", CLUTTER_EXPO_OUT },
  { "expo-in-out", CLUTTER_EXPO_IN_OUT },
  { "smooth-in-out", CLUTTER_SMOOTH_IN_OUT }
};

static const gint n_easing_modes = G_N_ELEMENTS (easing_modes);
static gint current_mode = 0;

static ClutterActor *main_stage        = NULL;
static ClutterActor *easing_mode_label = NULL;

static gboolean
on_button_press (ClutterActor       *actor,
                 ClutterButtonEvent *event,
                 ClutterActor       *rectangle)
{
  ClutterAnimation *animation;
  ClutterAnimationMode cur_mode;
  gchar *text;
  guint stage_width, stage_height;
  guint label_width, label_height;

  text = g_strdup_printf ("Easing mode: %s (%d of %d)\n",
                          easing_modes[current_mode].name,
                          current_mode + 1,
                          n_easing_modes);

  clutter_text_set_text (CLUTTER_TEXT (easing_mode_label), text);
  g_free (text);

  clutter_actor_get_size (main_stage, &stage_width, &stage_height);
  clutter_actor_get_size (easing_mode_label, &label_width, &label_height);

  clutter_actor_set_position (easing_mode_label,
                              stage_width  - label_width  - 10,
                              stage_height - label_height - 10);

  cur_mode = easing_modes[current_mode].mode;

  animation =
    clutter_actor_animate (rectangle, cur_mode, 2000,
                           "x", event->x,
                           "y", event->y,
                           NULL);

  current_mode = (current_mode + 1 < n_easing_modes) ? current_mode + 1 : 0;

  return TRUE;
}

G_MODULE_EXPORT int
test_easing_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect, *label;
  ClutterColor stage_color = { 0x66, 0x66, 0xdd, 0xff };
  ClutterColor rect_color = { 0x44, 0xdd, 0x44, 0xff };
  gchar *text;
  guint stage_width, stage_height;
  guint label_width, label_height;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  main_stage = stage;

  clutter_actor_get_size (stage, &stage_width, &stage_height);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
  clutter_actor_set_size (rect, 50, 50);
  clutter_actor_set_anchor_point (rect, 25, 25);
  clutter_actor_set_position (rect, stage_width / 2, stage_height / 2);
  clutter_actor_set_opacity (rect, 0x88);
  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (on_button_press),
                    rect);

  text = g_strdup_printf ("Easing mode: %s (%d of %d)\n",
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

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
