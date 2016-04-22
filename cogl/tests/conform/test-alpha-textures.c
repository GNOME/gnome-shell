#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

static void
create_pipeline (CoglTexture **tex_out,
                 CoglPipeline **pipeline_out)
{
  CoglTexture2D *tex;
  CoglPipeline *pipeline;
  static const uint8_t tex_data[] =
    { 0x00, 0x44, 0x88, 0xcc };

  tex = cogl_texture_2d_new_from_data (test_ctx,
                                       2, 2, /* width/height */
                                       COGL_PIXEL_FORMAT_A_8, /* format */
                                       2, /* rowstride */
                                       tex_data,
                                       NULL);

  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_layer_filters (pipeline,
                                   0, /* layer */
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);
  cogl_pipeline_set_layer_wrap_mode (pipeline,
                                     0, /* layer */
                                     COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

  /* This is the layer combine used by cogl-pango */
  cogl_pipeline_set_layer_combine (pipeline,
                                   0, /* layer */
                                   "RGBA = MODULATE (PREVIOUS, TEXTURE[A])",
                                   NULL);

  cogl_pipeline_set_layer_texture (pipeline,
                                   0, /* layer */
                                   tex);

  *pipeline_out = pipeline;
  *tex_out = tex;
}

void
test_alpha_textures (void)
{
  CoglTexture *tex1, *tex2;
  CoglPipeline *pipeline1, *pipeline2;
  int fb_width = cogl_framebuffer_get_width (test_fb);
  int fb_height = cogl_framebuffer_get_height (test_fb);
  uint8_t replacement_data[1] = { 0xff };

  create_pipeline (&tex1, &pipeline1);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline1,
                                   -1.0f, 1.0f, /* x1/y1 */
                                   1.0f, 0.0f /* x2/y2 */);

  create_pipeline (&tex2, &pipeline2);

  cogl_texture_set_region (tex2,
                           0, 0, /* src_x/y */
                           1, 1, /* dst_x/y */
                           1, 1, /* dst_width / dst_height */
                           1, 1, /* width / height */
                           COGL_PIXEL_FORMAT_A_8,
                           1, /* rowstride */
                           replacement_data);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline2,
                                   -1.0f, 0.0f, /* x1/y1 */
                                   1.0f, -1.0f /* x2/y2 */);

  cogl_object_unref (tex1);
  cogl_object_unref (tex2);
  cogl_object_unref (pipeline1);
  cogl_object_unref (pipeline2);

  /* Unmodified texture */
  test_utils_check_pixel (test_fb,
                          fb_width / 4,
                          fb_height / 8,
                          0x000000ff);
  test_utils_check_pixel (test_fb,
                          fb_width * 3 / 4,
                          fb_height / 8,
                          0x444444ff);
  test_utils_check_pixel (test_fb,
                          fb_width / 4,
                          fb_height * 3 / 8,
                          0x888888ff);
  test_utils_check_pixel (test_fb,
                          fb_width * 3 / 4,
                          fb_height * 3 / 8,
                          0xccccccff);

  /* Modified texture */
  test_utils_check_pixel (test_fb,
                          fb_width / 4,
                          fb_height * 5 / 8,
                          0x000000ff);
  test_utils_check_pixel (test_fb,
                          fb_width * 3 / 4,
                          fb_height * 5 / 8,
                          0x444444ff);
  test_utils_check_pixel (test_fb,
                          fb_width / 4,
                          fb_height * 7 / 8,
                          0x888888ff);
  test_utils_check_pixel (test_fb,
                          fb_width * 3 / 4,
                          fb_height * 7 / 8,
                          0xffffffff);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

