#include <cogl/cogl2-experimental.h>
#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  int fb_width;
  int fb_height;
} TestState;

static void
test_sparse_layer_combine (TestState *state)
{
  CoglPipeline *pipeline;
  CoglTexture *tex1, *tex2;

  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  /* This tests that the TEXTURE_* numbers used in the layer combine
     string refer to the layer number rather than the unit numbers by
     creating a pipeline with very large layer numbers. This should
     end up being mapped to much smaller unit numbers */

  tex1 = test_utils_create_color_texture (ctx, 0xff0000ff);
  tex2 = test_utils_create_color_texture (ctx, 0x00ff00ff);

  pipeline = cogl_pipeline_new (ctx);

  cogl_pipeline_set_layer_texture (pipeline, 50, tex1);
  cogl_pipeline_set_layer_texture (pipeline, 100, tex2);
  cogl_pipeline_set_layer_combine (pipeline, 200,
                                   "RGBA = ADD(TEXTURE_50, TEXTURE_100)",
                                   NULL);

  cogl_framebuffer_draw_rectangle (fb, pipeline, -1, -1, 1, 1);

  test_utils_check_pixel (fb, 2, 2, 0xffff00ff);

  cogl_object_unref (pipeline);
  cogl_object_unref (tex1);
  cogl_object_unref (tex2);
}

void
test_sparse_pipeline (void)
{
  TestState state;

  state.fb_width = cogl_framebuffer_get_width (fb);
  state.fb_height = cogl_framebuffer_get_height (fb);

  test_sparse_layer_combine (&state);

  /* FIXME: This should have a lot more tests, for example testing
     whether using an attribute with sparse texture coordinates will
     work */

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

