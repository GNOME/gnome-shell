#define COGL_ENABLE_EXPERIMENTAL_2_0_API
#include <cogl/cogl.h>
#include <cogl-path/cogl-path.h>

#include <string.h>

#include "test-utils.h"

void
test_path_clip (void)
{
  CoglPath *path;
  CoglPipeline *pipeline;
  int fb_width, fb_height;

  fb_width = cogl_framebuffer_get_width (test_fb);
  fb_height = cogl_framebuffer_get_height (test_fb);

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0, fb_width, fb_height, -1, 100);

  path = cogl_path_new ();

  cogl_framebuffer_clear4f (test_fb,
                            COGL_BUFFER_BIT_COLOR,
                            1.0f, 0.0f, 0.0f, 1.0f);

  /* Make an L-shape with the top right corner left untouched */
  cogl_path_move_to (path, 0, fb_height);
  cogl_path_line_to (path, fb_width, fb_height);
  cogl_path_line_to (path, fb_width, fb_height / 2);
  cogl_path_line_to (path, fb_width / 2, fb_height / 2);
  cogl_path_line_to (path, fb_width / 2, 0);
  cogl_path_line_to (path, 0, 0);
  cogl_path_close (path);

  cogl_framebuffer_push_path_clip (test_fb, path);

  /* Try to fill the framebuffer with a blue rectangle. This should be
   * clipped to leave the top right quadrant as is */
  pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_color4ub (pipeline, 0, 0, 255, 255);
  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   0, 0, fb_width, fb_height);

  cogl_framebuffer_pop_clip (test_fb);

  cogl_object_unref (pipeline);
  cogl_object_unref (path);

  /* Check each of the four quadrants */
  test_utils_check_pixel (test_fb,
                          fb_width / 4, fb_height / 4,
                          0x0000ffff);
  test_utils_check_pixel (test_fb,
                          fb_width * 3 / 4, fb_height / 4,
                          0xff0000ff);
  test_utils_check_pixel (test_fb,
                          fb_width / 4, fb_height * 3 / 4,
                          0x0000ffff);
  test_utils_check_pixel (test_fb,
                          fb_width * 3 / 4, fb_height * 3 / 4,
                          0x0000ffff);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
