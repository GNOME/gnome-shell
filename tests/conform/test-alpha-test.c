#include <cogl/cogl.h>
#include <string.h>

#include "test-utils.h"

static CoglTexture2D *
create_texture (CoglContext *context)
{
  static const uint8_t data[] =
    {
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xfa, 0x00, 0xfa
    };

  return cogl_texture_2d_new_from_data (context,
                                        2, 1, /* width/height */
                                        COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                        4, /* rowstride */
                                        data,
                                        NULL /* error */);
}

void
test_alpha_test (void)
{
  CoglTexture *tex = create_texture (test_ctx);
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);
  int fb_width = cogl_framebuffer_get_width (test_fb);
  int fb_height = cogl_framebuffer_get_height (test_fb);
  CoglColor clear_color;

  cogl_pipeline_set_layer_texture (pipeline, 0, tex);
  cogl_pipeline_set_layer_filters (pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);
  cogl_pipeline_set_alpha_test_function (pipeline,
                                         COGL_PIPELINE_ALPHA_FUNC_GEQUAL,
                                         254 / 255.0f /* alpha reference */);

  cogl_color_init_from_4ub (&clear_color, 0x00, 0x00, 0xff, 0xff);
  cogl_framebuffer_clear (test_fb,
                          COGL_BUFFER_BIT_COLOR,
                          &clear_color);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1, -1,
                                   1, 1);

  cogl_object_unref (pipeline);
  cogl_object_unref (tex);

  /* The left side of the framebuffer should use the first pixel from
   * the texture which is red */
  test_utils_check_region (test_fb,
                           2, 2,
                           fb_width / 2 - 4,
                           fb_height - 4,
                           0xff0000ff);
  /* The right side of the framebuffer should use the clear color
   * because the second pixel from the texture is clipped from the
   * alpha test */
  test_utils_check_region (test_fb,
                           fb_width / 2 + 2,
                           2,
                           fb_width / 2 - 4,
                           fb_height - 4,
                           0x0000ffff);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

