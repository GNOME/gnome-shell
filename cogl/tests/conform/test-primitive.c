#include <cogl/cogl.h>
#include <string.h>
#include <stdlib.h>

#include "test-utils.h"

typedef struct _TestState
{
  int fb_width;
  int fb_height;
} TestState;

#define PRIM_COLOR 0xff00ffff
#define TEX_COLOR 0x0000ffff

#define N_ATTRIBS 8

typedef CoglPrimitive * (* TestPrimFunc) (CoglContext *ctx, uint32_t *expected_color);

static CoglPrimitive *
test_prim_p2 (CoglContext *ctx, uint32_t *expected_color)
{
  static const CoglVertexP2 verts[] =
    { { 0, 0 }, { 0, 10 }, { 10, 0 } };

  return cogl_primitive_new_p2 (test_ctx,
                                COGL_VERTICES_MODE_TRIANGLES,
                                3, /* n_vertices */
                                verts);
}

static CoglPrimitive *
test_prim_p3 (CoglContext *ctx, uint32_t *expected_color)
{
  static const CoglVertexP3 verts[] =
    { { 0, 0, 0 }, { 0, 10, 0 }, { 10, 0, 0 } };

  return cogl_primitive_new_p3 (test_ctx,
                                COGL_VERTICES_MODE_TRIANGLES,
                                3, /* n_vertices */
                                verts);
}

static CoglPrimitive *
test_prim_p2c4 (CoglContext *ctx, uint32_t *expected_color)
{
  static const CoglVertexP2C4 verts[] =
    { { 0, 0, 255, 255, 0, 255 },
      { 0, 10, 255, 255, 0, 255 },
      { 10, 0, 255, 255, 0, 255 } };

  *expected_color = 0xffff00ff;

  return cogl_primitive_new_p2c4 (test_ctx,
                                  COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p3c4 (CoglContext *ctx, uint32_t *expected_color)
{
  static const CoglVertexP3C4 verts[] =
    { { 0, 0, 0, 255, 255, 0, 255 },
      { 0, 10, 0, 255, 255, 0, 255 },
      { 10, 0, 0, 255, 255, 0, 255 } };

  *expected_color = 0xffff00ff;

  return cogl_primitive_new_p3c4 (test_ctx,
                                  COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p2t2 (CoglContext *ctx, uint32_t *expected_color)
{
  static const CoglVertexP2T2 verts[] =
    { { 0, 0, 1, 0 },
      { 0, 10, 1, 0 },
      { 10, 0, 1, 0 } };

  *expected_color = TEX_COLOR;

  return cogl_primitive_new_p2t2 (test_ctx,
                                  COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p3t2 (CoglContext *ctx, uint32_t *expected_color)
{
  static const CoglVertexP3T2 verts[] =
    { { 0, 0, 0, 1, 0 },
      { 0, 10, 0, 1, 0 },
      { 10, 0, 0, 1, 0 } };

  *expected_color = TEX_COLOR;

  return cogl_primitive_new_p3t2 (test_ctx,
                                  COGL_VERTICES_MODE_TRIANGLES,
                                  3, /* n_vertices */
                                  verts);
}

static CoglPrimitive *
test_prim_p2t2c4 (CoglContext *ctx, uint32_t *expected_color)
{
  static const CoglVertexP2T2C4 verts[] =
    { { 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 0, 10, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 10, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff } };

  /* The blue component of the texture color should be replaced with 0xf0 */
  *expected_color = (TEX_COLOR & 0xffff00ff) | 0x0000f000;

  return cogl_primitive_new_p2t2c4 (test_ctx,
                                    COGL_VERTICES_MODE_TRIANGLES,
                                    3, /* n_vertices */
                                    verts);
}

static CoglPrimitive *
test_prim_p3t2c4 (CoglContext *ctx, uint32_t *expected_color)
{
  static const CoglVertexP3T2C4 verts[] =
    { { 0, 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 0, 10, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff },
      { 10, 0, 0, 1, 0, 0xff, 0xff, 0xf0, 0xff } };

  /* The blue component of the texture color should be replaced with 0xf0 */
  *expected_color = (TEX_COLOR & 0xffff00ff) | 0x0000f000;

  return cogl_primitive_new_p3t2c4 (test_ctx,
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
test_paint (TestState *state)
{
  CoglPipeline *pipeline;
  CoglTexture *tex;
  uint8_t tex_data[6];
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
  tex = test_utils_texture_new_from_data (test_ctx,
                                          2, 1, /* size */
                                          TEST_UTILS_TEXTURE_NO_ATLAS,
                                          COGL_PIXEL_FORMAT_RGB_888,
                                          6, /* rowstride */
                                          tex_data);
  pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_color4ub (pipeline,
                              (PRIM_COLOR >> 24) & 0xff,
                              (PRIM_COLOR >> 16) & 0xff,
                              (PRIM_COLOR >> 8) & 0xff,
                              (PRIM_COLOR >> 0) & 0xff);
  cogl_pipeline_set_layer_texture (pipeline, 0, tex);
  cogl_object_unref (tex);

  for (i = 0; i < G_N_ELEMENTS (test_prim_funcs); i++)
    {
      CoglPrimitive *prim;
      uint32_t expected_color = PRIM_COLOR;

      prim = test_prim_funcs[i] (test_ctx, &expected_color);

      cogl_framebuffer_push_matrix (test_fb);
      cogl_framebuffer_translate (test_fb, i * 10, 0, 0);
      cogl_primitive_draw (prim, test_fb, pipeline);
      cogl_framebuffer_pop_matrix (test_fb);

      test_utils_check_pixel (test_fb, i * 10 + 2, 2, expected_color);

      cogl_object_unref (prim);
    }

  cogl_object_unref (pipeline);
}

static CoglBool
get_attributes_cb (CoglPrimitive *prim,
                   CoglAttribute *attrib,
                   void *user_data)
{
  CoglAttribute ***p = user_data;
  *((* p)++) = attrib;
  return TRUE;
}

static int
compare_pointers (const void *a, const void *b)
{
  CoglAttribute *pa = *(CoglAttribute **) a;
  CoglAttribute *pb = *(CoglAttribute **) b;

  if (pa < pb)
    return -1;
  else if (pa > pb)
    return 1;
  else
    return 0;
}

static void
test_copy (TestState *state)
{
  static const uint16_t indices_data[2] = { 1, 2 };
  CoglAttributeBuffer *buffer =
    cogl_attribute_buffer_new (test_ctx, 100, NULL);
  CoglAttribute *attributes[N_ATTRIBS];
  CoglAttribute *attributes_a[N_ATTRIBS], *attributes_b[N_ATTRIBS];
  CoglAttribute **p;
  CoglPrimitive *prim_a, *prim_b;
  CoglIndices *indices;
  int i;

  for (i = 0; i < N_ATTRIBS; i++)
    {
      char *name = g_strdup_printf ("foo_%i", i);
      attributes[i] = cogl_attribute_new (buffer,
                                          name,
                                          16, /* stride */
                                          16, /* offset */
                                          2, /* components */
                                          COGL_ATTRIBUTE_TYPE_FLOAT);
      g_free (name);
    }

  prim_a = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                               8, /* n_vertices */
                                               attributes,
                                               N_ATTRIBS);

  indices = cogl_indices_new (test_ctx,
                              COGL_INDICES_TYPE_UNSIGNED_SHORT,
                              indices_data,
                              2 /* n_indices */);

  cogl_primitive_set_first_vertex (prim_a, 12);
  cogl_primitive_set_indices (prim_a, indices, 2);

  prim_b = cogl_primitive_copy (prim_a);

  p = attributes_a;
  cogl_primitive_foreach_attribute (prim_a,
                                    get_attributes_cb,
                                    &p);
  g_assert_cmpint (p - attributes_a, ==, N_ATTRIBS);

  p = attributes_b;
  cogl_primitive_foreach_attribute (prim_b,
                                    get_attributes_cb,
                                    &p);
  g_assert_cmpint (p - attributes_b, ==, N_ATTRIBS);

  qsort (attributes_a, N_ATTRIBS, sizeof (CoglAttribute *), compare_pointers);
  qsort (attributes_b, N_ATTRIBS, sizeof (CoglAttribute *), compare_pointers);

  g_assert (memcmp (attributes_a, attributes_b, sizeof (attributes_a)) == 0);

  g_assert_cmpint (cogl_primitive_get_first_vertex (prim_a),
                   ==,
                   cogl_primitive_get_first_vertex (prim_b));

  g_assert_cmpint (cogl_primitive_get_n_vertices (prim_a),
                   ==,
                   cogl_primitive_get_n_vertices (prim_b));

  g_assert_cmpint (cogl_primitive_get_mode (prim_a),
                   ==,
                   cogl_primitive_get_mode (prim_b));

  g_assert (cogl_primitive_get_indices (prim_a) ==
            cogl_primitive_get_indices (prim_b));

  cogl_object_unref (prim_a);
  cogl_object_unref (prim_b);
  cogl_object_unref (indices);

  for (i = 0; i < N_ATTRIBS; i++)
    cogl_object_unref (attributes[i]);

  cogl_object_unref (buffer);
}

void
test_primitive (void)
{
  TestState state;

  state.fb_width = cogl_framebuffer_get_width (test_fb);
  state.fb_height = cogl_framebuffer_get_height (test_fb);

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 state.fb_width,
                                 state.fb_height,
                                 -1,
                                 100);

  test_paint (&state);
  test_copy (&state);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
