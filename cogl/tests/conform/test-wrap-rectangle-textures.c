#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

#define DRAW_SIZE 64

static CoglPipeline *
create_base_pipeline (void)
{
  CoglBitmap *bmp;
  CoglTextureRectangle *tex;
  CoglPipeline *pipeline;
  uint8_t tex_data[] =
    {
      0x44, 0x44, 0x44, 0x88, 0x88, 0x88,
      0xcc, 0xcc, 0xcc, 0xff, 0xff, 0xff
    };

  bmp = cogl_bitmap_new_for_data (test_ctx,
                                  2, 2, /* width/height */
                                  COGL_PIXEL_FORMAT_RGB_888,
                                  2 * 3, /* rowstride */
                                  tex_data);

  tex = cogl_texture_rectangle_new_from_bitmap (bmp);

  cogl_object_unref (bmp);

  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_layer_filters (pipeline,
                                   0, /* layer */
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  cogl_pipeline_set_layer_texture (pipeline,
                                   0, /* layer */
                                   tex);

  cogl_object_unref (tex);

  return pipeline;
}

static void
check_colors (int x_offset,
              int y_offset,
              const uint8_t expected_colors[9])
{
  int x, y;

  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      {
        uint32_t color = expected_colors[x + y * 4];
        test_utils_check_region (test_fb,
                                 x * DRAW_SIZE / 4 + 1 + x_offset,
                                 y * DRAW_SIZE / 4 + 1 + y_offset,
                                 DRAW_SIZE / 4 - 2,
                                 DRAW_SIZE / 4 - 2,
                                 0xff |
                                 (color << 8) |
                                 (color << 16) |
                                 (color << 24));
      }
}

static void
test_pipeline (CoglPipeline *pipeline,
               int x_offset,
               int y_offset,
               const uint8_t expected_colors[9])
{
  float x1 = x_offset;
  float y1 = y_offset;
  float x2 = x1 + DRAW_SIZE;
  float y2 = y1 + DRAW_SIZE;
  int y, x;

  cogl_framebuffer_draw_textured_rectangle (test_fb,
                                            pipeline,
                                            x1, y1,
                                            x2, y2,
                                            -0.5f, /* s1 */
                                            -0.5f, /* t1 */
                                            1.5f, /* s2 */
                                            1.5f /* t2 */);

  check_colors (x_offset, y_offset, expected_colors);

  /* Also try drawing each quadrant of the rectangle with a small
   * rectangle */

  for (y = -1; y < 3; y++)
    for (x = -1; x < 3; x++)
      {
        x1 = x_offset + (x + 1) * DRAW_SIZE / 4 + DRAW_SIZE;
        y1 = y_offset + (y + 1) * DRAW_SIZE / 4;
        x2 = x1 + DRAW_SIZE / 4;
        y2 = y1 + DRAW_SIZE / 4;

        cogl_framebuffer_draw_textured_rectangle (test_fb,
                                                  pipeline,
                                                  x1, y1,
                                                  x2, y2,
                                                  x / 2.0f, /* s1 */
                                                  y / 2.0f, /* t1 */
                                                  (x + 1) / 2.0f, /* s2 */
                                                  (y + 1) / 2.0f /* t2 */);
      }

  check_colors (x_offset + DRAW_SIZE, y_offset, expected_colors);
}

void
test_wrap_rectangle_textures (void)
{
  float fb_width = cogl_framebuffer_get_width (test_fb);
  float fb_height = cogl_framebuffer_get_height (test_fb);
  CoglPipeline *base_pipeline;
  CoglPipeline *clamp_pipeline;
  CoglPipeline *repeat_pipeline;
  /* The textures are drawn with the texture coordinates from
   * -0.5→1.5. That means we get one complete copy of the texture and
   * an extra half of the texture surrounding it. The drawing is
   * tested against a 4x4 grid of colors. The center 2x2 colours
   * specify the normal texture colors and the other colours specify
   * what the wrap mode should generate */
  static const uint8_t clamp_colors[] =
    {
      0x44, 0x44, 0x88, 0x88,
      0x44, 0x44, 0x88, 0x88,
      0xcc, 0xcc, 0xff, 0xff,
      0xcc, 0xcc, 0xff, 0xff
    };
  static const uint8_t repeat_colors[] =
    {
      0xff, 0xcc, 0xff, 0xcc,
      0x88, 0x44, 0x88, 0x44,
      0xff, 0xcc, 0xff, 0xcc,
      0x88, 0x44, 0x88, 0x44
    };

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0, /* x_1, y_1 */
                                 fb_width, /* x_2 */
                                 fb_height /* y_2 */,
                                 -1, 100 /* near/far */);

  base_pipeline = create_base_pipeline ();

  clamp_pipeline = cogl_pipeline_copy (base_pipeline);
  cogl_pipeline_set_layer_wrap_mode (clamp_pipeline,
                                     0, /* layer */
                                     COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

  repeat_pipeline = cogl_pipeline_copy (base_pipeline);
  cogl_pipeline_set_layer_wrap_mode (repeat_pipeline,
                                     0, /* layer */
                                     COGL_PIPELINE_WRAP_MODE_REPEAT);

  test_pipeline (clamp_pipeline,
                 0, 0, /* x/y offset */
                 clamp_colors);

  test_pipeline (repeat_pipeline,
                 0, DRAW_SIZE * 2, /* x/y offset */
                 repeat_colors);

  cogl_object_unref (repeat_pipeline);
  cogl_object_unref (clamp_pipeline);
  cogl_object_unref (base_pipeline);
}
