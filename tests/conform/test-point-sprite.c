#include <cogl/cogl2-experimental.h>

#include "test-utils.h"

#define POINT_SIZE 8

static const CoglVertexP2T2
point =
  {
    POINT_SIZE, POINT_SIZE,
    0.0f, 0.0f
  };

static const uint8_t
tex_data[3 * 2 * 2] =
  {
    0x00, 0x00, 0xff, 0x00, 0xff, 0x00,
    0x00, 0xff, 0xff, 0xff, 0x00, 0x00
  };

static void
do_test (CoglBool check_orientation)
{
  int fb_width = cogl_framebuffer_get_width (test_fb);
  int fb_height = cogl_framebuffer_get_height (test_fb);
  CoglPrimitive *prim;
  CoglError *error = NULL;
  CoglTexture2D *tex_2d;
  CoglPipeline *pipeline, *solid_pipeline;
  CoglBool res;
  int tex_height;

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0, /* x_1, y_1 */
                                 fb_width, /* x_2 */
                                 fb_height /* y_2 */,
                                 -1, 100 /* near/far */);

  cogl_framebuffer_clear4f (test_fb,
                            COGL_BUFFER_BIT_COLOR,
                            1.0f, 1.0f, 1.0f, 1.0f);

  /* If we're not checking the orientation of the point sprite then
   * we'll set the height of the texture to 1 so that the vertical
   * orientation does not matter */
  if (check_orientation)
    tex_height = 2;
  else
    tex_height = 1;

  tex_2d = cogl_texture_2d_new_from_data (test_ctx,
                                          2, tex_height, /* width/height */
                                          COGL_PIXEL_FORMAT_RGB_888,
                                          COGL_PIXEL_FORMAT_ANY,
                                          6, /* row stride */
                                          tex_data,
                                          &error);
  g_assert (tex_2d != NULL);
  g_assert (error == NULL);

  pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_layer_texture (pipeline, 0, COGL_TEXTURE (tex_2d));

  res = cogl_pipeline_set_layer_point_sprite_coords_enabled (pipeline,
                                                             /* layer_index */
                                                             0,
                                                             /* enable */
                                                             TRUE,
                                                             &error);
  g_assert (res == TRUE);
  g_assert (error == NULL);

  cogl_pipeline_set_layer_filters (pipeline,
                                   0, /* layer_index */
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);
  cogl_pipeline_set_point_size (pipeline, POINT_SIZE);

  prim = cogl_primitive_new_p2t2 (test_ctx,
                                  COGL_VERTICES_MODE_POINTS,
                                  1, /* n_vertices */
                                  &point);

  cogl_primitive_draw (prim, test_fb, pipeline);

  /* Render the primitive again without point sprites to make sure
     disabling it works */
  solid_pipeline = cogl_pipeline_copy (pipeline);
  cogl_pipeline_set_layer_point_sprite_coords_enabled (solid_pipeline,
                                                       /* layer_index */
                                                       0,
                                                       /* enable */
                                                       FALSE,
                                                       &error);
  cogl_framebuffer_push_matrix (test_fb);
  cogl_framebuffer_translate (test_fb,
                              POINT_SIZE * 2, /* x */
                              0.0f, /* y */
                              0.0f /* z */);
  cogl_primitive_draw (prim, test_fb, solid_pipeline);
  cogl_framebuffer_pop_matrix (test_fb);

  cogl_object_unref (prim);
  cogl_object_unref (solid_pipeline);
  cogl_object_unref (pipeline);
  cogl_object_unref (tex_2d);

  test_utils_check_pixel (test_fb,
                          POINT_SIZE - POINT_SIZE / 4,
                          POINT_SIZE - POINT_SIZE / 4,
                          0x0000ffff);
  test_utils_check_pixel (test_fb,
                          POINT_SIZE + POINT_SIZE / 4,
                          POINT_SIZE - POINT_SIZE / 4,
                          0x00ff00ff);
  test_utils_check_pixel (test_fb,
                          POINT_SIZE - POINT_SIZE / 4,
                          POINT_SIZE + POINT_SIZE / 4,
                          check_orientation ?
                          0x00ffffff :
                          0x0000ffff);
  test_utils_check_pixel (test_fb,
                          POINT_SIZE + POINT_SIZE / 4,
                          POINT_SIZE + POINT_SIZE / 4,
                          check_orientation ?
                          0xff0000ff :
                          0x00ff00ff);

  /* When rendering without the point sprites all of the texture
     coordinates should be 0,0 so it should get the top-left texel
     which is blue */
  test_utils_check_region (test_fb,
                           POINT_SIZE * 3 - POINT_SIZE / 2 + 1,
                           POINT_SIZE - POINT_SIZE / 2 + 1,
                           POINT_SIZE - 2, POINT_SIZE - 2,
                           0x0000ffff);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

void
test_point_sprite (void)
{
  do_test (FALSE /* don't check orientation */);
}

void
test_point_sprite_orientation (void)
{
  do_test (TRUE /* check orientation */);
}
