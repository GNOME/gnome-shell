#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static const ClutterColor colors[] = {
  { 255,   0,   0, 255 },
  {   0, 255,   0, 255 },
  {   0,   0, 255, 255 },
};

#define PADDING         (64.0f)
#define SIZE            (64.0f)

G_MODULE_EXPORT const char *
test_keyframe_transition_describe (void)
{
  return "Demonstrate the keyframe transition.";
}

G_MODULE_EXPORT int
test_keyframe_transition_main (int argc, char *argv[])
{
  ClutterActor *stage;
  int i;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Keyframe Transitions");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  for (i = 0; i < 3; i++)
    {
      ClutterTransition *transition;
      ClutterActor *rect;
      float cur_x, cur_y;
      float new_x;

      cur_x = PADDING;
      cur_y = PADDING + ((SIZE + PADDING) * i);

      new_x = clutter_actor_get_width (stage) - PADDING - SIZE;

      rect = clutter_actor_new ();

      clutter_actor_set_background_color (rect, &colors[i]);
      clutter_actor_set_size (rect, SIZE, SIZE);
      clutter_actor_set_position (rect, PADDING, cur_y);
      clutter_actor_add_child (stage, rect);

      clutter_actor_save_easing_state (rect);
      clutter_actor_set_easing_duration (rect, 2000);
      clutter_actor_set_easing_mode (rect, CLUTTER_LINEAR);

      transition = clutter_keyframe_transition_new ("x");
      clutter_transition_set_from (transition, G_TYPE_FLOAT, cur_x);
      clutter_transition_set_to (transition, G_TYPE_FLOAT, new_x);

      clutter_keyframe_transition_set (CLUTTER_KEYFRAME_TRANSITION (transition),
                                       G_TYPE_FLOAT, 1,
                                       0.5, new_x / 2.0f, CLUTTER_EASE_OUT_EXPO);

      clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), 1);
      clutter_timeline_set_auto_reverse (CLUTTER_TIMELINE (transition), TRUE);

      clutter_actor_add_transition (rect, "horizAnimation", transition);

      clutter_actor_restore_easing_state (rect);
    }

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
