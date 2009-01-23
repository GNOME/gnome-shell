#include <clutter/clutter.h>
#include <stdlib.h>

#include "test-conform-common.h"

#define NOTIFY_ANCHOR_X        1
#define NOTIFY_ANCHOR_Y        2
#define NOTIFY_ANCHOR_GRAVITY  4
#define NOTIFY_SCALE_X         8
#define NOTIFY_SCALE_Y         16
#define NOTIFY_SCALE_CENTER_X  32
#define NOTIFY_SCALE_CENTER_Y  64
#define NOTIFY_SCALE_GRAVITY   128

#define RECT_WIDTH             100
#define RECT_HEIGHT            80

/* Allow the transformed position by off by a certain number of
   pixels */
#define POSITION_TOLERANCE     2

typedef struct _TestState
{
  gulong notifications;
  ClutterActor *rect;
} TestState;

static const struct
{
  ClutterGravity gravity;
  gint x_pos, y_pos;
} gravities[] =
  {
    { CLUTTER_GRAVITY_NORTH,      RECT_WIDTH / 2, 0               },
    { CLUTTER_GRAVITY_NORTH_EAST, RECT_WIDTH,     0               },
    { CLUTTER_GRAVITY_EAST,       RECT_WIDTH,     RECT_HEIGHT / 2 },
    { CLUTTER_GRAVITY_SOUTH_EAST, RECT_WIDTH,     RECT_HEIGHT     },
    { CLUTTER_GRAVITY_SOUTH,      RECT_WIDTH / 2, RECT_HEIGHT     },
    { CLUTTER_GRAVITY_SOUTH_WEST, 0,              RECT_HEIGHT     },
    { CLUTTER_GRAVITY_WEST,       0,              RECT_HEIGHT / 2 },
    { CLUTTER_GRAVITY_NORTH_WEST, 0,              0               },
    { CLUTTER_GRAVITY_CENTER,     RECT_WIDTH / 2, RECT_HEIGHT / 2 }
  };

#define make_notify_cb(func, flag)                              \
  static void                                                   \
  func (GObject *object, GParamSpec *pspec, TestState *state)   \
  {                                                             \
    g_assert ((state->notifications & (flag)) == 0);            \
    state->notifications |= (flag);                             \
  }

make_notify_cb (anchor_x_cb, NOTIFY_ANCHOR_X);
make_notify_cb (anchor_y_cb, NOTIFY_ANCHOR_Y);
make_notify_cb (anchor_gravity_cb, NOTIFY_ANCHOR_GRAVITY);
make_notify_cb (scale_x_cb, NOTIFY_SCALE_X);
make_notify_cb (scale_y_cb, NOTIFY_SCALE_Y);
make_notify_cb (scale_center_x_cb, NOTIFY_SCALE_CENTER_X);
make_notify_cb (scale_center_y_cb, NOTIFY_SCALE_CENTER_Y);
make_notify_cb (scale_gravity_cb, NOTIFY_SCALE_GRAVITY);

#define assert_notifications(flags)                     \
  do                                                    \
    {                                                   \
      g_assert (state->notifications == (flags));       \
      state->notifications = 0;                         \
    } while (0)

/* Helper macro to assert the transformed position. This needs to be a
   macro so that the assertion failure will report the right line
   number */
#define assert_coords(state, x_1, y_1, x_2, y_2)                        \
  do                                                                    \
    {                                                                   \
      ClutterVertex verts[4];                                           \
      clutter_actor_get_abs_allocation_vertices ((state)->rect, verts); \
      check_coords ((state), (x_1), (y_1), (x_2), (y_2), verts);        \
      g_assert (abs ((x_1) - CLUTTER_UNITS_TO_DEVICE (verts[0].x))      \
                <= POSITION_TOLERANCE);                                 \
      g_assert (abs ((y_1) - CLUTTER_UNITS_TO_DEVICE (verts[0].y))      \
                <= POSITION_TOLERANCE);                                 \
      g_assert (abs ((x_2) - CLUTTER_UNITS_TO_DEVICE (verts[3].x))      \
                <= POSITION_TOLERANCE);                                 \
      g_assert (abs ((y_2) - CLUTTER_UNITS_TO_DEVICE (verts[3].y))      \
                <= POSITION_TOLERANCE);                                 \
    } while (0)

#define assert_position(state, x, y) \
  assert_coords((state), (x), (y), (x) + RECT_WIDTH, (y) + RECT_HEIGHT)

static void
check_coords (TestState *state,
              gint x_1, gint y_1, gint x_2, gint y_2,
              const ClutterVertex *verts)
{
  if (g_test_verbose ())
    g_print ("checking that (%i,%i,%i,%i) \xe2\x89\x88 (%i,%i,%i,%i): %s\n",
             x_1, y_1, x_2, y_2,
             CLUTTER_UNITS_TO_DEVICE (verts[0].x),
             CLUTTER_UNITS_TO_DEVICE (verts[0].y),
             CLUTTER_UNITS_TO_DEVICE (verts[3].x),
             CLUTTER_UNITS_TO_DEVICE (verts[3].y),
             abs (x_1 - verts[0].x) <= POSITION_TOLERANCE
             && abs (y_1 - verts[0].y) <= POSITION_TOLERANCE
             && abs (x_2 - verts[3].x) <= POSITION_TOLERANCE
             && abs (y_2 - verts[3].y) <= POSITION_TOLERANCE
             ? "yes" : "NO");
}

static void
test_anchor_point (TestState *state)
{
  ClutterActor *rect = state->rect;
  gint anchor_x, anchor_y;
  ClutterGravity anchor_gravity;
  int i;

  /* Assert the default settings */
  g_assert (clutter_actor_get_x (rect) == 100);
  g_assert (clutter_actor_get_y (rect) == 200);
  g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
  g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
  g_object_get (rect,
                "anchor-x", &anchor_x, "anchor-y", &anchor_y,
                "anchor-gravity", &anchor_gravity,
                NULL);
  g_assert (anchor_x == 0);
  g_assert (anchor_y == 0);
  g_assert (anchor_gravity == CLUTTER_GRAVITY_NONE);

  /* Change the anchor point */
  clutter_actor_set_anchor_point (rect, 20, 30);
  g_object_get (rect,
                "anchor-x", &anchor_x, "anchor-y", &anchor_y,
                "anchor-gravity", &anchor_gravity,
                NULL);
  g_assert (anchor_x == 20);
  g_assert (anchor_y == 30);
  g_assert (anchor_gravity == CLUTTER_GRAVITY_NONE);
  assert_position (state, 80, 170);
  assert_notifications (NOTIFY_ANCHOR_X | NOTIFY_ANCHOR_Y);

  /* Move the anchor point */
  clutter_actor_move_anchor_point (rect, 40, 50);
  g_object_get (rect,
                "anchor-x", &anchor_x, "anchor-y", &anchor_y,
                "anchor-gravity", &anchor_gravity,
                NULL);
  g_assert (anchor_x == 40);
  g_assert (anchor_y == 50);
  g_assert (anchor_gravity == CLUTTER_GRAVITY_NONE);
  assert_position (state, 80, 170);
  assert_notifications (NOTIFY_ANCHOR_X | NOTIFY_ANCHOR_Y);

  /* Put the actor back to its default position */
  clutter_actor_set_position (rect, 100, 200);

  /* Change the anchor point with each of the gravities */
  for (i = 0; i < G_N_ELEMENTS (gravities); i++)
    {
      if (g_test_verbose ())
        {
          GEnumClass *gravity_class = g_type_class_ref (CLUTTER_TYPE_GRAVITY);
          GEnumValue *value = g_enum_get_value (gravity_class,
                                                gravities[i].gravity);
          g_print ("Setting gravity to %s\n",
                   value ? value->value_name : "?");
          g_type_class_unref (gravity_class);
        }

      g_object_set (rect, "anchor-gravity", gravities[i].gravity, NULL);

      g_object_get (rect,
                    "anchor-x", &anchor_x, "anchor-y", &anchor_y,
                    "anchor-gravity", &anchor_gravity,
                    NULL);
      g_assert (anchor_x == gravities[i].x_pos);
      g_assert (anchor_y == gravities[i].y_pos);
      g_assert (anchor_gravity == gravities[i].gravity);
      assert_position (state,
                       100 - gravities[i].x_pos,
                       200 - gravities[i].y_pos);

      assert_notifications (NOTIFY_ANCHOR_X | NOTIFY_ANCHOR_Y
                            | NOTIFY_ANCHOR_GRAVITY);
    }

  /* Verify that the anchor point moves if the actor changes size when
     it is set from the gravity */
  clutter_actor_set_size (rect, RECT_WIDTH * 2, RECT_HEIGHT * 2);
  g_object_get (rect,
                "anchor-x", &anchor_x, "anchor-y", &anchor_y,
                "anchor-gravity", &anchor_gravity,
                NULL);
  g_assert (anchor_x == RECT_WIDTH);
  g_assert (anchor_y == RECT_HEIGHT);
  g_assert (anchor_gravity == CLUTTER_GRAVITY_CENTER);
  assert_coords (state, 100 - RECT_WIDTH, 200 - RECT_HEIGHT,
                 100 + RECT_WIDTH, 200 + RECT_HEIGHT);
  assert_notifications (0);
  clutter_actor_set_size (rect, RECT_WIDTH, RECT_HEIGHT);

  /* Change the anchor point using units again to assert that the
     gravity property changes */
  clutter_actor_set_anchor_point (rect, 20, 30);
  g_object_get (rect,
                "anchor-x", &anchor_x, "anchor-y", &anchor_y,
                "anchor-gravity", &anchor_gravity,
                NULL);
  g_assert (anchor_x == 20);
  g_assert (anchor_y == 30);
  g_assert (anchor_gravity == CLUTTER_GRAVITY_NONE);
  assert_position (state, 80, 170);
  assert_notifications (NOTIFY_ANCHOR_X | NOTIFY_ANCHOR_Y
                        | NOTIFY_ANCHOR_GRAVITY);

  /* Verify that the anchor point doesn't move if the actor changes
     size when it is set from units */
  clutter_actor_set_size (rect, RECT_WIDTH * 2, RECT_HEIGHT * 2);
  g_object_get (rect,
                "anchor-x", &anchor_x, "anchor-y", &anchor_y,
                "anchor-gravity", &anchor_gravity,
                NULL);
  g_assert (anchor_x == 20);
  g_assert (anchor_y == 30);
  g_assert (anchor_gravity == CLUTTER_GRAVITY_NONE);
  assert_coords (state, 80, 170, 80 + RECT_WIDTH * 2, 170 + RECT_HEIGHT * 2);
  assert_notifications (0);
  clutter_actor_set_size (rect, RECT_WIDTH, RECT_HEIGHT);

  /* Put the anchor back */
  clutter_actor_set_anchor_point_from_gravity (rect, CLUTTER_GRAVITY_NONE);
  assert_notifications (NOTIFY_ANCHOR_X | NOTIFY_ANCHOR_Y);
}

static void
test_scale_center (TestState *state)
{
  ClutterActor *rect = state->rect;
  gdouble scale_x, scale_y;
  gint center_x, center_y;
  ClutterGravity gravity;
  int i;

  /* Assert the default settings */
  g_assert (clutter_actor_get_x (rect) == 100);
  g_assert (clutter_actor_get_y (rect) == 200);
  g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
  g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
  g_object_get (rect,
                "scale-center-x", &center_x, "scale-center-y", &center_y,
                "scale-x", &scale_x, "scale-y", &scale_y,
                "scale-gravity", &gravity,
                NULL);
  g_assert (center_x == 0);
  g_assert (center_y == 0);
  g_assert (scale_x == 1.0);
  g_assert (scale_y == 1.0);
  g_assert (gravity == CLUTTER_GRAVITY_NONE);

  /* Try changing the scale without affecting the center */
  g_object_set (rect, "scale-x", 2.0, "scale-y", 3.0, NULL);
  g_assert (clutter_actor_get_x (rect) == 100);
  g_assert (clutter_actor_get_y (rect) == 200);
  g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
  g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
  g_object_get (rect,
                "scale-center-x", &center_x, "scale-center-y", &center_y,
                "scale-x", &scale_x, "scale-y", &scale_y,
                "scale-gravity", &gravity,
                NULL);
  g_assert (center_x == 0);
  g_assert (center_y == 0);
  g_assert (scale_x == 2.0);
  g_assert (scale_y == 3.0);
  g_assert (gravity == CLUTTER_GRAVITY_NONE);
  assert_notifications (NOTIFY_SCALE_X | NOTIFY_SCALE_Y);
  assert_coords (state, 100, 200, 100 + RECT_WIDTH * 2, 200 + RECT_HEIGHT * 3);

  /* Change the scale and center */
  g_object_set (rect, "scale-x", 4.0, "scale-y", 2.0,
                "scale-center-x", 10, "scale-center-y", 20, NULL);
  g_assert (clutter_actor_get_x (rect) == 100);
  g_assert (clutter_actor_get_y (rect) == 200);
  g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
  g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
  g_object_get (rect,
                "scale-center-x", &center_x, "scale-center-y", &center_y,
                "scale-x", &scale_x, "scale-y", &scale_y,
                "scale-gravity", &gravity,
                NULL);
  g_assert (center_x == 10);
  g_assert (center_y == 20);
  g_assert (scale_x == 4.0);
  g_assert (scale_y == 2.0);
  g_assert (gravity == CLUTTER_GRAVITY_NONE);
  assert_notifications (NOTIFY_SCALE_X | NOTIFY_SCALE_Y
                        | NOTIFY_SCALE_CENTER_X | NOTIFY_SCALE_CENTER_Y);
  assert_coords (state, 100 + 10 - 10 * 4, 200 + 20 - 20 * 2,
                 100 + 10 + (RECT_WIDTH - 10) * 4,
                 200 + 20 + (RECT_HEIGHT - 20) * 2);

  /* Change the anchor point with each of the gravities */
  for (i = 0; i < G_N_ELEMENTS (gravities); i++)
    {
      if (g_test_verbose ())
        {
          GEnumClass *gravity_class = g_type_class_ref (CLUTTER_TYPE_GRAVITY);
          GEnumValue *value = g_enum_get_value (gravity_class,
                                                gravities[i].gravity);
          g_print ("Setting scale center to %s\n",
                   value ? value->value_name : "?");
          g_type_class_unref (gravity_class);
        }

      g_object_set (rect, "scale-gravity", gravities[i].gravity, NULL);

      g_assert (clutter_actor_get_x (rect) == 100);
      g_assert (clutter_actor_get_y (rect) == 200);
      g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
      g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
      g_object_get (rect,
                    "scale-center-x", &center_x, "scale-center-y", &center_y,
                    "scale-x", &scale_x, "scale-y", &scale_y,
                    "scale-gravity", &gravity,
                    NULL);
      g_assert (center_x == gravities[i].x_pos);
      g_assert (center_y == gravities[i].y_pos);
      g_assert (scale_x == 4.0);
      g_assert (scale_y == 2.0);
      g_assert (gravity == gravities[i].gravity);
      assert_notifications (NOTIFY_SCALE_X | NOTIFY_SCALE_Y
                            | NOTIFY_SCALE_CENTER_X | NOTIFY_SCALE_CENTER_Y
                            | NOTIFY_SCALE_GRAVITY);
      assert_coords (state,
                     100 - gravities[i].x_pos * 3,
                     200 - gravities[i].y_pos,
                     100 + (gravities[i].x_pos
                            + (RECT_WIDTH - gravities[i].x_pos) * 4),
                     200 + (gravities[i].y_pos
                            + (RECT_HEIGHT - gravities[i].y_pos) * 2));
    }

  /* Change the scale center using units again to assert that the
     gravity property changes */
  clutter_actor_set_scale_full (rect, 4, 2, 10, 20);
  g_object_get (rect,
                "scale-center-x", &center_x, "scale-center-y", &center_y,
                "scale-x", &scale_x, "scale-y", &scale_y,
                "scale-gravity", &gravity,
                NULL);
  g_assert (center_x == 10);
  g_assert (center_y == 20);
  g_assert (scale_x == 4.0);
  g_assert (scale_y == 2.0);
  g_assert (gravity == CLUTTER_GRAVITY_NONE);
  assert_notifications (NOTIFY_SCALE_X | NOTIFY_SCALE_Y
                        | NOTIFY_SCALE_CENTER_X | NOTIFY_SCALE_CENTER_Y
                        | NOTIFY_SCALE_GRAVITY);
  assert_coords (state, 100 + 10 - 10 * 4, 200 + 20 - 20 * 2,
                 100 + 10 + (RECT_WIDTH - 10) * 4,
                 200 + 20 + (RECT_HEIGHT - 20) * 2);
}

static gboolean
idle_cb (gpointer data)
{
  test_anchor_point (data);
  test_scale_center (data);

  clutter_main_quit ();

  return FALSE;
}

void
test_anchors (TestConformSimpleFixture *fixture,
              gconstpointer data)
{
  TestState state;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();

  state.rect = clutter_rectangle_new ();
  clutter_container_add (CLUTTER_CONTAINER (stage), state.rect, NULL);
  clutter_actor_set_position (state.rect, 100, 200);
  clutter_actor_set_size (state.rect, RECT_WIDTH, RECT_HEIGHT);

  /* Record notifications on the actor properties */
  state.notifications = 0;
  g_signal_connect (state.rect, "notify::anchor-x",
                    G_CALLBACK (anchor_x_cb), &state);
  g_signal_connect (state.rect, "notify::anchor-y",
                    G_CALLBACK (anchor_y_cb), &state);
  g_signal_connect (state.rect, "notify::anchor-gravity",
                    G_CALLBACK (anchor_gravity_cb), &state);
  g_signal_connect (state.rect, "notify::scale-x",
                    G_CALLBACK (scale_x_cb), &state);
  g_signal_connect (state.rect, "notify::scale-y",
                    G_CALLBACK (scale_y_cb), &state);
  g_signal_connect (state.rect, "notify::scale-center-x",
                    G_CALLBACK (scale_center_x_cb), &state);
  g_signal_connect (state.rect, "notify::scale-center-y",
                    G_CALLBACK (scale_center_y_cb), &state);
  g_signal_connect (state.rect, "notify::scale-gravity",
                    G_CALLBACK (scale_gravity_cb), &state);

  /* Run the tests in a low priority idle function so that we can be
     sure the stage is correctly setup */
  g_idle_add_full (G_PRIORITY_LOW, idle_cb, &state, NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_idle_remove_by_data (&state);

  clutter_actor_destroy (state.rect);

  if (g_test_verbose ())
    g_print ("OK\n");
}

