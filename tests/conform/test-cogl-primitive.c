#define COGL_ENABLE_EXPERIMENTAL_2_0_API
#define CLUTTER_ENABLE_EXPERIMENTAL_API
#include <clutter/clutter.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x00, 0xff, 0x00, 0xff };
static const ClutterColor prim_color = { 0xff, 0x00, 0xff, 0xff };
static const ClutterColor tex_color = { 0x00, 0x00, 0xff, 0xff };

typedef CoglPrimitive * (* TestPrimFunc) (CoglContext *ctx,
                                          ClutterColor *expected_color);

static CoglPrimitive *
test_prim_p2 (CoglContext *ctx, ClutterColor *expected_color)
{
  static const CoglVertexP2 verts[] =
    { { 0, 0 }, { 0, 10 }, { 10, 0 } };

  return cogl_primitive_new_p2 (ctx,
                                COGL_VERTICES_MODE_TRIANGLES,
                                3, /* n_vertices */
                                verts);
}

static CoglPrimitive *
test_prim_p3 (CoglContext *ctx, ClutterColor *expected_color)
{
  static const CoglVertexP3 verts[] =
    { { 0, 0, 0 }, { 0, 10, 0 }, { 10, 0, 0 } };

  return cogl_primitive_new_p3 (ctx,
                                COGL_VERTICES_MODE_TRIANGLES,
                                3, /* n_vertices */
                                verts);
}

static CoglPrimitive *
test_prim_p2c4 (CoglContext *ctx, ClutterColor *expected_color)
{
  static const CoglVertexP2C4 verts[] =
    { { 0, 0, 255, 255, 0, 255 },
      { 0, 10, 255, 255, 0, 255 },
      { 10, 0, 255, 255, 0, 255 } };

  expected_color->red = 255;
  expected_color->green = 255;
  expected_color->blue = 0;

  return cogl_primitive_new_p2c4 (ctx,
                                  COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p3c4 (CoglContext *ctx, ClutterColor *expected_color)
{
  static const CoglVertexP3C4 verts[] =
    { { 0, 0, 0, 255, 255, 0, 255 },
      { 0, 10, 0, 255, 255, 0, 255 },
      { 10, 0, 0, 255, 255, 0, 255 } };

  expected_color->red = 255;
  expected_color->green = 255;
  expected_color->blue = 0;

  return cogl_primitive_new_p3c4 (ctx,
                                  COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p2t2 (CoglContext *ctx, ClutterColor *expected_color)
{
  static const CoglVertexP2T2 verts[] =
    { { 0, 0, 1, 0 },
      { 0, 10, 1, 0 },
      { 10, 0, 1, 0 } };

  *expected_color = tex_color;

  return cogl_primitive_new_p2t2 (ctx,
                                  COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p3t2 (CoglContext *ctx, ClutterColor *expected_color)
{
  static const CoglVertexP3T2 verts[] =
    { { 0, 0, 0, 1, 0 },
      { 0, 10, 0, 1, 0 },
      { 10, 0, 0, 1, 0 } };

  *expected_color = tex_color;

  return cogl_primitive_new_p3t2 (ctx,
                                  COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p2t2c4 (CoglContext *ctx, ClutterColor *expected_color)
{
  static const CoglVertexP2T2C4 verts[] =
    { { 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 0, 10, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 10, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff } };

  *expected_color = tex_color;
  expected_color->blue = 0xf0;

  return cogl_primitive_new_p2t2c4 (ctx,
                                    COGL_VERTICES_MODE_TRIANGLES,
                                    3, /* n_vertices */
                                    verts);
}

static CoglPrimitive *
test_prim_p3t2c4 (CoglContext *ctx, ClutterColor *expected_color)
{
  static const CoglVertexP3T2C4 verts[] =
    { { 0, 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 0, 10, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 10, 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff } };

  *expected_color = tex_color;
  expected_color->blue = 0xf0;

  return cogl_primitive_new_p3t2c4 (ctx,
                                    COGL_VERTICES_MODE_TRIANGLES,
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
paint_cb (void)
{
  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());
  CoglFramebuffer *fb = cogl_get_draw_framebuffer ();
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
  tex_data[3] = tex_color.red;
  tex_data[4] = tex_color.green;
  tex_data[5] = tex_color.blue;
  tex = cogl_texture_new_from_data (2, 1, /* size */
                                    COGL_TEXTURE_NO_ATLAS,
                                    COGL_PIXEL_FORMAT_RGB_888,
                                    COGL_PIXEL_FORMAT_ANY,
                                    6, /* rowstride */
                                    tex_data);
  pipeline = cogl_pipeline_new ();
  cogl_pipeline_set_color4ub (pipeline,
                              prim_color.red,
                              prim_color.green,
                              prim_color.blue,
                              prim_color.alpha);
  cogl_pipeline_set_layer_texture (pipeline, 0, tex);
  cogl_object_unref (tex);

  for (i = 0; i < G_N_ELEMENTS (test_prim_funcs); i++)
    {
      CoglPrimitive *prim;
      ClutterColor expected_color = prim_color;
      guint8 pixel[4];

      prim = test_prim_funcs[i] (ctx, &expected_color);

      cogl_push_matrix ();
      cogl_translate (i * 10, 0, 0);
      cogl_framebuffer_draw_primitive (fb, pipeline, prim);
      cogl_pop_matrix ();

      cogl_read_pixels (i * 10 + 2, 2, 1, 1,
                        COGL_READ_PIXELS_COLOR_BUFFER,
                        COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                        pixel);

      g_assert_cmpint (pixel[0], ==, expected_color.red);
      g_assert_cmpint (pixel[1], ==, expected_color.green);
      g_assert_cmpint (pixel[2], ==, expected_color.blue);

      cogl_object_unref (prim);
    }

  cogl_object_unref (pipeline);

  /* Comment this out to see what the test paints */
  clutter_main_quit ();
}

void
test_cogl_primitive (TestConformSimpleFixture *fixture,
                     gconstpointer data)
{
  ClutterActor *stage;
  guint paint_handler;

  stage = clutter_stage_new ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  paint_handler = g_signal_connect_after (stage, "paint",
                                          G_CALLBACK (paint_cb), NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_signal_handler_disconnect (stage, paint_handler);

  clutter_actor_destroy (stage);

  if (g_test_verbose ())
    g_print ("OK\n");
}
