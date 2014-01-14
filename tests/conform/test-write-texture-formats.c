#include <cogl/cogl2-experimental.h>
#include <stdarg.h>

#include "test-utils.h"

/*
 * This tests writing data to an RGBA texture in all of the available
 * pixel formats
 */

static void
test_color (CoglTexture *texture,
            uint32_t expected_pixel)
{
  uint8_t received_pixel[4];

  cogl_texture_get_data (texture,
                         COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                         4, /* rowstride */
                         received_pixel);

  test_utils_compare_pixel_and_alpha (received_pixel, expected_pixel);
}

static void
test_write_byte (CoglContext *context,
                 CoglPixelFormat format,
                 uint8_t byte,
                 uint32_t expected_pixel)
{
  CoglTexture *texture = test_utils_create_color_texture (context, 0);

  cogl_texture_set_region (texture,
                           0, 0, /* src_x / src_y */
                           0, 0, /* dst_x / dst_y */
                           1, 1, /* dst_w / dst_h */
                           1, 1, /* width / height */
                           format,
                           1, /* rowstride */
                           &byte);

  test_color (texture, expected_pixel);

  cogl_object_unref (texture);
}

static void
test_write_short (CoglContext *context,
                  CoglPixelFormat format,
                  uint16_t value,
                  uint32_t expected_pixel)
{
  CoglTexture *texture = test_utils_create_color_texture (context, 0);

  cogl_texture_set_region (texture,
                           0, 0, /* src_x / src_y */
                           0, 0, /* dst_x / dst_y */
                           1, 1, /* dst_w / dst_h */
                           1, 1, /* width / height */
                           format,
                           2, /* rowstride */
                           (uint8_t *) &value);

  test_color (texture, expected_pixel);

  cogl_object_unref (texture);
}

static void
test_write_bytes (CoglContext *context,
                  CoglPixelFormat format,
                  uint32_t value,
                  uint32_t expected_pixel)
{
  CoglTexture *texture = test_utils_create_color_texture (context, 0);

  value = GUINT32_TO_BE (value);

  cogl_texture_set_region (texture,
                           0, 0, /* src_x / src_y */
                           0, 0, /* dst_x / dst_y */
                           1, 1, /* dst_w / dst_h */
                           1, 1, /* width / height */
                           format,
                           4, /* rowstride */
                           (uint8_t *) &value);

  test_color (texture, expected_pixel);

  cogl_object_unref (texture);
}

static void
test_write_int (CoglContext *context,
                CoglPixelFormat format,
                uint32_t expected_pixel,
                ...)
{
  va_list ap;
  int bits;
  uint32_t tex_data = 0;
  int bits_sum = 0;
  CoglTexture *texture = test_utils_create_color_texture (context, 0);

  va_start (ap, expected_pixel);

  /* Convert the va args into a single 32-bit value */
  while ((bits = va_arg (ap, int)) != -1)
    {
      uint32_t value = (va_arg (ap, int) * ((1 << bits) - 1) + 127) / 255;

      bits_sum += bits;

      tex_data |= value << (32 - bits_sum);
    }

  va_end (ap);

  cogl_texture_set_region (texture,
                           0, 0, /* src_x / src_y */
                           0, 0, /* dst_x / dst_y */
                           1, 1, /* dst_w / dst_h */
                           1, 1, /* width / height */
                           format,
                           4, /* rowstride */
                           (uint8_t *) &tex_data);

  test_color (texture, expected_pixel);

  cogl_object_unref (texture);
}

void
test_write_texture_formats (void)
{
  test_write_byte (test_ctx, COGL_PIXEL_FORMAT_A_8, 0x34, 0x00000034);
#if 0
  /* I'm not sure what's the right value to put here because Nvidia
     and Mesa seem to behave differently so one of them must be
     wrong. */
  test_write_byte (test_ctx, COGL_PIXEL_FORMAT_G_8, 0x34, 0x340000ff);
#endif

  /* We should always be able to read from an RG buffer regardless of
   * whether RG textures are supported because Cogl will do the
   * conversion for us */
  test_write_bytes (test_ctx, COGL_PIXEL_FORMAT_RG_88, 0x123456ff, 0x123400ff);

  test_write_short (test_ctx, COGL_PIXEL_FORMAT_RGB_565, 0x0843, 0x080819ff);
  test_write_short (test_ctx, COGL_PIXEL_FORMAT_RGBA_4444_PRE, 0x1234, 0x11223344);
  test_write_short (test_ctx, COGL_PIXEL_FORMAT_RGBA_5551_PRE, 0x0887, 0x081019ff);

  test_write_bytes (test_ctx, COGL_PIXEL_FORMAT_RGB_888, 0x123456ff, 0x123456ff);
  test_write_bytes (test_ctx, COGL_PIXEL_FORMAT_BGR_888, 0x563412ff, 0x123456ff);

  test_write_bytes (test_ctx, COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    0x12345678, 0x12345678);
  test_write_bytes (test_ctx, COGL_PIXEL_FORMAT_BGRA_8888_PRE,
                    0x56341278, 0x12345678);
  test_write_bytes (test_ctx, COGL_PIXEL_FORMAT_ARGB_8888_PRE,
                    0x78123456, 0x12345678);
  test_write_bytes (test_ctx, COGL_PIXEL_FORMAT_ABGR_8888_PRE,
                    0x78563412, 0x12345678);

  test_write_int (test_ctx, COGL_PIXEL_FORMAT_RGBA_1010102_PRE,
                  0x123456ff,
                  10, 0x12, 10, 0x34, 10, 0x56, 2, 0xff,
                  -1);
  test_write_int (test_ctx, COGL_PIXEL_FORMAT_BGRA_1010102_PRE,
                  0x123456ff,
                  10, 0x56, 10, 0x34, 10, 0x12, 2, 0xff,
                  -1);
  test_write_int (test_ctx, COGL_PIXEL_FORMAT_ARGB_2101010_PRE,
                  0x123456ff,
                  2, 0xff, 10, 0x12, 10, 0x34, 10, 0x56,
                  -1);
  test_write_int (test_ctx, COGL_PIXEL_FORMAT_ABGR_2101010_PRE,
                  0x123456ff,
                  2, 0xff, 10, 0x56, 10, 0x34, 10, 0x12,
                  -1);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
