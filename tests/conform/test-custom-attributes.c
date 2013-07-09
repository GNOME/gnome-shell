#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  CoglPipeline *pipeline;
} TestState;

typedef struct
{
  int16_t x, y;
  float r, g, b, a;
} FloatVert;

typedef struct
{
  int16_t x, y;
  uint8_t r, g, b, a;
} ByteVert;

typedef struct
{
  int16_t x, y;
} ShortVert;

static void
test_float_verts (TestState *state, int offset_x, int offset_y)
{
  CoglAttribute *attributes[2];
  CoglAttributeBuffer *buffer;
  CoglPrimitive *primitive;

  static const FloatVert float_verts[] =
    {
      { 0, 10, /**/ 1, 0, 0, 1 },
      { 10, 10, /**/ 1, 0, 0, 1 },
      { 5, 0, /**/ 1, 0, 0, 1 },

      { 10, 10, /**/ 0, 1, 0, 1 },
      { 20, 10, /**/ 0, 1, 0, 1 },
      { 15, 0, /**/ 0, 1, 0, 1 }
    };

  buffer = cogl_attribute_buffer_new (test_ctx,
                                      sizeof (float_verts), float_verts);
  attributes[0] = cogl_attribute_new (buffer,
                                      "cogl_position_in",
                                      sizeof (FloatVert),
                                      G_STRUCT_OFFSET (FloatVert, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_SHORT);
  attributes[1] = cogl_attribute_new (buffer,
                                      "color",
                                      sizeof (FloatVert),
                                      G_STRUCT_OFFSET (FloatVert, r),
                                      4, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  cogl_framebuffer_push_matrix (test_fb);
  cogl_framebuffer_translate (test_fb, offset_x, offset_y, 0.0f);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                                  6, /* n_vertices */
                                                  attributes,
                                                  2); /* n_attributes */
  cogl_primitive_draw (primitive, test_fb, state->pipeline);
  cogl_object_unref (primitive);

  cogl_framebuffer_pop_matrix (test_fb);

  cogl_object_unref (attributes[1]);
  cogl_object_unref (attributes[0]);
  cogl_object_unref (buffer);

  test_utils_check_pixel (test_fb, offset_x + 5, offset_y + 5, 0xff0000ff);
  test_utils_check_pixel (test_fb, offset_x + 15, offset_y + 5, 0x00ff00ff);
}

static void
test_byte_verts (TestState *state, int offset_x, int offset_y)
{
  CoglAttribute *attributes[2];
  CoglAttributeBuffer *buffer, *unnorm_buffer;
  CoglPrimitive *primitive;

  static const ByteVert norm_verts[] =
    {
      { 0, 10, /**/ 255, 0, 0, 255 },
      { 10, 10, /**/ 255, 0, 0, 255 },
      { 5, 0, /**/ 255, 0, 0, 255 },

      { 10, 10, /**/ 0, 255, 0, 255 },
      { 20, 10, /**/ 0, 255, 0, 255 },
      { 15, 0, /**/ 0, 255, 0, 255 }
    };

  static const ByteVert unnorm_verts[] =
    {
      { 0, 0, /**/ 0, 0, 1, 1 },
      { 0, 0, /**/ 0, 0, 1, 1 },
      { 0, 0, /**/ 0, 0, 1, 1 },
    };

  buffer = cogl_attribute_buffer_new (test_ctx,
                                      sizeof (norm_verts), norm_verts);
  attributes[0] = cogl_attribute_new (buffer,
                                      "cogl_position_in",
                                      sizeof (ByteVert),
                                      G_STRUCT_OFFSET (ByteVert, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_SHORT);
  attributes[1] = cogl_attribute_new (buffer,
                                      "color",
                                      sizeof (ByteVert),
                                      G_STRUCT_OFFSET (ByteVert, r),
                                      4, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
  cogl_attribute_set_normalized (attributes[1], TRUE);

  cogl_framebuffer_push_matrix (test_fb);
  cogl_framebuffer_translate (test_fb, offset_x, offset_y, 0.0f);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                                  6, /* n_vertices */
                                                  attributes,
                                                  2); /* n_attributes */
  cogl_primitive_draw (primitive, test_fb, state->pipeline);
  cogl_object_unref (primitive);

  cogl_object_unref (attributes[1]);

  /* Test again with unnormalized attributes */
  unnorm_buffer = cogl_attribute_buffer_new (test_ctx,
                                             sizeof (unnorm_verts),
                                             unnorm_verts);
  attributes[1] = cogl_attribute_new (unnorm_buffer,
                                      "color",
                                      sizeof (ByteVert),
                                      G_STRUCT_OFFSET (ByteVert, r),
                                      4, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_BYTE);

  cogl_framebuffer_translate (test_fb, 20, 0, 0);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                                  3, /* n_vertices */
                                                  attributes,
                                                  2); /* n_attributes */
  cogl_primitive_draw (primitive, test_fb, state->pipeline);
  cogl_object_unref (primitive);

  cogl_framebuffer_pop_matrix (test_fb);

  cogl_object_unref (attributes[0]);
  cogl_object_unref (attributes[1]);
  cogl_object_unref (buffer);
  cogl_object_unref (unnorm_buffer);

  test_utils_check_pixel (test_fb, offset_x + 5, offset_y + 5, 0xff0000ff);
  test_utils_check_pixel (test_fb, offset_x + 15, offset_y + 5, 0x00ff00ff);
  test_utils_check_pixel (test_fb, offset_x + 25, offset_y + 5, 0x0000ffff);
}

static void
test_short_verts (TestState *state, int offset_x, int offset_y)
{
  CoglAttribute *attributes[1];
  CoglAttributeBuffer *buffer;
  CoglPipeline *pipeline, *pipeline2;
  CoglSnippet *snippet;
  CoglPrimitive *primitive;

  static const ShortVert short_verts[] =
    {
      { -10, -10 },
      { -1, -10 },
      { -5, -1 }
    };

  pipeline = cogl_pipeline_new (test_ctx);
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX_TRANSFORM,
                              "attribute vec2 pos;",
                              NULL);
  cogl_snippet_set_replace (snippet,
                            "cogl_position_out = "
                            "cogl_modelview_projection_matrix * "
                            "vec4 (pos, 0.0, 1.0);");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_pipeline_set_color4ub (pipeline, 255, 0, 0, 255);

  buffer = cogl_attribute_buffer_new (test_ctx,
                                      sizeof (short_verts), short_verts);
  attributes[0] = cogl_attribute_new (buffer,
                                      "pos",
                                      sizeof (ShortVert),
                                      G_STRUCT_OFFSET (ShortVert, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_SHORT);

  cogl_framebuffer_push_matrix (test_fb);
  cogl_framebuffer_translate (test_fb,
                              offset_x + 10.0f,
                              offset_y + 10.0f,
                              0.0f);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                                  3, /* n_vertices */
                                                  attributes,
                                                  1); /* n_attributes */
  cogl_primitive_draw (primitive, test_fb, pipeline);
  cogl_object_unref (primitive);

  cogl_framebuffer_pop_matrix (test_fb);

  cogl_object_unref (attributes[0]);

  /* Test again treating the attribute as unsigned */
  attributes[0] = cogl_attribute_new (buffer,
                                      "pos",
                                      sizeof (ShortVert),
                                      G_STRUCT_OFFSET (ShortVert, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_UNSIGNED_SHORT);

  pipeline2 = cogl_pipeline_copy (pipeline);
  cogl_pipeline_set_color4ub (pipeline2, 0, 255, 0, 255);

  cogl_framebuffer_push_matrix (test_fb);
  cogl_framebuffer_translate (test_fb,
                              offset_x + 10.0f - 65525.0f,
                              offset_y - 65525,
                              0.0f);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                                  3, /* n_vertices */
                                                  attributes,
                                                  1); /* n_attributes */
  cogl_primitive_draw (primitive, test_fb, pipeline2);
  cogl_object_unref (primitive);

  cogl_framebuffer_pop_matrix (test_fb);

  cogl_object_unref (attributes[0]);

  cogl_object_unref (pipeline2);
  cogl_object_unref (pipeline);
  cogl_object_unref (buffer);

  test_utils_check_pixel (test_fb, offset_x + 5, offset_y + 5, 0xff0000ff);
  test_utils_check_pixel (test_fb, offset_x + 15, offset_y + 5, 0x00ff00ff);
}

static void
paint (TestState *state)
{
  cogl_framebuffer_clear4f (test_fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  test_float_verts (state, 0, 0);
  test_byte_verts (state, 0, 10);
  test_short_verts (state, 0, 20);
}

void
test_custom_attributes (void)
{
  CoglSnippet *snippet;
  TestState state;

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cogl_framebuffer_get_width (test_fb),
                                 cogl_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  state.pipeline = cogl_pipeline_new (test_ctx);
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                              "attribute vec4 color;",
                              "cogl_color_out = color;");
  cogl_pipeline_add_snippet (state.pipeline, snippet);

  paint (&state);

  cogl_object_unref (state.pipeline);
  cogl_object_unref (snippet);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
