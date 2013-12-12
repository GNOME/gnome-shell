#define CLUTTER_ENABLE_EXPERIMENTAL_API
#include <clutter/clutter.h>

#define STAGE_WIDTH (300)
#define STAGE_HEIGHT (300)

typedef struct
{
  ClutterActor *stage;

  ClutterActor *actor_group1;
  ClutterEffect *blur_effect1;

  ClutterActor *actor_group2;
  ClutterEffect *blur_effect2;
} Data;

static void
check_results (ClutterStage *stage, gpointer user_data)
{
  Data *data = user_data;
  gfloat width, height;
  ClutterRect rect;

  clutter_offscreen_effect_get_target_rect (CLUTTER_OFFSCREEN_EFFECT (data->blur_effect1),
                                            &rect);

  width = clutter_rect_get_width (&rect);
  height = clutter_rect_get_height (&rect);

  if (g_test_verbose ())
    g_print ("Checking effect1 size: %.2f x %.2f\n",
             clutter_rect_get_width (&rect),
             clutter_rect_get_height (&rect));

  g_assert_cmpint (width, <, STAGE_WIDTH);
  g_assert_cmpint (height, <, STAGE_HEIGHT);

  clutter_offscreen_effect_get_target_rect (CLUTTER_OFFSCREEN_EFFECT (data->blur_effect2),
                                            &rect);

  width = clutter_rect_get_width (&rect);
  height = clutter_rect_get_height (&rect);

  if (g_test_verbose ())
    g_print ("Checking effect2 size: %.2f x %.2f\n", width, height);

  g_assert_cmpint (width, ==, STAGE_WIDTH);
  g_assert_cmpint (height, ==, STAGE_HEIGHT);


  clutter_main_quit ();
}

static ClutterActor *
create_actor (gfloat x, gfloat y,
              gfloat width, gfloat height,
              const ClutterColor *color)
{
  return g_object_new (CLUTTER_TYPE_ACTOR,
                       "x", x,
                       "y", y,
                       "width", width,
                       "height", height,
                       "background-color", color,
                       NULL);
}

static void
actor_offscreen_limit_max_size (void)
{
  Data data;

  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return;

  data.stage = clutter_test_get_stage ();
  clutter_stage_set_paint_callback (CLUTTER_STAGE (data.stage),
                                    check_results,
                                    &data,
                                    NULL);
  clutter_actor_set_size (data.stage, STAGE_WIDTH, STAGE_HEIGHT);

  data.actor_group1 = clutter_actor_new ();
  clutter_actor_add_child (data.stage, data.actor_group1);
  data.blur_effect1 = clutter_blur_effect_new ();
  clutter_actor_add_effect (data.actor_group1, data.blur_effect1);
  clutter_actor_add_child (data.actor_group1,
                           create_actor (10, 10,
                                         100, 100,
                                         CLUTTER_COLOR_Blue));
  clutter_actor_add_child (data.actor_group1,
                           create_actor (100, 100,
                                         100, 100,
                                         CLUTTER_COLOR_Gray));

  data.actor_group2 = clutter_actor_new ();
  clutter_actor_add_child (data.stage, data.actor_group2);
  data.blur_effect2 = clutter_blur_effect_new ();
  clutter_actor_add_effect (data.actor_group2, data.blur_effect2);
  clutter_actor_add_child (data.actor_group2,
                           create_actor (-10, -10,
                                         100, 100,
                                         CLUTTER_COLOR_Yellow));
  clutter_actor_add_child (data.actor_group2,
                           create_actor (250, 10,
                                         100, 100,
                                         CLUTTER_COLOR_ScarletRed));
  clutter_actor_add_child (data.actor_group2,
                           create_actor (10, 250,
                                         100, 100,
                                         CLUTTER_COLOR_Yellow));

  clutter_actor_show (data.stage);

  clutter_main ();
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/actor/offscreen/limit-max-size", actor_offscreen_limit_max_size)
)
