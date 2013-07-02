#include <math.h>
#include <clutter/clutter.h>
#include "test-conform-common.h"

typedef struct _TestState       TestState;

struct _TestState
{
  GPtrArray *actors;
  GPtrArray *colors;

  ClutterActor *stage;

  gulong id;

  gint in_validation;

  guint was_painted : 1;
};

static TestState *
test_state_new (void)
{
  return g_slice_new0 (TestState);
}

static void
test_state_free (TestState *state)
{
  if (state->id != 0)
    g_source_remove (state->id);

  if (state->actors != NULL)
    g_ptr_array_unref (state->actors);

  if (state->colors != NULL)
    g_ptr_array_unref (state->colors);

  if (state->stage != NULL)
    clutter_actor_destroy (state->stage);

  g_slice_free (TestState, state);
}

static void
test_state_set_stage (TestState    *state,
                      ClutterActor *stage)
{
  g_assert (!state->was_painted);

  state->stage = stage;
}

static void
test_state_add_actor (TestState          *state,
                      ClutterActor       *actor,
                      const ClutterColor *color)
{
  g_assert (!state->was_painted);

  if (state->actors == NULL)
    {
      state->actors = g_ptr_array_new ();
      g_ptr_array_set_free_func (state->actors,
                                 (GDestroyNotify) g_object_unref);
    }

  g_ptr_array_add (state->actors, g_object_ref (actor));

  if (state->colors == NULL)
    {
      state->colors = g_ptr_array_new ();
      g_ptr_array_set_free_func (state->colors,
                                 (GDestroyNotify) clutter_color_free);
    }

  g_ptr_array_add (state->colors, clutter_color_copy (color));
}

static void
test_state_push_validation (TestState *state)
{
  state->in_validation += 1;
}

static void
test_state_pop_validation (TestState *state)
{
  state->in_validation -= 1;
}

static gboolean
test_state_in_validation (TestState *state)
{
  return state->in_validation > 0;
}

static gboolean
check_color_at (ClutterActor *stage,
                ClutterActor *actor,
                ClutterColor *expected_color,
                float         x,
                float         y)
{
  guchar *buffer;

  if (g_test_verbose ())
    g_print ("Checking actor '%s'\n", clutter_actor_get_name (actor));

  if (g_test_verbose ())
    g_print ("Sampling at { %d, %d }\t", (int) x, (int) y);

  buffer = clutter_stage_read_pixels (CLUTTER_STAGE (stage), x, y, 1, 1);
  g_assert (buffer != NULL);

  if (g_test_verbose ())
    g_print ("Color: { %d, %d, %d } - Expected color { %d, %d, %d }\n",
             buffer[0],
             buffer[1],
             buffer[2],
             expected_color->red,
             expected_color->green,
             expected_color->blue);

  g_assert_cmpint (buffer[0], ==, expected_color->red);
  g_assert_cmpint (buffer[1], ==, expected_color->green);
  g_assert_cmpint (buffer[2], ==, expected_color->blue);

  g_free (buffer);

  return TRUE;
}

static gboolean
validate_state (gpointer data)
{
  TestState *state = data;
  int i;

  /* avoid recursion */
  if (test_state_in_validation (state))
    return;

  g_assert (state->actors != NULL);
  g_assert (state->colors != NULL);

  g_assert_cmpint (state->actors->len, ==, state->colors->len);

  test_state_push_validation (state);

  if (g_test_verbose ())
    g_print ("Sampling %d actors\n", state->actors->len);

  for (i = 0; i < state->actors->len; i++)
    {
      ClutterActor *actor = g_ptr_array_index (state->actors, i);
      ClutterColor *color = g_ptr_array_index (state->colors, i);
      ClutterActorBox box;

      clutter_actor_get_allocation_box (actor, &box);

      check_color_at (state->stage, actor, color, box.x1 + 2, box.y1 + 2);
      check_color_at (state->stage, actor, color, box.x2 - 2, box.y2 - 2);
    }

  test_state_pop_validation (state);

  state->was_painted = TRUE;

  return G_SOURCE_REMOVE;
}

static gboolean
test_state_run (TestState *state)
{
  clutter_threads_add_repaint_func_full (CLUTTER_REPAINT_FLAGS_POST_PAINT,
                                         validate_state,
                                         state,
                                         NULL);

  while (!state->was_painted)
    g_main_context_iteration (NULL, FALSE);

  return TRUE;
}

void
actor_basic_layout (TestConformSimpleFixture *fixture,
                    gconstpointer data)
{
  ClutterActor *stage = clutter_stage_new ();
  ClutterActor *vase;
  ClutterActor *flower[3];
  TestState *state;

  vase = clutter_actor_new ();
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

  clutter_actor_show_all (stage);

  state = test_state_new ();
  test_state_set_stage (state, stage);
  test_state_add_actor (state, flower[0], CLUTTER_COLOR_Red);
  test_state_add_actor (state, flower[1], CLUTTER_COLOR_Yellow);
  test_state_add_actor (state, flower[2], CLUTTER_COLOR_Green);

  g_assert (test_state_run (state));

  test_state_free (state);
}

void
actor_margin_layout (TestConformSimpleFixture *fixture,
                     gconstpointer data)
{
  ClutterActor *stage = clutter_stage_new ();
  ClutterActor *vase;
  ClutterActor *flower[3];
  TestState *state;

  vase = clutter_actor_new ();
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

  clutter_actor_show_all (stage);

  state = test_state_new ();
  test_state_set_stage (state, stage);
  test_state_add_actor (state, flower[0], CLUTTER_COLOR_Red);
  test_state_add_actor (state, flower[1], CLUTTER_COLOR_Yellow);
  test_state_add_actor (state, flower[2], CLUTTER_COLOR_Green);

  g_assert (test_state_run (state));

  test_state_free (state);
}
