#include <cogl/cogl2-experimental.h>

#include "test-utils.h"

/*
 * This tests reading back an RGBA texture in alpha-only format.
 * This test just exists because I accidentally broke it and
 * gnome-shell is doing it.
 *
 * https://bugzilla.gnome.org/show_bug.cgi?id=671016
 */

static const guint8 tex_data[4] = { 0x12, 0x34, 0x56, 0x78 };

void
test_cogl_read_alpha_texture (TestUtilsGTestFixture *fixture,
                              void *data)
{
  TestUtilsSharedState *shared_state = data;
  CoglTexture2D *tex_2d;
  guint8 alpha_value;

  tex_2d = cogl_texture_2d_new_from_data (shared_state->ctx,
                                          1, 1, /* width / height */
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          4, /* rowstride */
                                          tex_data,
                                          NULL);

  cogl_texture_get_data (COGL_TEXTURE (tex_2d),
                         COGL_PIXEL_FORMAT_A_8,
                         1, /* rowstride */
                         &alpha_value);

  cogl_object_unref (tex_2d);

  g_assert_cmpint (alpha_value, ==, 0x78);

  if (g_test_verbose ())
    g_print ("OK\n");
}
