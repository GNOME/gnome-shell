#include <stdlib.h>
#include <string.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include <clutter/clutter.h>

#define NOTIFY_ANCHOR_X                  (1 << 0)
#define NOTIFY_ANCHOR_Y                  (1 << 1)
#define NOTIFY_ANCHOR_GRAVITY            (1 << 2)
#define NOTIFY_SCALE_X                   (1 << 3)
#define NOTIFY_SCALE_Y                   (1 << 4)
#define NOTIFY_SCALE_CENTER_X            (1 << 5)
#define NOTIFY_SCALE_CENTER_Y            (1 << 6)
#define NOTIFY_SCALE_GRAVITY             (1 << 7)
#define NOTIFY_ROTATION_ANGLE_X          (1 << 8)
#define NOTIFY_ROTATION_ANGLE_Y          (1 << 9)
#define NOTIFY_ROTATION_ANGLE_Z          (1 << 10)
#define NOTIFY_ROTATION_CENTER_X         (1 << 11)
#define NOTIFY_ROTATION_CENTER_Y         (1 << 12)
#define NOTIFY_ROTATION_CENTER_Z         (1 << 13)
#define NOTIFY_ROTATION_CENTER_Z_GRAVITY (1 << 14)

#define RECT_WIDTH             100.0
#define RECT_HEIGHT            80.0

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
  gfloat x_pos;
  gfloat y_pos;
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

static const char * const properties[] = {
  "anchor-x",
  "anchor-y",
  "anchor-gravity",
  "scale-x",
  "scale-y",
  "scale-center-x",
  "scale-center-y",
  "scale-gravity",
  "rotation-angle-x",
  "rotation-angle-y",
  "rotation-angle-z",
  "rotation-center-x",
  "rotation-center-y",
  "rotation-center-z",
  "rotation-center-z-gravity"
};

static const int n_properties = G_N_ELEMENTS (properties);

static void
notify_cb (GObject *object, GParamSpec *pspec, TestState *state)
{
  int i;
  int new_flags = 0;
  int flag = 1;

  for (i = 0; i < n_properties; i++)
    {
      if (!strcmp (properties[i], pspec->name))
        new_flags |= flag;

      flag <<= 1;
    }

  g_assert ((new_flags & state->notifications) == 0);

  state->notifications |= new_flags;
}

#define assert_notifications(flags)     G_STMT_START {  \
  g_assert (state->notifications == (flags));           \
  state->notifications = 0;             } G_STMT_END

/* Helper macro to assert the transformed position. This needs to be a
   macro so that the assertion failure will report the right line
   number */
#define assert_coords(state, x_1, y_1, x_2, y_2)        G_STMT_START {  \
  ClutterVertex verts[4];                                               \
  clutter_actor_get_abs_allocation_vertices ((state)->rect, verts);     \
  check_coords ((state), (x_1), (y_1), (x_2), (y_2), verts);            \
  g_assert (approx_equal ((x_1), verts[0].x));                          \
  g_assert (approx_equal ((y_1), verts[0].y));                          \
  g_assert (approx_equal ((x_2), verts[3].x));                          \
  g_assert (approx_equal ((y_2), verts[3].y));          } G_STMT_END

#define assert_position(state, x, y) \
  assert_coords((state), (x), (y), (x) + RECT_WIDTH, (y) + RECT_HEIGHT)

#define assert_vertex_and_free(v, xc, yc, zc)   G_STMT_START {  \
  g_assert (approx_equal (v->x, xc) &&                          \
            approx_equal (v->y, yc) &&                          \
            approx_equal (v->z, zc));                           \
  g_boxed_free (CLUTTER_TYPE_VERTEX, v);        } G_STMT_END

static inline gboolean
approx_equal (int a, int b)
{
  return abs (a - b) <= POSITION_TOLERANCE;
}

static void
check_coords (TestState *state,
              gint x_1,
              gint y_1,
              gint x_2,
              gint y_2,
              const ClutterVertex *verts)
{
  if (g_test_verbose ())
    g_print ("checking that (%i,%i,%i,%i) \xe2\x89\x88 (%i,%i,%i,%i): %s\n",
             x_1, y_1, x_2, y_2,
             (int) (verts[0].x),
             (int) (verts[0].y),
             (int) (verts[3].x),
             (int) (verts[3].y),
             approx_equal (x_1, verts[0].x) &&
             approx_equal (y_1, verts[0].y) &&
             approx_equal (x_2, verts[3].x) &&
             approx_equal (y_2, verts[3].y) ? "yes"
                                            : "NO");
}

static void
test_anchor_point (TestState *state)
{
  ClutterActor *rect = state->rect;
  gfloat anchor_x, anchor_y;
  ClutterGravity anchor_gravity;
  int i;

  /* Assert the default settings */
  g_assert (clutter_actor_get_x (rect) == 100);
  g_assert (clutter_actor_get_y (rect) == 200);
  g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
  g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
  g_object_get (rect,
                "anchor-x", &anchor_x,
                "anchor-y", &anchor_y,
                "anchor-gravity", &anchor_gravity,
                NULL);
  g_assert (anchor_x == 0);
  g_assert (anchor_y == 0);
  g_assert (anchor_gravity == CLUTTER_GRAVITY_NONE);

  /* Change the anchor point */
  clutter_actor_set_anchor_point (rect, 20, 30);
  g_object_get (rect,
                "anchor-x", &anchor_x,
                "anchor-y", &anchor_y,
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
                "anchor-x", &anchor_x,
                "anchor-y", &anchor_y,
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
                    "anchor-x", &anchor_x,
                    "anchor-y", &anchor_y,
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
                "anchor-x", &anchor_x,
                "anchor-y", &anchor_y,
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
                "anchor-x", &anchor_x,
                "anchor-y", &anchor_y,
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
  gfloat center_x, center_y;
  ClutterGravity gravity;
  int i;

  /* Assert the default settings */
  g_assert (clutter_actor_get_x (rect) == 100);
  g_assert (clutter_actor_get_y (rect) == 200);
  g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
  g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
  g_object_get (rect,
                "scale-center-x", &center_x,
                "scale-center-y", &center_y,
                "scale-x", &scale_x,
                "scale-y", &scale_y,
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
                "scale-center-x", &center_x,
                "scale-center-y", &center_y,
                "scale-x", &scale_x,
                "scale-y", &scale_y,
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
  g_object_set (rect,
                "scale-x", 4.0,
                "scale-y", 2.0,
                "scale-center-x", 10.0,
                "scale-center-y", 20.0,
                NULL);
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
      assert_notifications (NOTIFY_SCALE_CENTER_X |
                            NOTIFY_SCALE_CENTER_Y |
                            NOTIFY_SCALE_GRAVITY);
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
                "scale-center-x", &center_x,
                "scale-center-y", &center_y,
                "scale-x", &scale_x,
                "scale-y", &scale_y,
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

  /* Put the scale back to normal */
  clutter_actor_set_scale_full (rect, 1, 1, 0, 0);
  assert_notifications (NOTIFY_SCALE_X | NOTIFY_SCALE_Y
                        | NOTIFY_SCALE_CENTER_X | NOTIFY_SCALE_CENTER_Y);
}

static void
test_rotate_center (TestState *state)
{
  ClutterActor *rect = state->rect;
  gdouble angle_x, angle_y, angle_z;
  ClutterVertex *center_x, *center_y, *center_z;
  ClutterGravity z_center_gravity;
  gfloat stage_width, stage_height;
  gfloat rect_x, rect_y;
  int i;

  /* Position the rectangle at the center of the stage so that
     rotations by 90° along the X or Y axis will cause the actor to be
     appear as a flat line. This makes verifying the transformations
     easier */
  clutter_actor_get_size (clutter_actor_get_stage (rect),
                          &stage_width,
                          &stage_height);
  rect_x = stage_width / 2;
  rect_y = stage_height / 2;
  clutter_actor_set_position (rect, rect_x, rect_y);

  /* Assert the default settings */
  g_assert_cmpfloat (clutter_actor_get_x (rect), ==, rect_x);
  g_assert_cmpfloat (clutter_actor_get_y (rect), ==, rect_y);
  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, RECT_WIDTH);
  g_assert_cmpfloat (clutter_actor_get_height (rect), ==, RECT_HEIGHT);
  g_object_get (rect,
                "rotation-angle-x", &angle_x,
                "rotation-angle-y", &angle_y,
                "rotation-angle-z", &angle_z,
                "rotation-center-x", &center_x,
                "rotation-center-y", &center_y,
                "rotation-center-z", &center_z,
                "rotation-center-z-gravity", &z_center_gravity,
                NULL);
  g_assert (angle_x == 0.0);
  g_assert (angle_y == 0.0);
  g_assert (angle_z == 0.0);
  assert_vertex_and_free (center_x, 0, 0, 0);
  assert_vertex_and_free (center_y, 0, 0, 0);
  assert_vertex_and_free (center_z, 0, 0, 0);
  g_assert (z_center_gravity == CLUTTER_GRAVITY_NONE);

  /* Change each of the rotation angles without affecting the center
     point */
  for (i = CLUTTER_X_AXIS; i <= CLUTTER_Z_AXIS; i++)
    {
      char prop_name[] = "rotation-angle- ";
      prop_name[sizeof (prop_name) - 2] = i - CLUTTER_X_AXIS + 'x';

      if (g_test_verbose ())
        g_print ("Setting %s to 90 degrees\n", prop_name);

      g_object_set (rect, prop_name, 90.0, NULL);
      assert_notifications (NOTIFY_ROTATION_ANGLE_X << (i - CLUTTER_X_AXIS));

      g_assert (clutter_actor_get_x (rect) == rect_x);
      g_assert (clutter_actor_get_y (rect) == rect_y);
      g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
      g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
      g_object_get (rect,
                    "rotation-angle-x", &angle_x,
                    "rotation-angle-y", &angle_y,
                    "rotation-angle-z", &angle_z,
                    "rotation-center-x", &center_x,
                    "rotation-center-y", &center_y,
                    "rotation-center-z", &center_z,
                    "rotation-center-z-gravity", &z_center_gravity,
                    NULL);
      if (i == CLUTTER_X_AXIS)
        {
          g_assert (angle_x == 90.0);
          assert_coords (state, rect_x, rect_y, verts[3].x, rect_y);
        }
      else
        g_assert (angle_x == 0.0);
      if (i == CLUTTER_Y_AXIS)
        {
          g_assert (angle_y == 90.0);
          assert_coords (state, rect_x, rect_y, rect_x, verts[3].y);
        }
      else
        g_assert (angle_y == 0.0);
      if (i == CLUTTER_Z_AXIS)
        {
          g_assert (angle_z == 90.0);
          assert_coords (state, rect_x, rect_y,
                         rect_x - RECT_HEIGHT,
                         rect_y + RECT_WIDTH);
        }
      else
        g_assert (angle_z == 0.0);
      assert_vertex_and_free (center_x, 0, 0, 0);
      assert_vertex_and_free (center_y, 0, 0, 0);
      assert_vertex_and_free (center_z, 0, 0, 0);
      g_assert (z_center_gravity == CLUTTER_GRAVITY_NONE);

      g_object_set (rect, prop_name, 0.0, NULL);
      assert_notifications (NOTIFY_ROTATION_ANGLE_X << (i - CLUTTER_X_AXIS));
    }

  clutter_actor_set_position (rect, rect_x -= 10, rect_y -= 20);

  /* Same test but also change the center position */
  for (i = CLUTTER_X_AXIS; i <= CLUTTER_Z_AXIS; i++)
    {
      char prop_name[] = "rotation-angle- ";
      prop_name[sizeof (prop_name) - 2] = i - CLUTTER_X_AXIS + 'x';

      if (g_test_verbose ())
        g_print ("Setting %s to 90 degrees with center 10,20,0\n", prop_name);

      clutter_actor_set_rotation (rect, i, 90.0, 10, 20, 0);
      assert_notifications ((NOTIFY_ROTATION_ANGLE_X << (i - CLUTTER_X_AXIS))
                            | (NOTIFY_ROTATION_CENTER_X
                               << (i - CLUTTER_X_AXIS)));

      g_assert (clutter_actor_get_x (rect) == rect_x);
      g_assert (clutter_actor_get_y (rect) == rect_y);
      g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
      g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
      g_object_get (rect,
                    "rotation-angle-x", &angle_x,
                    "rotation-angle-y", &angle_y,
                    "rotation-angle-z", &angle_z,
                    "rotation-center-x", &center_x,
                    "rotation-center-y", &center_y,
                    "rotation-center-z", &center_z,
                    "rotation-center-z-gravity", &z_center_gravity,
                    NULL);
      if (i == CLUTTER_X_AXIS)
        {
          g_assert (angle_x == 90.0);
          assert_coords (state,
                         verts[0].x, rect_y + 20,
                         verts[3].x, rect_y + 20);
          assert_vertex_and_free (center_x, 10, 20, 0);
        }
      else
        {
          g_assert (angle_x == 0.0);
          assert_vertex_and_free (center_x, 0, 0, 0);
        }
      if (i == CLUTTER_Y_AXIS)
        {
          g_assert (angle_y == 90.0);
          assert_coords (state,
                         rect_x + 10, verts[0].y,
                         rect_x + 10, verts[3].y);
          assert_vertex_and_free (center_y, 10, 20, 0);
        }
      else
        {
          g_assert (angle_y == 0.0);
          assert_vertex_and_free (center_y, 0, 0, 0);
        }
      if (i == CLUTTER_Z_AXIS)
        {
          g_assert (angle_z == 90.0);
          assert_coords (state,
                         rect_x + 10 + 20,
                         rect_y + 20 - 10,
                         rect_x + 10 + 20 - RECT_HEIGHT,
                         rect_y + 20 + RECT_WIDTH - 10);
          assert_vertex_and_free (center_z, 10, 20, 0);
        }
      else
        {
          g_assert (angle_z == 0.0);
          assert_vertex_and_free (center_z, 0, 0, 0);
        }
      g_assert (z_center_gravity == CLUTTER_GRAVITY_NONE);

      clutter_actor_set_rotation (rect, i, 0, 0, 0, 0);
      assert_notifications ((NOTIFY_ROTATION_ANGLE_X << (i - CLUTTER_X_AXIS))
                            | (NOTIFY_ROTATION_CENTER_X
                               << (i - CLUTTER_X_AXIS)));

    }

  /* Try rotating the z with all of the gravities */
  for (i = 0; i < G_N_ELEMENTS (gravities); i++)
    {
      if (g_test_verbose ())
        {
          GEnumClass *gravity_class = g_type_class_ref (CLUTTER_TYPE_GRAVITY);
          GEnumValue *value = g_enum_get_value (gravity_class,
                                                gravities[i].gravity);
          g_print ("Setting z rotation to 90 degrees with center at %s\n",
                   value ? value->value_name : "?");
          g_type_class_unref (gravity_class);
        }

      clutter_actor_set_z_rotation_from_gravity (rect, 90,
                                                 gravities[i].gravity);
      assert_notifications (NOTIFY_ROTATION_ANGLE_Z
                            | NOTIFY_ROTATION_CENTER_Z
                            | NOTIFY_ROTATION_CENTER_Z_GRAVITY);

      g_assert (clutter_actor_get_x (rect) == rect_x);
      g_assert (clutter_actor_get_y (rect) == rect_y);
      g_assert (clutter_actor_get_width (rect) == RECT_WIDTH);
      g_assert (clutter_actor_get_height (rect) == RECT_HEIGHT);
      g_object_get (rect,
                    "rotation-angle-x", &angle_x,
                    "rotation-angle-y", &angle_y,
                    "rotation-angle-z", &angle_z,
                    "rotation-center-x", &center_x,
                    "rotation-center-y", &center_y,
                    "rotation-center-z", &center_z,
                    "rotation-center-z-gravity", &z_center_gravity,
                    NULL);
      g_assert (angle_x == 0.0);
      g_assert (angle_y == 0.0);
      g_assert (angle_z == 90.0);
      assert_vertex_and_free (center_x, 0, 0, 0);
      assert_vertex_and_free (center_y, 0, 0, 0);
      assert_vertex_and_free (center_z,
                              gravities[i].x_pos, gravities[i].y_pos, 0);
      assert_coords (state,
                     rect_x + gravities[i].x_pos + gravities[i].y_pos,
                     rect_y + gravities[i].y_pos - gravities[i].x_pos,
                     rect_x + gravities[i].x_pos + gravities[i].y_pos
                     - RECT_HEIGHT,
                     rect_y + gravities[i].y_pos + RECT_WIDTH
                     - gravities[i].x_pos);
      g_assert (z_center_gravity == gravities[i].gravity);
      g_assert (clutter_actor_get_z_rotation_gravity (rect)
                == gravities[i].gravity);

      /* Put the rotation back */
      clutter_actor_set_z_rotation_from_gravity (rect, 0, CLUTTER_GRAVITY_NONE);
      assert_notifications (NOTIFY_ROTATION_ANGLE_Z
                            | NOTIFY_ROTATION_CENTER_Z
                            | NOTIFY_ROTATION_CENTER_Z_GRAVITY);
    }
}

static gboolean
idle_cb (gpointer data)
{
  test_anchor_point (data);
  test_scale_center (data);
  test_rotate_center (data);

  clutter_main_quit ();

  return G_SOURCE_REMOVE;
}

static void
actor_anchors (void)
{
  TestState state;
  ClutterActor *stage;

  stage = clutter_test_get_stage ();

  state.rect = clutter_actor_new ();
  clutter_actor_add_child (stage, state.rect);
  clutter_actor_set_position (state.rect, 100, 200);
  clutter_actor_set_size (state.rect, RECT_WIDTH, RECT_HEIGHT);

  /* Record notifications on the actor properties */
  state.notifications = 0;
  g_signal_connect (state.rect, "notify",
                    G_CALLBACK (notify_cb), &state);

  /* Run the tests in a low priority idle function so that we can be
     sure the stage is correctly setup */
  clutter_threads_add_idle_full (G_PRIORITY_LOW, idle_cb, &state, NULL);

  clutter_actor_show (stage);

  clutter_main ();
}

static void
actor_pivot (void)
{
  ClutterActor *stage, *actor_implicit, *actor_explicit;
  ClutterMatrix transform, result_implicit, result_explicit;
  ClutterActorBox allocation = CLUTTER_ACTOR_BOX_INIT (0, 0, 90, 30);
  gfloat angle = 30;

  stage = clutter_test_get_stage ();

  actor_implicit = clutter_actor_new ();
  actor_explicit = clutter_actor_new ();

  clutter_actor_add_child (stage, actor_implicit);
  clutter_actor_add_child (stage, actor_explicit);

  /* Fake allocation or pivot-point will not have any effect */
  clutter_actor_allocate (actor_implicit, &allocation, CLUTTER_ALLOCATION_NONE);
  clutter_actor_allocate (actor_explicit, &allocation, CLUTTER_ALLOCATION_NONE);

  clutter_actor_set_pivot_point (actor_implicit, 0.5, 0.5);
  clutter_actor_set_pivot_point (actor_explicit, 0.5, 0.5);

  /* Implict transformation */
  clutter_actor_set_rotation_angle (actor_implicit, CLUTTER_Z_AXIS, angle);

  /* Explict transformation */
  clutter_matrix_init_identity(&transform);
  cogl_matrix_rotate (&transform, angle, 0, 0, 1.0);
  clutter_actor_set_transform (actor_explicit, &transform);

  clutter_actor_get_transform (actor_implicit, &result_implicit);
  clutter_actor_get_transform (actor_explicit, &result_explicit);

  g_assert (cogl_matrix_equal (&result_implicit, &result_explicit));
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/actor/transforms/anchor-point", actor_anchors)
  CLUTTER_TEST_UNIT ("/actor/transforms/pivot-point", actor_pivot)
)
