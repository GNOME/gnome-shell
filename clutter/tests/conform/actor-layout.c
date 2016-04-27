#include <clutter/clutter.h>

static void
actor_basic_layout (void)
{
  ClutterActor *stage = clutter_test_get_stage ();
  ClutterActor *vase;
  ClutterActor *flower[3];
  ClutterPoint p;

  vase = clutter_actor_new ();
  clutter_actor_set_name (vase, "Vase");
  clutter_actor_set_layout_manager (vase, clutter_flow_layout_new (CLUTTER_FLOW_HORIZONTAL));
  clutter_actor_add_child (stage, vase);

  flower[0] = clutter_actor_new ();
  clutter_actor_set_background_color (flower[0], CLUTTER_COLOR_Red);
  clutter_actor_set_size (flower[0], 100, 100);
  clutter_actor_set_name (flower[0], "Red Flower");
  clutter_actor_add_child (vase, flower[0]);

  flower[1] = clutter_actor_new ();
  clutter_actor_set_background_color (flower[1], CLUTTER_COLOR_Yellow);
  clutter_actor_set_size (flower[1], 100, 100);
  clutter_actor_set_name (flower[1], "Yellow Flower");
  clutter_actor_add_child (vase, flower[1]);

  flower[2] = clutter_actor_new ();
  clutter_actor_set_background_color (flower[2], CLUTTER_COLOR_Green);
  clutter_actor_set_size (flower[2], 100, 100);
  clutter_actor_set_name (flower[2], "Green Flower");
  clutter_actor_add_child (vase, flower[2]);

  clutter_point_init (&p, 50, 50);
  clutter_test_assert_actor_at_point (stage, &p, flower[0]);

  clutter_point_init (&p, 150, 50);
  clutter_test_assert_actor_at_point (stage, &p, flower[1]);

  clutter_point_init (&p, 250, 50);
  clutter_test_assert_actor_at_point (stage, &p, flower[2]);
}

static void
actor_margin_layout (void)
{
  ClutterActor *stage = clutter_test_get_stage ();
  ClutterActor *vase;
  ClutterActor *flower[3];
  ClutterPoint p;

  vase = clutter_actor_new ();
  clutter_actor_set_name (vase, "Vase");
  clutter_actor_set_layout_manager (vase, clutter_box_layout_new ());
  clutter_actor_add_child (stage, vase);

  flower[0] = clutter_actor_new ();
  clutter_actor_set_background_color (flower[0], CLUTTER_COLOR_Red);
  clutter_actor_set_size (flower[0], 100, 100);
  clutter_actor_set_name (flower[0], "Red Flower");
  clutter_actor_add_child (vase, flower[0]);

  flower[1] = clutter_actor_new ();
  clutter_actor_set_background_color (flower[1], CLUTTER_COLOR_Yellow);
  clutter_actor_set_size (flower[1], 100, 100);
  clutter_actor_set_name (flower[1], "Yellow Flower");
  clutter_actor_set_margin_right (flower[1], 6);
  clutter_actor_set_margin_left (flower[1], 6);
  clutter_actor_add_child (vase, flower[1]);

  flower[2] = clutter_actor_new ();
  clutter_actor_set_background_color (flower[2], CLUTTER_COLOR_Green);
  clutter_actor_set_size (flower[2], 100, 100);
  clutter_actor_set_name (flower[2], "Green Flower");
  clutter_actor_set_margin_top (flower[2], 6);
  clutter_actor_set_margin_bottom (flower[2], 6);
  clutter_actor_add_child (vase, flower[2]);

  clutter_point_init (&p, 0, 7);
  clutter_test_assert_actor_at_point (stage, &p, flower[0]);

  clutter_point_init (&p, 106, 50);
  clutter_test_assert_actor_at_point (stage, &p, flower[1]);

  clutter_point_init (&p, 212, 7);
  clutter_test_assert_actor_at_point (stage, &p, flower[2]);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/actor/layout/basic", actor_basic_layout)
  CLUTTER_TEST_UNIT ("/actor/layout/margin", actor_margin_layout)
)
