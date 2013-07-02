#include <cogl/cogl.h>

#include "test-utils.h"

#define N_TEXTURES 128

#define OPACITY_FOR_ROW(y) \
  (0xff - ((y) & 0xf) * 0x10)

#define COLOR_FOR_SIZE(size) \
  (colors + (size) % 3)

typedef struct
{
  uint8_t red, green, blue, alpha;
} TestColor;

static const TestColor colors[] =
  { { 0xff, 0x00, 0x00, 0xff },
    { 0x00, 0xff, 0x00, 0xff },
    { 0x00, 0x00, 0xff, 0xff } };

static CoglTexture *
create_texture (int size)
{
  CoglTexture *texture;
  const TestColor *color;
  uint8_t *data, *p;
  int x, y;

  /* Create a red, green or blue texture depending on the size */
  color = COLOR_FOR_SIZE (size);

  p = data = g_malloc (size * size * 4);

  /* Fill the data with the color but fade the opacity out with
     increasing y coordinates so that we can see the blending it the
     atlas migration accidentally blends with garbage in the
     texture */
  for (y = 0; y < size; y++)
    {
      int opacity = OPACITY_FOR_ROW (y);

      for (x = 0; x < size; x++)
        {
          /* Store the colors premultiplied */
          p[0] = color->red * opacity / 255;
          p[1] = color->green * opacity / 255;
          p[2] = color->blue * opacity / 255;
          p[3] = opacity;

          p += 4;
        }
    }

  texture = test_utils_texture_new_from_data (test_ctx,
                                              size, /* width */
                                              size, /* height */
                                              TEST_UTILS_TEXTURE_NONE, /* flags */
                                              /* format */
                                              COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                              /* rowstride */
                                              size * 4,
                                              data);

  g_free (data);

  return texture;
}

static void
verify_texture (CoglTexture *texture, int size)
{
  uint8_t *data, *p;
  int x, y;
  const TestColor *color;

  color = COLOR_FOR_SIZE (size);

  p = data = g_malloc (size * size * 4);

  cogl_texture_get_data (texture,
                         COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                         size * 4,
                         data);

  for (y = 0; y < size; y++)
    {
      int opacity = OPACITY_FOR_ROW (y);

      for (x = 0; x < size; x++)
        {
          TestColor real_color =
            {
              color->red * opacity / 255,
              color->green * opacity / 255,
              color->blue * opacity / 255
            };

          test_utils_compare_pixel (p,
                                    (real_color.red << 24) |
                                    (real_color.green << 16) |
                                    (real_color.blue << 8) |
                                    opacity);
          g_assert_cmpint (p[3], ==, opacity);

          p += 4;
        }
    }

  g_free (data);
}

void
test_atlas_migration (void)
{
  CoglTexture *textures[N_TEXTURES];
  int i, tex_num;

  /* Create and destroy all of the textures a few times to increase
     the chances that we'll end up reusing the buffers for previously
     discarded atlases */
  for (i = 0; i < 5; i++)
    {
      for (tex_num = 0; tex_num < N_TEXTURES; tex_num++)
        textures[tex_num] = create_texture (tex_num + 1);
      for (tex_num = 0; tex_num < N_TEXTURES; tex_num++)
        cogl_object_unref (textures[tex_num]);
    }

  /* Create all the textures again */
  for (tex_num = 0; tex_num < N_TEXTURES; tex_num++)
    textures[tex_num] = create_texture (tex_num + 1);

  /* Verify that they all still have the right data */
  for (tex_num = 0; tex_num < N_TEXTURES; tex_num++)
    verify_texture (textures[tex_num], tex_num + 1);

  /* Destroy them all */
  for (tex_num = 0; tex_num < N_TEXTURES; tex_num++)
    cogl_object_unref (textures[tex_num]);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
