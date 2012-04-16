#include <clutter/clutter.h>
#include <glib.h>
#include <string.h>

#include "test-conform-common.h"

static void
check_texture (int width, int height, CoglTextureFlags flags)
{
  CoglHandle tex;
  uint8_t *data, *p;
  int y, x;

  p = data = g_malloc (width * height * 4);
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        *(p++) = x;
        *(p++) = y;
        *(p++) = 128;
        *(p++) = (x ^ y);
      }

  tex = cogl_texture_new_from_data (width, height,
                                    flags,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    width * 4,
                                    data);

  /* Replace the bottom right quarter of the data with negated data to
     test set_region */
  p = data + (height + 1) * width * 2;
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
                           width / 2, /* src_x */
                           height / 2, /* src_y */
                           width / 2, /* dst_x */
                           height / 2, /* dst_y */
                           width / 2, /* dst_width */
                           height / 2, /* dst_height */
                           width,
                           height,
                           COGL_PIXEL_FORMAT_RGBA_8888,
                           width * 4, /* rowstride */
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

  cogl_handle_unref (tex);
  g_free (data);
}

static void
paint_cb (void)
{
  /* First try without atlasing */
  check_texture (256, 256, COGL_TEXTURE_NO_ATLAS);
  /* Try again with atlasing. This should end up testing the atlas
     backend and the sub texture backend */
  check_texture (256, 256, 0);
  /* Try with a really big texture in the hope that it will end up
     sliced. */
  check_texture (4, 5128, COGL_TEXTURE_NO_ATLAS);
  /* And in the other direction. */
  check_texture (5128, 4, COGL_TEXTURE_NO_ATLAS);

  clutter_main_quit ();
}

void
test_texture_get_set_data (TestUtilsGTestFixture *fixture,
                                void *data)
{
  ClutterActor *stage;
  unsigned int paint_handler;

  /* We create a stage even though we don't usually need it so that if
     the draw-and-read texture fallback is needed then it will have
     something to draw to */
  stage = clutter_stage_get_default ();

  paint_handler = g_signal_connect_after (stage, "paint",
                                          G_CALLBACK (paint_cb), NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_signal_handler_disconnect (stage, paint_handler);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
