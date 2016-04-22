#include <cogl/cogl.h>
#include <string.h>

#include "test-utils.h"

#define BITMAP_SIZE 256

/*
 * Creates a 256 x 256 with image data split into four quadrants. The
 * colours of these in reading order will be: blue, green, cyan,
 * red */
static void
generate_bitmap_data (uint8_t *data,
                      int stride)
{
  int y, x;

  for (y = 0; y < BITMAP_SIZE; y++)
    {
      for (x = 0; x < BITMAP_SIZE; x++)
        {
          int color_num = x / (BITMAP_SIZE / 2) + y / (BITMAP_SIZE / 2) * 2 + 1;
          *(data++) = (color_num & 4) ? 255 : 0;
          *(data++) = (color_num & 2) ? 255 : 0;
          *(data++) = (color_num & 1) ? 255 : 0;
          *(data++) = 255;
        }
      data += stride - BITMAP_SIZE * 4;
    }
}

static CoglBitmap *
create_bitmap (void)
{
  CoglBitmap *bitmap;
  CoglBuffer *buffer;

  bitmap = cogl_bitmap_new_with_size (test_ctx,
                                      BITMAP_SIZE,
                                      BITMAP_SIZE,
                                      COGL_PIXEL_FORMAT_RGBA_8888);
  buffer = cogl_bitmap_get_buffer (bitmap);

  g_assert (cogl_is_pixel_buffer (buffer));
  g_assert (cogl_is_buffer (buffer));

  cogl_buffer_set_update_hint (buffer, COGL_BUFFER_UPDATE_HINT_DYNAMIC);
  g_assert_cmpint (cogl_buffer_get_update_hint (buffer),
                   ==,
                   COGL_BUFFER_UPDATE_HINT_DYNAMIC);

  return bitmap;
}

static CoglBitmap *
create_and_fill_bitmap (void)
{
  CoglBitmap *bitmap = create_bitmap ();
  CoglBuffer *buffer = cogl_bitmap_get_buffer (bitmap);
  uint8_t *map;
  unsigned int stride;

  stride = cogl_bitmap_get_rowstride (bitmap);

  map = cogl_buffer_map (buffer,
                         COGL_BUFFER_ACCESS_WRITE,
                         COGL_BUFFER_MAP_HINT_DISCARD);
  g_assert (map);

  generate_bitmap_data (map, stride);

  cogl_buffer_unmap (buffer);

  return bitmap;
}

static CoglTexture *
create_texture_from_bitmap (CoglBitmap *bitmap)
{
  CoglTexture2D *texture;

  texture = cogl_texture_2d_new_from_bitmap (bitmap);

  g_assert (texture != NULL);

  return texture;
}

static CoglPipeline *
create_pipeline_from_texture (CoglTexture *texture)
{
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_layer_texture (pipeline, 0, texture);
  cogl_pipeline_set_layer_filters (pipeline,
                                   0, /* layer_num */
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  return pipeline;
}

static void
check_colours (uint32_t color0,
               uint32_t color1,
               uint32_t color2,
               uint32_t color3)
{
  int fb_width = cogl_framebuffer_get_width (test_fb);
  int fb_height = cogl_framebuffer_get_height (test_fb);

  test_utils_check_region (test_fb,
                           1, 1, /* x/y */
                           fb_width / 2 - 2, /* width */
                           fb_height / 2 - 2, /* height */
                           color0);
  test_utils_check_region (test_fb,
                           fb_width / 2 + 1, /* x */
                           1, /* y */
                           fb_width / 2 - 2, /* width */
                           fb_height / 2 - 2, /* height */
                           color1);
  test_utils_check_region (test_fb,
                           1, /* x */
                           fb_height / 2 + 1, /* y */
                           fb_width / 2 - 2, /* width */
                           fb_height / 2 - 2, /* height */
                           color2);
  test_utils_check_region (test_fb,
                           fb_width / 2 + 1, /* x */
                           fb_height / 2 + 1, /* y */
                           fb_width / 2 - 2, /* width */
                           fb_height / 2 - 2, /* height */
                           color3);
}

void
test_pixel_buffer_map (void)
{
  CoglBitmap *bitmap = create_and_fill_bitmap ();
  CoglPipeline *pipeline;
  CoglTexture *texture;

  texture = create_texture_from_bitmap (bitmap);
  pipeline = create_pipeline_from_texture (texture);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1.0f, 1.0f,
                                   1.0f, -1.0f);

  cogl_object_unref (bitmap);
  cogl_object_unref (texture);
  cogl_object_unref (pipeline);

  check_colours (0x0000ffff,
                 0x00ff00ff,
                 0x00ffffff,
                 0xff0000ff);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

void
test_pixel_buffer_set_data (void)
{
  CoglBitmap *bitmap = create_bitmap ();
  CoglBuffer *buffer = cogl_bitmap_get_buffer (bitmap);
  CoglPipeline *pipeline;
  CoglTexture *texture;
  uint8_t *data;
  unsigned int stride;

  stride = cogl_bitmap_get_rowstride (bitmap);

  data = g_malloc (stride * BITMAP_SIZE);

  generate_bitmap_data (data, stride);

  cogl_buffer_set_data (buffer,
                        0, /* offset */
                        data,
                        stride * (BITMAP_SIZE - 1) +
                        BITMAP_SIZE * 4);

  g_free (data);

  texture = create_texture_from_bitmap (bitmap);
  pipeline = create_pipeline_from_texture (texture);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1.0f, 1.0f,
                                   1.0f, -1.0f);

  cogl_object_unref (bitmap);
  cogl_object_unref (texture);
  cogl_object_unref (pipeline);

  check_colours (0x0000ffff,
                 0x00ff00ff,
                 0x00ffffff,
                 0xff0000ff);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

static CoglTexture *
create_white_texture (void)
{
  CoglTexture2D *texture;
  uint8_t *data = g_malloc (BITMAP_SIZE * BITMAP_SIZE * 4);

  memset (data, 255, BITMAP_SIZE * BITMAP_SIZE * 4);

  texture = cogl_texture_2d_new_from_data (test_ctx,
                                           BITMAP_SIZE,
                                           BITMAP_SIZE,
                                           COGL_PIXEL_FORMAT_RGBA_8888,
                                           BITMAP_SIZE * 4, /* rowstride */
                                           data,
                                           NULL); /* don't catch errors */

  g_free (data);

  return texture;
}

void
test_pixel_buffer_sub_region (void)
{
  CoglBitmap *bitmap = create_and_fill_bitmap ();
  CoglPipeline *pipeline;
  CoglTexture *texture;

  texture = create_white_texture ();

  /* Replace the top-right quadrant of the texture with the red part
   * of the bitmap */
  cogl_texture_set_region_from_bitmap (texture,
                                       BITMAP_SIZE / 2, /* src_x */
                                       BITMAP_SIZE / 2, /* src_y */
                                       BITMAP_SIZE / 2, /* dst_x */
                                       0, /* dst_y */
                                       BITMAP_SIZE / 2, /* width */
                                       BITMAP_SIZE / 2, /* height */
                                       bitmap);

  pipeline = create_pipeline_from_texture (texture);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1.0f, 1.0f,
                                   1.0f, -1.0f);

  cogl_object_unref (bitmap);
  cogl_object_unref (texture);
  cogl_object_unref (pipeline);

  check_colours (0xffffffff,
                 0xff0000ff,
                 0xffffffff,
                 0xffffffff);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
