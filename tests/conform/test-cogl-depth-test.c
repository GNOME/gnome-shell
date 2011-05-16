
#include <clutter/clutter.h>

#ifndef COGL_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API
#endif
#include <cogl/cogl.h>

#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

#define QUAD_WIDTH 20

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24)
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16)
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8)
#define MASK_ALPHA(COLOR) (COLOR & 0xff)

typedef struct _TestState
{
  ClutterGeometry stage_geom;
} TestState;

typedef struct
{
  guint32               color;
  float                 depth;
  gboolean              test_enable;
  CoglDepthTestFunction test_function;
  gboolean              write_enable;
  float                 range_near;
  float                 range_far;
} TestDepthState;

static void
check_pixel (GLubyte *pixel, guint32 color)
{
  guint8 r = MASK_RED (color);
  guint8 g = MASK_GREEN (color);
  guint8 b = MASK_BLUE (color);
  guint8 a = MASK_ALPHA (color);

  if (g_test_verbose ())
    g_print ("  expected = %x, %x, %x, %x\n",
             r, g, b, a);
  /* FIXME - allow for hardware in-precision */
  g_assert_cmpint (pixel[RED], ==, r);
  g_assert_cmpint (pixel[GREEN], ==, g);
  g_assert_cmpint (pixel[BLUE], ==, b);

  /* FIXME
   * We ignore the alpha, since we don't know if our render target is
   * RGB or RGBA */
  /* g_assert (pixel[ALPHA] == a); */
}

static gboolean
draw_rectangle (TestState *state,
                int x,
                int y,
                TestDepthState *rect_state)
{
  CoglDepthState depth_state;
  guint8 Cr = MASK_RED (rect_state->color);
  guint8 Cg = MASK_GREEN (rect_state->color);
  guint8 Cb = MASK_BLUE (rect_state->color);
  guint8 Ca = MASK_ALPHA (rect_state->color);
  CoglPipeline *pipeline;

  pipeline = cogl_pipeline_new ();
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, rect_state->test_enable);
  cogl_depth_state_set_test_function (&depth_state, rect_state->test_function);
  cogl_depth_state_set_write_enabled (&depth_state, rect_state->write_enable);
  cogl_depth_state_set_range (&depth_state,
                              rect_state->range_near,
                              rect_state->range_far);
  if (!cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL))
    {
      cogl_object_unref (pipeline);
      return FALSE;
    }

  cogl_pipeline_set_color4ub (pipeline, Cr, Cg, Cb, Ca);

  cogl_set_source (pipeline);

  cogl_push_matrix ();
  cogl_translate (0, 0, rect_state->depth);
  cogl_rectangle (x * QUAD_WIDTH,
                  y * QUAD_WIDTH,
                  x * QUAD_WIDTH + QUAD_WIDTH,
                  y * QUAD_WIDTH + QUAD_WIDTH);
  cogl_pop_matrix ();

  cogl_object_unref (pipeline);

  return TRUE;
}

static void
test_depth (TestState *state,
            int x,
            int y,
            TestDepthState *rect0_state,
            TestDepthState *rect1_state,
            TestDepthState *rect2_state,
            guint32 expected_result)
{
  GLubyte pixel[4];
  GLint y_off;
  GLint x_off;
  gboolean missing_feature = FALSE;

  if (rect0_state)
    missing_feature |= !draw_rectangle (state, x, y, rect0_state);
  if (rect1_state)
    missing_feature |= !draw_rectangle (state, x, y, rect1_state);
  if (rect2_state)
    missing_feature |= !draw_rectangle (state, x, y, rect2_state);

  /* We don't consider it an error that we can't test something
   * the driver doesn't support. */
  if (missing_feature)
    return;

  /* See what we got... */

  y_off = y * QUAD_WIDTH + (QUAD_WIDTH / 2);
  x_off = x * QUAD_WIDTH + (QUAD_WIDTH / 2);

  cogl_read_pixels (x_off, y_off, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    pixel);

  check_pixel (pixel, expected_result);
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  CoglMatrix projection_save;
  CoglMatrix identity;

  /* We don't want the effects of perspective division to interfere
   * with the positions of our test rectangles on the x and y axis
   * so we use an orthographic projection...
   */

  cogl_get_projection_matrix (&projection_save);

  cogl_ortho (0, state->stage_geom.width, /* left, right */
              state->stage_geom.height, 0, /* bottom, top */
              -1, 100 /* z near, far */);

  cogl_push_matrix ();
  cogl_matrix_init_identity (&identity);
  cogl_set_modelview_matrix (&identity);

  /* Sanity check a few of the different depth test functions
   * and that depth writing can be disabled... */

  {
    /* Closest */
    TestDepthState rect0_state = {
      0xff0000ff, /* rgba color */
      -10, /* depth */
      FALSE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_ALWAYS,
      TRUE, /* depth write enable */
      0, 1 /* depth range */
    };
    /* Furthest */
    TestDepthState rect1_state = {
      0x00ff00ff, /* rgba color */
      -70, /* depth */
      TRUE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_ALWAYS,
      TRUE, /* depth write enable */
      0, 1 /* depth range */
    };
    /* In the middle */
    TestDepthState rect2_state = {
      0x0000ffff, /* rgba color */
      -20, /* depth */
      TRUE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_NEVER,
      TRUE, /* depth write enable */
      0, 1 /* depth range */
    };

    test_depth (state, 0, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x00ff00ff); /* expected */

    rect2_state.test_function = COGL_DEPTH_TEST_FUNCTION_ALWAYS;
    test_depth (state, 1, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x0000ffff); /* expected */

    rect2_state.test_function = COGL_DEPTH_TEST_FUNCTION_LESS;
    test_depth (state, 2, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x0000ffff); /* expected */

    rect2_state.test_function = COGL_DEPTH_TEST_FUNCTION_GREATER;
    test_depth (state, 3, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x00ff00ff); /* expected */

    rect0_state.test_enable = TRUE;
    rect1_state.write_enable = FALSE;
    test_depth (state, 4, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                0x0000ffff); /* expected */
  }

  /* Check that the depth buffer values can be mapped into different
   * ranges... */

  {
    /* Closest by depth, furthest by depth range */
    TestDepthState rect0_state = {
      0xff0000ff, /* rgba color */
      -10, /* depth */
      TRUE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_ALWAYS,
      TRUE, /* depth write enable */
      0.5, 1 /* depth range */
    };
    /* Furthest by depth, nearest by depth range */
    TestDepthState rect1_state = {
      0x00ff00ff, /* rgba color */
      -70, /* depth */
      TRUE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_GREATER,
      TRUE, /* depth write enable */
      0, 0.5 /* depth range */
    };

    test_depth (state, 0, 1, /* position */
                &rect0_state, &rect1_state, NULL,
                0xff0000ff); /* expected */
  }

  /* Test that the legacy cogl_set_depth_test_enabled() API still
   * works... */

  {
    /* Nearest */
    TestDepthState rect0_state = {
      0xff0000ff, /* rgba color */
      -10, /* depth */
      FALSE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_LESS,
      TRUE, /* depth write enable */
      0, 1 /* depth range */
    };
    /* Furthest */
    TestDepthState rect1_state = {
      0x00ff00ff, /* rgba color */
      -70, /* depth */
      FALSE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_LESS,
      TRUE, /* depth write enable */
      0, 1 /* depth range */
    };

    cogl_set_depth_test_enabled (TRUE);
    test_depth (state, 0, 2, /* position */
                &rect0_state, &rect1_state, NULL,
                0xff0000ff); /* expected */
    cogl_set_depth_test_enabled (FALSE);
    test_depth (state, 1, 2, /* position */
                &rect0_state, &rect1_state, NULL,
                0x00ff00ff); /* expected */
  }

  cogl_pop_matrix ();
  cogl_set_projection_matrix (&projection_save);

  clutter_main_quit ();
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_cogl_depth_test (TestConformSimpleFixture *fixture,
                      gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterActor *group;
  guint idle_source;

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_get_geometry (stage, &state.stage_geom);

  group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing incase someone comments out the
   * clutter_main_quit and wants visual feedback for the test since we
   * wont be doing anything else that will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (stage);

  clutter_main ();

  g_source_remove (idle_source);

  if (g_test_verbose ())
    g_print ("OK\n");
}

