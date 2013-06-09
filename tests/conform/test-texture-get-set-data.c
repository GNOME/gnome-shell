#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

static void
check_texture (int width, int height, TestUtilsTextureFlags flags)
{
  CoglTexture *tex;
  uint8_t *data, *p;
  int y, x;
  int rowstride;

  p = data = g_malloc (width * height * 4);
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        *(p++) = x;
        *(p++) = y;
        *(p++) = 128;
        *(p++) = (x ^ y);
      }

  tex = test_utils_texture_new_from_data (test_ctx,
                                          width, height,
                                          flags,
                                          COGL_PIXEL_FORMAT_RGBA_8888,
                                          COGL_PIXEL_FORMAT_RGBA_8888,
                                          width * 4,
                                          data);

  /* Replace the bottom right quarter of the data with negated data to
     test set_region */
  rowstride = width * 4;
  p = data + (height / 2) * rowstride + rowstride / 2;
  for (y = 0; y < height / 2; y++)
    {
      for (x = 0; x < width / 2; x++)
        {
          p[0] = ~p[0];
          p[1] = ~p[1];
          p[2] = ~p[2];
          p[3] = ~p[3];
          p += 4;
        }
      p += width * 2;
    }
  cogl_texture_set_region (tex,
                           width / 2,
                           height / 2,
                           width / 2, /* dest x */
                           height / 2, /* dest y */
                           width / 2, /* region width */
                           height / 2, /* region height */
                           width, /* src width */
                           height, /* src height */
                           COGL_PIXEL_FORMAT_RGBA_8888,
                           rowstride,
                           data);

  /* Check passing a NULL pointer and a zero rowstride. The texture
     should calculate the needed data size and return it */
  g_assert_cmpint (cogl_texture_get_data (tex, COGL_PIXEL_FORMAT_ANY, 0, NULL),
                   ==,
                   width * height * 4);

  /* Try first receiving the data as RGB. This should cause a
   * conversion */
  memset (data, 0, width * height * 4);

  cogl_texture_get_data (tex, COGL_PIXEL_FORMAT_RGB_888,
                         width * 3, data);

  p = data;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        if (x >= width / 2 && y >= height / 2)
          {
            g_assert_cmpint (p[0], ==, ~x & 0xff);
            g_assert_cmpint (p[1], ==, ~y & 0xff);
            g_assert_cmpint (p[2], ==, ~128 & 0xff);
          }
        else
          {
            g_assert_cmpint (p[0], ==, x & 0xff);
            g_assert_cmpint (p[1], ==, y & 0xff);
            g_assert_cmpint (p[2], ==, 128);
          }
        p += 3;
      }

  /* Now try receiving the data as RGBA. This should not cause a
   * conversion and no unpremultiplication because we explicitly set
   * the internal format when we created the texture */
  memset (data, 0, width * height * 4);

  cogl_texture_get_data (tex, COGL_PIXEL_FORMAT_RGBA_8888,
                         width * 4, data);

  p = data;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        if (x >= width / 2 && y >= height / 2)
          {
            g_assert_cmpint (p[0], ==, ~x & 0xff);
            g_assert_cmpint (p[1], ==, ~y & 0xff);
            g_assert_cmpint (p[2], ==, ~128 & 0xff);
            g_assert_cmpint (p[3], ==, ~(x ^ y) & 0xff);
          }
        else
          {
            g_assert_cmpint (p[0], ==, x & 0xff);
            g_assert_cmpint (p[1], ==, y & 0xff);
            g_assert_cmpint (p[2], ==, 128);
            g_assert_cmpint (p[3], ==, (x ^ y) & 0xff);
          }
        p += 4;
      }

  cogl_object_unref (tex);
  g_free (data);
}

void
test_texture_get_set_data (void)
{
  /* First try without atlasing */
  check_texture (256, 256, TEST_UTILS_TEXTURE_NO_ATLAS);
  /* Try again with atlasing. This should end up testing the atlas
     backend and the sub texture backend */
  check_texture (256, 256, 0);
  /* Try with a really big texture in the hope that it will end up
     sliced. */
  check_texture (4, 5128, TEST_UTILS_TEXTURE_NO_ATLAS);
  /* And in the other direction. */
  check_texture (5128, 4, TEST_UTILS_TEXTURE_NO_ATLAS);
}
