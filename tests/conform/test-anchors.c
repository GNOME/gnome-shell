#include <clutter/clutter.h>
#include <stdlib.h>

#include "test-conform-common.h"

#define NOTIFY_ANCHOR_X        1
#define NOTIFY_ANCHOR_Y        2
#define NOTIFY_ANCHOR_GRAVITY  4

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

#define assert_notifications(flags)                     \
  do                                                    \
    {                                                   \
      g_assert (state->notifications == (flags));       \
      state->notifications = 0;                         \
    } while (0)

/* Helper macro to assert the transformed position. This needs to be a
   macro so that the assertion failure will report the right line
   number */
#define assert_position(state, x_, y_)                                  \
  do                                                                    \
    {                                                                   \
      ClutterVertex verts[4];                                           \
      clutter_actor_get_abs_allocation_vertices ((state)->rect, verts); \
      check_position ((state), (x_), (y_), verts);                      \
      g_assert (abs ((x_) - CLUTTER_UNITS_TO_DEVICE (verts[0].x))       \
                <= POSITION_TOLERANCE);                                 \
      g_assert (abs ((y_) - CLUTTER_UNITS_TO_DEVICE (verts[0].y))       \
                <= POSITION_TOLERANCE);                                 \
    } while (0)

static void
check_position (TestState *state,
                gint pos_x, gint pos_y,
                const ClutterVertex *verts)
{
  if (g_test_verbose ())
    g_print ("checking that (%i,%i) \xe2\x89\x88 (%i,%i): %s\n",
             pos_x, pos_y,
             CLUTTER_UNITS_TO_DEVICE (verts[0].x),
             CLUTTER_UNITS_TO_DEVICE (verts[0].y),
             abs (pos_x - verts[0].x) <= POSITION_TOLERANCE
             && abs (pos_y - verts[0].y) <= POSITION_TOLERANCE
             ? "yes" : "NO");
}

static gboolean
idle_cb (gpointer data)
{
  TestState *state = data;
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
  assert_position (state, 100 - RECT_WIDTH, 200 - RECT_HEIGHT);
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
  assert_position (state, 80, 170);
  assert_notifications (0);
  clutter_actor_set_size (rect, RECT_WIDTH, RECT_HEIGHT);

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

  /* Record notifications on the actor anchor properties */
  state.notifications = 0;
  g_signal_connect (state.rect, "notify::anchor-x",
                    G_CALLBACK (anchor_x_cb), &state);
  g_signal_connect (state.rect, "notify::anchor-y",
                    G_CALLBACK (anchor_y_cb), &state);
  g_signal_connect (state.rect, "notify::anchor-gravity",
                    G_CALLBACK (anchor_gravity_cb), &state);

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

