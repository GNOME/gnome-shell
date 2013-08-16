#include <cogl/cogl.h>

#include "test-utils.h"

void
test_framebuffer_get_bits (void)
{
  CoglTexture2D *tex_a =
    cogl_texture_2d_new_with_size (test_ctx,
                                   16, 16, /* width/height */
                                   COGL_PIXEL_FORMAT_A_8);
  CoglOffscreen *offscreen_a =
    cogl_offscreen_new_with_texture (COGL_TEXTURE (tex_a));
  CoglFramebuffer *fb_a = COGL_FRAMEBUFFER (offscreen_a);
  CoglTexture2D *tex_rgba =
    cogl_texture_2d_new_with_size (test_ctx,
                                   16, 16, /* width/height */
                                   COGL_PIXEL_FORMAT_RGBA_8888);
  CoglOffscreen *offscreen_rgba =
    cogl_offscreen_new_with_texture (COGL_TEXTURE (tex_rgba));
  CoglFramebuffer *fb_rgba = COGL_FRAMEBUFFER (offscreen_rgba);

  cogl_framebuffer_allocate (fb_a, NULL);
  cogl_framebuffer_allocate (fb_rgba, NULL);

  g_assert_cmpint (cogl_framebuffer_get_red_bits (fb_a), ==, 0);
  g_assert_cmpint (cogl_framebuffer_get_green_bits (fb_a), ==, 0);
  g_assert_cmpint (cogl_framebuffer_get_blue_bits (fb_a), ==, 0);
  g_assert_cmpint (cogl_framebuffer_get_alpha_bits (fb_a), >=, 1);

  g_assert_cmpint (cogl_framebuffer_get_red_bits (fb_rgba), >=, 1);
  g_assert_cmpint (cogl_framebuffer_get_green_bits (fb_rgba), >=, 1);
  g_assert_cmpint (cogl_framebuffer_get_blue_bits (fb_rgba), >=, 1);
  g_assert_cmpint (cogl_framebuffer_get_alpha_bits (fb_rgba), >=, 1);

  cogl_object_unref (fb_rgba);
  cogl_object_unref (tex_rgba);
  cogl_object_unref (fb_a);
  cogl_object_unref (tex_a);
}
