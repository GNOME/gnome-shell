#include <cogl/cogl.h>

#include "test-utils.h"

/* Tests that the various texture types can be freed without being
 * allocated */

/* Texture size that is probably to big to fit within the texture
 * limits */
#define BIG_TEX_WIDTH 16384
#define BIG_TEX_HEIGHT 128

void
test_texture_no_allocate (void)
{
  uint8_t *tex_data;
  CoglTexture *texture;
  CoglTexture2D *texture_2d;

  tex_data = g_malloc (BIG_TEX_WIDTH * BIG_TEX_HEIGHT * 4);

  /* NB: if we make the atlas and sliced texture APIs public then this
   * could changed to explicitly use that instead of the magic texture
   * API */

  /* Try to create an atlas texture that is too big so it will
   * internally be freed without allocating */
  texture = cogl_texture_new_from_data (BIG_TEX_WIDTH,
                                        BIG_TEX_HEIGHT,
                                        COGL_TEXTURE_NONE, /* flags */
                                        /* format */
                                        COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                        /* internal format */
                                        COGL_PIXEL_FORMAT_ANY,
                                        /* rowstride */
                                        BIG_TEX_WIDTH * 4,
                                        tex_data);

  g_free (tex_data);

  /* It's ok if this causes an error, we just don't want it to
   * crash */

  if (texture)
    cogl_object_unref (texture);

  /* Try to create a sliced texture without allocating it */
  texture = cogl_texture_new_with_size (BIG_TEX_WIDTH,
                                        BIG_TEX_HEIGHT,
                                        COGL_TEXTURE_NO_ATLAS,
                                        COGL_PIXEL_FORMAT_RGBA_8888_PRE);
  cogl_object_unref (texture);

  /* 2D texture */
  texture_2d = cogl_texture_2d_new_with_size (test_ctx,
                                              64, 64,
                                              COGL_PIXEL_FORMAT_RGBA_8888_PRE);
  cogl_object_unref (texture_2d);

  /* 3D texture */
  if (cogl_has_feature (test_ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      CoglTexture3D *texture_3d =
        cogl_texture_3d_new_with_size (test_ctx,
                                       64, 64, 64,
                                       COGL_PIXEL_FORMAT_RGBA_8888_PRE);
      cogl_object_unref (texture_3d);
    }

  /* Rectangle texture */
  if (cogl_has_feature (test_ctx, COGL_FEATURE_ID_TEXTURE_RECTANGLE))
    {
      CoglTextureRectangle *texture_rect =
        cogl_texture_rectangle_new_with_size (test_ctx,
                                              64, 64,
                                              COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                              NULL /* error */);
      cogl_object_unref (texture_rect);
    }
}
