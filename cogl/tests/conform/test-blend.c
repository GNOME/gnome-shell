#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

static void
paint (void)
{
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);
  int width = cogl_framebuffer_get_width (test_fb);
  int half_width = width / 2;
  int height = cogl_framebuffer_get_height (test_fb);
  CoglVertexP2 tri0_vertices[] = {
        { 0, 0 },
        { 0, height },
        { half_width, height },
  };
  CoglVertexP2C4 tri1_vertices[] = {
        { half_width, 0, 0x80, 0x80, 0x80, 0x80 },
        { half_width, height, 0x80, 0x80, 0x80, 0x80 },
        { width, height, 0x80, 0x80, 0x80, 0x80 },
  };
  CoglPrimitive *tri0;
  CoglPrimitive *tri1;

  cogl_framebuffer_clear4f (test_fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);

  tri0 = cogl_primitive_new_p2 (test_ctx, COGL_VERTICES_MODE_TRIANGLES,
                                3, tri0_vertices);
  tri1 = cogl_primitive_new_p2c4 (test_ctx, COGL_VERTICES_MODE_TRIANGLES,
                                  3, tri1_vertices);

  /* Check that cogl correctly handles the case where we draw
   * different primitives same pipeline and switch from using the
   * opaque color associated with the pipeline and using a colour
   * attribute with an alpha component which implies blending is
   * required.
   *
   * If Cogl gets this wrong then then in all likelyhood the second
   * primitive will be drawn with blending still disabled.
   */

  cogl_primitive_draw (tri0, test_fb, pipeline);
  cogl_primitive_draw (tri1, test_fb, pipeline);

  test_utils_check_pixel_and_alpha (test_fb,
                                    half_width + 5,
                                    height - 5,
                                    0x80808080);
}

void
test_blend (void)
{
  cogl_framebuffer_orthographic (test_fb, 0, 0,
                                 cogl_framebuffer_get_width (test_fb),
                                 cogl_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint ();
}

