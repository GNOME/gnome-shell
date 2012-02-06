#include <cogl/cogl.h>
#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  CoglContext *context;
  int fb_width;
  int fb_height;
  CoglFramebuffer *fb;
} TestState;

#define PRIM_COLOR 0xff00ffff
#define TEX_COLOR 0x0000ffff

typedef CoglPrimitive * (* TestPrimFunc) (guint32 *expected_color);

static CoglPrimitive *
test_prim_p2 (guint32 *expected_color)
{
  static const CoglVertexP2 verts[] =
    { { 0, 0 }, { 0, 10 }, { 10, 0 } };

  return cogl_primitive_new_p2 (COGL_VERTICES_MODE_TRIANGLES,
                                3, /* n_vertices */
                                verts);
}

static CoglPrimitive *
test_prim_p3 (guint32 *expected_color)
{
  static const CoglVertexP3 verts[] =
    { { 0, 0, 0 }, { 0, 10, 0 }, { 10, 0, 0 } };

  return cogl_primitive_new_p3 (COGL_VERTICES_MODE_TRIANGLES,
                                3, /* n_vertices */
                                verts);
}

static CoglPrimitive *
test_prim_p2c4 (guint32 *expected_color)
{
  static const CoglVertexP2C4 verts[] =
    { { 0, 0, 255, 255, 0, 255 },
      { 0, 10, 255, 255, 0, 255 },
      { 10, 0, 255, 255, 0, 255 } };

  *expected_color = 0xffff00ff;

  return cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p3c4 (guint32 *expected_color)
{
  static const CoglVertexP3C4 verts[] =
    { { 0, 0, 0, 255, 255, 0, 255 },
      { 0, 10, 0, 255, 255, 0, 255 },
      { 10, 0, 0, 255, 255, 0, 255 } };

  *expected_color = 0xffff00ff;

  return cogl_primitive_new_p3c4 (COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p2t2 (guint32 *expected_color)
{
  static const CoglVertexP2T2 verts[] =
    { { 0, 0, 1, 0 },
      { 0, 10, 1, 0 },
      { 10, 0, 1, 0 } };

  *expected_color = TEX_COLOR;

  return cogl_primitive_new_p2t2 (COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p3t2 (guint32 *expected_color)
{
  static const CoglVertexP3T2 verts[] =
    { { 0, 0, 0, 1, 0 },
      { 0, 10, 0, 1, 0 },
      { 10, 0, 0, 1, 0 } };

  *expected_color = TEX_COLOR;

  return cogl_primitive_new_p3t2 (COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p2t2c4 (guint32 *expected_color)
{
  static const CoglVertexP2T2C4 verts[] =
    { { 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 0, 10, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 10, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff } };

  /* The blue component of the texture color should be replaced with 0xf0 */
  *expected_color = (TEX_COLOR & 0xffff00ff) | 0x0000f000;

  return cogl_primitive_new_p2t2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                    3, /* n_vertices */
                                    verts);
}

static CoglPrimitive *
test_prim_p3t2c4 (guint32 *expected_color)
{
  static const CoglVertexP3T2C4 verts[] =
    { { 0, 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 0, 10, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 10, 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff } };

  /* The blue component of the texture color should be replaced with 0xf0 */
  *expected_color = (TEX_COLOR & 0xffff00ff) | 0x0000f000;

  return cogl_primitive_new_p3t2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                    3, /* n_vertices */
                                    verts);
}

static const TestPrimFunc
test_prim_funcs[] =
  {
    test_prim_p2,
    test_prim_p3,
    test_prim_p2c4,
    test_prim_p3c4,
    test_prim_p2t2,
    test_prim_p3t2,
    test_prim_p2t2c4,
    test_prim_p3t2c4
  };

static void
test_paint (TestState *state)
{
  CoglPipeline *pipeline;
  CoglTexture *tex;
  guint8 tex_data[6];
  int i;

  /* Create a two pixel texture. The first pixel is white and the
     second pixel is tex_color. The assumption is that if no texture
     coordinates are specified then it will default to 0,0 and get
     white */
  tex_data[0] = 255;
  tex_data[1] = 255;
  tex_data[2] = 255;
  tex_data[3] = (TEX_COLOR >> 24) & 0xff;
  tex_data[4] = (TEX_COLOR >> 16) & 0xff;
  tex_data[5] = (TEX_COLOR >> 8) & 0xff;
  tex = cogl_texture_new_from_data (2, 1, /* size */
                                    COGL_TEXTURE_NO_ATLAS,
                                    COGL_PIXEL_FORMAT_RGB_888,
                                    COGL_PIXEL_FORMAT_ANY,
                                    6, /* rowstride */
                                    tex_data);
  pipeline = cogl_pipeline_new ();
  cogl_pipeline_set_color4ub (pipeline,
                              (PRIM_COLOR >> 24) & 0xff,
                              (PRIM_COLOR >> 16) & 0xff,
                              (PRIM_COLOR >> 8) & 0xff,
                              (PRIM_COLOR >> 0) & 0xff);
  cogl_pipeline_set_layer_texture (pipeline, 0, tex);
  cogl_handle_unref (tex);
  cogl_set_source (pipeline);
  cogl_object_unref (pipeline);

  for (i = 0; i < G_N_ELEMENTS (test_prim_funcs); i++)
    {
      CoglPrimitive *prim;
      guint32 expected_color = PRIM_COLOR;

      prim = test_prim_funcs[i] (&expected_color);

      cogl_push_matrix ();
      cogl_translate (i * 10, 0, 0);
      cogl_primitive_draw (prim);
      cogl_pop_matrix ();

      test_utils_check_pixel (i * 10 + 2, 2, expected_color);

      cogl_object_unref (prim);
    }
}

void
test_cogl_primitive (TestUtilsGTestFixture *fixture,
                     void *data)
{
  TestUtilsSharedState *shared_state = data;
  TestState state;

  state.context = shared_state->ctx;
  state.fb_width = cogl_framebuffer_get_width (shared_state->fb);
  state.fb_height = cogl_framebuffer_get_height (shared_state->fb);
  state.fb = shared_state->fb;

  cogl_ortho (0, state.fb_width, /* left, right */
              state.fb_height, 0, /* bottom, top */
              -1, 100 /* z near, far */);

  test_paint (&state);

  if (g_test_verbose ())
    g_print ("OK\n");
}
