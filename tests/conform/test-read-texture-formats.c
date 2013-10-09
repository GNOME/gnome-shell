#include <cogl/cogl2-experimental.h>
#include <stdarg.h>

#include "test-utils.h"

/*
 * This tests reading back an RGBA texture in all of the available
 * pixel formats
 */

static const uint8_t tex_data[4] = { 0x12, 0x34, 0x56, 0x78 };

static void
test_read_byte (CoglTexture2D *tex_2d,
                CoglPixelFormat format,
                uint8_t expected_byte)
{
  uint8_t received_byte;

  cogl_texture_get_data (tex_2d,
                         format,
                         1, /* rowstride */
                         &received_byte);

  g_assert_cmpint (expected_byte, ==, received_byte);
}

static void
test_read_short (CoglTexture2D *tex_2d,
                 CoglPixelFormat format,
                 ...)
{
  va_list ap;
  int bits;
  uint16_t received_value;
  uint16_t expected_value = 0;
  char *received_value_str;
  char *expected_value_str;
  int bits_sum = 0;

  cogl_texture_get_data (tex_2d,
                         format,
                         2, /* rowstride */
                         (uint8_t *) &received_value);

  va_start (ap, format);

  /* Convert the va args into a single 16-bit expected value */
  while ((bits = va_arg (ap, int)) != -1)
    {
      int value = (va_arg (ap, int) * ((1 << bits) - 1) + 128) / 255;

      bits_sum += bits;

      expected_value |= value << (16 - bits_sum);
    }

  va_end (ap);

  received_value_str = g_strdup_printf ("0x%04x", received_value);
  expected_value_str = g_strdup_printf ("0x%04x", expected_value);
  g_assert_cmpstr (received_value_str, ==, expected_value_str);
  g_free (received_value_str);
  g_free (expected_value_str);
}

static void
test_read_888 (CoglTexture2D *tex_2d,
               CoglPixelFormat format,
               uint32_t expected_pixel)
{
  uint8_t pixel[4];

  cogl_texture_get_data (tex_2d,
                         format,
                         4, /* rowstride */
                         pixel);

  test_utils_compare_pixel (pixel, expected_pixel);
}

static void
test_read_8888 (CoglTexture2D *tex_2d,
                CoglPixelFormat format,
                uint32_t expected_pixel)
{
  uint32_t received_pixel;
  char *received_value_str;
  char *expected_value_str;

  cogl_texture_get_data (tex_2d,
                         format,
                         4, /* rowstride */
                         (uint8_t *) &received_pixel);

  received_pixel = GUINT32_FROM_BE (received_pixel);

  received_value_str = g_strdup_printf ("0x%08x", received_pixel);
  expected_value_str = g_strdup_printf ("0x%08x", expected_pixel);
  g_assert_cmpstr (received_value_str, ==, expected_value_str);
  g_free (received_value_str);
  g_free (expected_value_str);
}

static void
test_read_int (CoglTexture2D *tex_2d,
               CoglPixelFormat format,
               ...)
{
  va_list ap;
  int bits;
  uint32_t received_value;
  uint32_t expected_value = 0;
  char *received_value_str;
  char *expected_value_str;
  int bits_sum = 0;

  cogl_texture_get_data (tex_2d,
                         format,
                         4, /* rowstride */
                         (uint8_t *) &received_value);

  va_start (ap, format);

  /* Convert the va args into a single 32-bit expected value */
  while ((bits = va_arg (ap, int)) != -1)
    {
      uint32_t value = (va_arg (ap, int) * ((1 << bits) - 1) + 128) / 255;

      bits_sum += bits;

      expected_value |= value << (32 - bits_sum);
    }

  va_end (ap);

  received_value_str = g_strdup_printf ("0x%08x", received_value);
  expected_value_str = g_strdup_printf ("0x%08x", expected_value);
  g_assert_cmpstr (received_value_str, ==, expected_value_str);
  g_free (received_value_str);
  g_free (expected_value_str);
}

void
test_read_texture_formats (void)
{
  CoglTexture2D *tex_2d;

  tex_2d = cogl_texture_2d_new_from_data (test_ctx,
                                          1, 1, /* width / height */
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          4, /* rowstride */
                                          tex_data,
                                          NULL);

  test_read_byte (tex_2d, COGL_PIXEL_FORMAT_A_8, 0x78);

#if 0
  /* I'm not sure what's the right value to put here because Nvidia
     and Mesa seem to behave differently so one of them must be
     wrong. */
  test_read_byte (tex_2d, COGL_PIXEL_FORMAT_G_8, 0x9c);
#endif

  test_read_short (tex_2d, COGL_PIXEL_FORMAT_RGB_565,
                   5, 0x12, 6, 0x34, 5, 0x56,
                   -1);
  test_read_short (tex_2d, COGL_PIXEL_FORMAT_RGBA_4444_PRE,
                   4, 0x12, 4, 0x34, 4, 0x56, 4, 0x78,
                   -1);
  test_read_short (tex_2d, COGL_PIXEL_FORMAT_RGBA_5551_PRE,
                   5, 0x12, 5, 0x34, 5, 0x56, 1, 0x78,
                   -1);

  test_read_888 (tex_2d, COGL_PIXEL_FORMAT_RGB_888, 0x123456ff);
  test_read_888 (tex_2d, COGL_PIXEL_FORMAT_BGR_888, 0x563412ff);

  test_read_8888 (tex_2d, COGL_PIXEL_FORMAT_RGBA_8888_PRE, 0x12345678);
  test_read_8888 (tex_2d, COGL_PIXEL_FORMAT_BGRA_8888_PRE, 0x56341278);
  test_read_8888 (tex_2d, COGL_PIXEL_FORMAT_ARGB_8888_PRE, 0x78123456);
  test_read_8888 (tex_2d, COGL_PIXEL_FORMAT_ABGR_8888_PRE, 0x78563412);

  test_read_int (tex_2d, COGL_PIXEL_FORMAT_RGBA_1010102_PRE,
                 10, 0x12, 10, 0x34, 10, 0x56, 2, 0x78,
                 -1);
  test_read_int (tex_2d, COGL_PIXEL_FORMAT_BGRA_1010102_PRE,
                 10, 0x56, 10, 0x34, 10, 0x12, 2, 0x78,
                 -1);
  test_read_int (tex_2d, COGL_PIXEL_FORMAT_ARGB_2101010_PRE,
                 2, 0x78, 10, 0x12, 10, 0x34, 10, 0x56,
                 -1);
  test_read_int (tex_2d, COGL_PIXEL_FORMAT_ABGR_2101010_PRE,
                 2, 0x78, 10, 0x56, 10, 0x34, 10, 0x12,
                 -1);

  cogl_object_unref (tex_2d);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
