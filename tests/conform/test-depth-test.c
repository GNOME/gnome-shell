#define COGL_VERSION_MIN_REQUIRED COGL_VERSION_1_0

#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

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
  int padding;
} TestState;

typedef struct
{
  uint32_t               color;
  float                 depth;
  CoglBool              test_enable;
  CoglDepthTestFunction test_function;
  CoglBool              write_enable;
  CoglBool              fb_write_enable;
  float                 range_near;
  float                 range_far;
} TestDepthState;

static CoglBool
draw_rectangle (TestState *state,
                int x,
                int y,
                TestDepthState *rect_state,
                CoglBool legacy_mode)
{
  uint8_t Cr = MASK_RED (rect_state->color);
  uint8_t Cg = MASK_GREEN (rect_state->color);
  uint8_t Cb = MASK_BLUE (rect_state->color);
  uint8_t Ca = MASK_ALPHA (rect_state->color);
  CoglPipeline *pipeline;
  CoglDepthState depth_state;

  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, rect_state->test_enable);
  cogl_depth_state_set_test_function (&depth_state, rect_state->test_function);
  cogl_depth_state_set_write_enabled (&depth_state, rect_state->write_enable);
  cogl_depth_state_set_range (&depth_state,
                              rect_state->range_near,
                              rect_state->range_far);

  pipeline = cogl_pipeline_new (test_ctx);
  if (!cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL))
    {
      cogl_object_unref (pipeline);
      return FALSE;
    }

  if (!legacy_mode)
    {
      cogl_pipeline_set_color4ub (pipeline, Cr, Cg, Cb, Ca);

      cogl_framebuffer_set_depth_write_enabled (test_fb,
                                                rect_state->fb_write_enable);
      cogl_framebuffer_push_matrix (test_fb);
      cogl_framebuffer_translate (test_fb, 0, 0, rect_state->depth);
      cogl_framebuffer_draw_rectangle (test_fb,
                                       pipeline,
                                       x * QUAD_WIDTH,
                                       y * QUAD_WIDTH,
                                       x * QUAD_WIDTH + QUAD_WIDTH,
                                       y * QUAD_WIDTH + QUAD_WIDTH);
      cogl_framebuffer_pop_matrix (test_fb);
    }
  else
    {
      cogl_push_framebuffer (test_fb);
      cogl_push_matrix ();
      cogl_set_source_color4ub (Cr, Cg, Cb, Ca);
      cogl_translate (0, 0, rect_state->depth);
      cogl_rectangle (x * QUAD_WIDTH,
                      y * QUAD_WIDTH,
                      x * QUAD_WIDTH + QUAD_WIDTH,
                      y * QUAD_WIDTH + QUAD_WIDTH);
      cogl_pop_matrix ();
      cogl_pop_framebuffer ();
    }

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
            CoglBool legacy_mode,
            uint32_t expected_result)
{
  CoglBool missing_feature = FALSE;

  if (rect0_state)
    missing_feature |= !draw_rectangle (state, x, y, rect0_state, legacy_mode);
  if (rect1_state)
    missing_feature |= !draw_rectangle (state, x, y, rect1_state, legacy_mode);
  if (rect2_state)
    missing_feature |= !draw_rectangle (state, x, y, rect2_state, legacy_mode);

  /* We don't consider it an error that we can't test something
   * the driver doesn't support. */
  if (missing_feature)
    return;

  test_utils_check_pixel (test_fb,
                          x * QUAD_WIDTH + (QUAD_WIDTH / 2),
                          y * QUAD_WIDTH + (QUAD_WIDTH / 2),
                          expected_result);
}

static void
paint (TestState *state)
{
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
      TRUE, /* FB depth write enable */
      0, 1 /* depth range */
    };
    /* Furthest */
    TestDepthState rect1_state = {
      0x00ff00ff, /* rgba color */
      -70, /* depth */
      TRUE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_ALWAYS,
      TRUE, /* depth write enable */
      TRUE, /* FB depth write enable */
      0, 1 /* depth range */
    };
    /* In the middle */
    TestDepthState rect2_state = {
      0x0000ffff, /* rgba color */
      -20, /* depth */
      TRUE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_NEVER,
      TRUE, /* depth write enable */
      TRUE, /* FB depth write enable */
      0, 1 /* depth range */
    };

    test_depth (state, 0, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                FALSE, /* legacy mode */
                0x00ff00ff); /* expected */

    rect2_state.test_function = COGL_DEPTH_TEST_FUNCTION_ALWAYS;
    test_depth (state, 1, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                FALSE, /* legacy mode */
                0x0000ffff); /* expected */

    rect2_state.test_function = COGL_DEPTH_TEST_FUNCTION_LESS;
    test_depth (state, 2, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                FALSE, /* legacy mode */
                0x0000ffff); /* expected */

    rect2_state.test_function = COGL_DEPTH_TEST_FUNCTION_GREATER;
    test_depth (state, 3, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                FALSE, /* legacy mode */
                0x00ff00ff); /* expected */

    rect0_state.test_enable = TRUE;
    rect1_state.write_enable = FALSE;
    test_depth (state, 4, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                FALSE, /* legacy mode */
                0x0000ffff); /* expected */

    rect1_state.write_enable = TRUE;
    rect1_state.fb_write_enable = FALSE;
    test_depth (state, 4, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                FALSE, /* legacy mode */
                0x0000ffff); /* expected */

    /* Re-enable FB depth writing to verify state flush */
    rect1_state.write_enable = TRUE;
    rect1_state.fb_write_enable = TRUE;
    test_depth (state, 4, 0, /* position */
                &rect0_state, &rect1_state, &rect2_state,
                FALSE, /* legacy mode */
                0x00ff00ff); /* expected */
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
      TRUE, /* FB depth write enable */
      0.5, 1 /* depth range */
    };
    /* Furthest by depth, nearest by depth range */
    TestDepthState rect1_state = {
      0x00ff00ff, /* rgba color */
      -70, /* depth */
      TRUE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_GREATER,
      TRUE, /* depth write enable */
      TRUE, /* FB depth write enable */
      0, 0.5 /* depth range */
    };

    test_depth (state, 0, 1, /* position */
                &rect0_state, &rect1_state, NULL,
                FALSE, /* legacy mode */
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
      TRUE, /* FB depth write enable */
      0, 1 /* depth range */
    };
    /* Furthest */
    TestDepthState rect1_state = {
      0x00ff00ff, /* rgba color */
      -70, /* depth */
      FALSE, /* depth test enable */
      COGL_DEPTH_TEST_FUNCTION_LESS,
      TRUE, /* depth write enable */
      TRUE, /* FB depth write enable */
      0, 1 /* depth range */
    };

    cogl_set_depth_test_enabled (TRUE);
    test_depth (state, 0, 2, /* position */
                &rect0_state, &rect1_state, NULL,
                TRUE, /* legacy mode */
                0xff0000ff); /* expected */
    cogl_set_depth_test_enabled (FALSE);
    test_depth (state, 1, 2, /* position */
                &rect0_state, &rect1_state, NULL,
                TRUE, /* legacy mode */
                0x00ff00ff); /* expected */
  }
}

void
test_depth_test (void)
{
  TestState state;

  cogl_framebuffer_orthographic (test_fb, 0, 0,
                                 cogl_framebuffer_get_width (test_fb),
                                 cogl_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint (&state);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

