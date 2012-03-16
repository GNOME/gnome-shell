#include <cogl/cogl2-experimental.h>
#include <stdarg.h>

#include "test-utils.h"

/*
 * This tests writing data to an RGBA texture in all of the available
 * pixel formats
 */

static void
test_color (CoglTexture *texture,
            guint32 expected_pixel)
{
  guint32 received_pixel;
  char *received_value_str;
  char *expected_value_str;

  cogl_texture_get_data (texture,
                         COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                         4, /* rowstride */
                         (guint8 *) &received_pixel);

  received_pixel = GUINT32_FROM_BE (received_pixel);

  received_value_str = g_strdup_printf ("0x%08x", received_pixel);
  expected_value_str = g_strdup_printf ("0x%08x", expected_pixel);
  g_assert_cmpstr (received_value_str, ==, expected_value_str);
  g_free (received_value_str);
  g_free (expected_value_str);
}

static void
test_write_byte (CoglContext *context,
                 CoglPixelFormat format,
                 guint8 byte,
                 guint32 expected_pixel)
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
                  guint16 value,
                  guint32 expected_pixel)
{
  CoglTexture *texture = test_utils_create_color_texture (context, 0);

  cogl_texture_set_region (texture,
                           0, 0, /* src_x / src_y */
                           0, 0, /* dst_x / dst_y */
                           1, 1, /* dst_w / dst_h */
                           1, 1, /* width / height */
                           format,
                           2, /* rowstride */
                           (guint8 *) &value);

  test_color (texture, expected_pixel);

  cogl_object_unref (texture);
}

static void
test_write_bytes (CoglContext *context,
                  CoglPixelFormat format,
                  guint32 value,
                  guint32 expected_pixel)
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
                           (guint8 *) &value);

  test_color (texture, expected_pixel);

  cogl_object_unref (texture);
}

static void
test_write_int (CoglContext *context,
                CoglPixelFormat format,
                guint32 expected_pixel,
                ...)
{
  va_list ap;
  int bits;
  guint32 tex_data = 0;
  int bits_sum = 0;
  CoglTexture *texture = test_utils_create_color_texture (context, 0);

  va_start (ap, expected_pixel);

  /* Convert the va args into a single 32-bit value */
  while ((bits = va_arg (ap, int)) != -1)
    {
      guint32 value = (va_arg (ap, int) * ((1 << bits) - 1) + 127) / 255;

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
                           (guint8 *) &tex_data);

  test_color (texture, expected_pixel);

  cogl_object_unref (texture);
}

void
test_write_texture_formats (void)
{
  test_write_byte (ctx, COGL_PIXEL_FORMAT_A_8, 0x34, 0x00000034);
#if 0
  /* I'm not sure what's the right value to put here because Nvidia
     and Mesa seem to behave differently so one of them must be
     wrong. */
  test_write_byte (ctx, COGL_PIXEL_FORMAT_G_8, 0x34, 0x340000ff);
#endif

  test_write_short (ctx, COGL_PIXEL_FORMAT_RGB_565, 0x0843, 0x080819ff);
  test_write_short (ctx, COGL_PIXEL_FORMAT_RGBA_4444_PRE, 0x1234, 0x11223344);
  test_write_short (ctx, COGL_PIXEL_FORMAT_RGBA_5551_PRE, 0x0887, 0x081019ff);

  test_write_bytes (ctx, COGL_PIXEL_FORMAT_RGB_888, 0x123456ff, 0x123456ff);
  test_write_bytes (ctx, COGL_PIXEL_FORMAT_BGR_888, 0x563412ff, 0x123456ff);

  test_write_bytes (ctx, COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    0x12345678, 0x12345678);
  test_write_bytes (ctx, COGL_PIXEL_FORMAT_BGRA_8888_PRE,
                    0x56341278, 0x12345678);
  test_write_bytes (ctx, COGL_PIXEL_FORMAT_ARGB_8888_PRE,
                    0x78123456, 0x12345678);
  test_write_bytes (ctx, COGL_PIXEL_FORMAT_ABGR_8888_PRE,
                    0x78563412, 0x12345678);

  test_write_int (ctx, COGL_PIXEL_FORMAT_RGBA_1010102_PRE,
                  0x123456ff,
                  10, 0x12, 10, 0x34, 10, 0x56, 2, 0xff,
                  -1);
  test_write_int (ctx, COGL_PIXEL_FORMAT_BGRA_1010102_PRE,
                  0x123456ff,
                  10, 0x56, 10, 0x34, 10, 0x12, 2, 0xff,
                  -1);
  test_write_int (ctx, COGL_PIXEL_FORMAT_ARGB_2101010_PRE,
                  0x123456ff,
                  2, 0xff, 10, 0x12, 10, 0x34, 10, 0x56,
                  -1);
  test_write_int (ctx, COGL_PIXEL_FORMAT_ABGR_2101010_PRE,
                  0x123456ff,
                  2, 0xff, 10, 0x56, 10, 0x34, 10, 0x12,
                  -1);

  if (g_test_verbose ())
    g_print ("OK\n");
}
