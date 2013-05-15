#include <clutter/clutter.h>

#include "test-conform-common.h"

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

  clutter_offscreen_effect_get_target_size (CLUTTER_OFFSCREEN_EFFECT (data->blur_effect1),
                                            &width, &height);

  if (g_test_verbose ())
    g_print ("Checking effect1 size: %.2f x %.2f\n", width, height);

  g_assert_cmpint (width, <, STAGE_WIDTH);
  g_assert_cmpint (height, <, STAGE_HEIGHT);

  clutter_offscreen_effect_get_target_size (CLUTTER_OFFSCREEN_EFFECT (data->blur_effect2),
                                            &width, &height);

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

void
actor_offscreen_limit_max_size (TestConformSimpleFixture *fixture,
                                gconstpointer test_data)
{
  if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
    {
      Data data;

      data.stage = clutter_stage_new ();
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

      clutter_actor_destroy (data.stage);

      if (g_test_verbose ())
        g_print ("OK\n");
    }
  else if (g_test_verbose ())
    g_print ("Skipping\n");
}
