#include <cogl/cogl2-experimental.h>
#include <string.h>

#include "test-utils.h"

#define TEX_WIDTH        4
#define TEX_HEIGHT       8
#define TEX_DEPTH        16
/* Leave four bytes of padding between each row */
#define TEX_ROWSTRIDE    (TEX_WIDTH * 4 + 4)
/* Leave four rows of padding between each image */
#define TEX_IMAGE_STRIDE ((TEX_HEIGHT + 4) * TEX_ROWSTRIDE)

typedef struct _TestState
{
  int fb_width;
  int fb_height;
} TestState;

static CoglTexture3D *
create_texture_3d (CoglContext *context)
{
  int x, y, z;
  uint8_t *data = g_malloc (TEX_IMAGE_STRIDE * TEX_DEPTH);
  uint8_t *p = data;
  CoglTexture3D *tex;
  CoglError *error = NULL;

  for (z = 0; z < TEX_DEPTH; z++)
    {
      for (y = 0; y < TEX_HEIGHT; y++)
        {
          for (x = 0; x < TEX_WIDTH; x++)
            {
              /* Set red, green, blue to values based on x, y, z */
              *(p++) = 255 - x * 8;
              *(p++) = y * 8;
              *(p++) = 255 - z * 8;
              /* Fully opaque */
              *(p++) = 0xff;
            }

          /* Set the padding between rows to 0xde */
          memset (p, 0xde, TEX_ROWSTRIDE - (TEX_WIDTH * 4));
          p += TEX_ROWSTRIDE - (TEX_WIDTH * 4);
        }
      /* Set the padding between images to 0xad */
      memset (p, 0xba, TEX_IMAGE_STRIDE - (TEX_HEIGHT * TEX_ROWSTRIDE));
      p += TEX_IMAGE_STRIDE - (TEX_HEIGHT * TEX_ROWSTRIDE);
    }

  tex = cogl_texture_3d_new_from_data (context,
                                       TEX_WIDTH, TEX_HEIGHT, TEX_DEPTH,
                                       COGL_PIXEL_FORMAT_RGBA_8888,
                                       TEX_ROWSTRIDE,
                                       TEX_IMAGE_STRIDE,
                                       data,
                                       &error);

  if (tex == NULL)
    {
      g_assert (error != NULL);
      g_warning ("Failed to create 3D texture: %s", error->message);
      g_assert_not_reached ();
    }

  g_free (data);

  return tex;
}

static void
draw_frame (TestState *state)
{
  CoglTexture *tex = create_texture_3d (test_ctx);
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);
  typedef struct { float x, y, s, t, r; } Vert;
  CoglPrimitive *primitive;
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[2];
  Vert *verts, *v;
  int i;

  cogl_pipeline_set_layer_texture (pipeline, 0, tex);
  cogl_object_unref (tex);
  cogl_pipeline_set_layer_filters (pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  /* Render the texture repeated horizontally twice using a regular
     cogl rectangle. This should end up with the r texture coordinates
     as zero */
  cogl_framebuffer_draw_textured_rectangle (test_fb, pipeline,
                                            0.0f, 0.0f, TEX_WIDTH * 2, TEX_HEIGHT,
                                            0.0f, 0.0f, 2.0f, 1.0f);

  /* Render all of the images in the texture using coordinates from a
     CoglPrimitive */
  v = verts = g_new (Vert, 4 * TEX_DEPTH);
  for (i = 0; i < TEX_DEPTH; i++)
    {
      float r = (i + 0.5f) / TEX_DEPTH;

      v->x = i * TEX_WIDTH;
      v->y = TEX_HEIGHT;
      v->s = 0;
      v->t = 0;
      v->r = r;
      v++;

      v->x = i * TEX_WIDTH;
      v->y = TEX_HEIGHT * 2;
      v->s = 0;
      v->t = 1;
      v->r = r;
      v++;

      v->x = i * TEX_WIDTH + TEX_WIDTH;
      v->y = TEX_HEIGHT * 2;
      v->s = 1;
      v->t = 1;
      v->r = r;
      v++;

      v->x = i * TEX_WIDTH + TEX_WIDTH;
      v->y = TEX_HEIGHT;
      v->s = 1;
      v->t = 0;
      v->r = r;
      v++;
    }

  attribute_buffer = cogl_attribute_buffer_new (test_ctx,
                                                4 * TEX_DEPTH * sizeof (Vert),
                                                verts);
  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (Vert),
                                      G_STRUCT_OFFSET (Vert, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (attribute_buffer,
                                      "cogl_tex_coord_in",
                                      sizeof (Vert),
                                      G_STRUCT_OFFSET (Vert, s),
                                      3, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                                  6 * TEX_DEPTH,
                                                  attributes,
                                                  2 /* n_attributes */);

  cogl_primitive_set_indices (primitive,
                              cogl_get_rectangle_indices (test_ctx,
                                                          TEX_DEPTH),
                              6 * TEX_DEPTH);

  cogl_primitive_draw (primitive, test_fb, pipeline);

  g_free (verts);

  cogl_object_unref (primitive);
  cogl_object_unref (attributes[0]);
  cogl_object_unref (attributes[1]);
  cogl_object_unref (attribute_buffer);
  cogl_object_unref (pipeline);
}

static void
validate_block (int block_x, int block_y, int z)
{
  int x, y;

  for (y = 0; y < TEX_HEIGHT; y++)
    for (x = 0; x < TEX_WIDTH; x++)
      test_utils_check_pixel_rgb (test_fb,
                                  block_x * TEX_WIDTH + x,
                                  block_y * TEX_HEIGHT + y,
                                  255 - x * 8,
                                  y * 8,
                                  255 - z * 8);
}

static void
validate_result (void)
{
  int i;

  validate_block (0, 0, 0);

  for (i = 0; i < TEX_DEPTH; i++)
    validate_block (i, 1, i);
}

static void
test_multi_texture (TestState *state)
{
  CoglPipeline *pipeline;
  CoglTexture3D *tex_3d;
  CoglTexture2D *tex_2d;
  uint8_t tex_data[4];

  cogl_framebuffer_clear4f (test_fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  /* Tests a pipeline that is using multi-texturing to combine a 3D
     texture with a 2D texture. The texture from another layer is
     sampled with TEXTURE_? just to pick up a specific bug that was
     happening with the ARBfp fragend */

  pipeline = cogl_pipeline_new (test_ctx);

  tex_data[0] = 0xff;
  tex_data[1] = 0x00;
  tex_data[2] = 0x00;
  tex_data[3] = 0xff;
  tex_2d = cogl_texture_2d_new_from_data (test_ctx,
                                          1, 1, /* width/height */
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          4, /* rowstride */
                                          tex_data,
                                          NULL);
  cogl_pipeline_set_layer_texture (pipeline, 0, tex_2d);

  tex_data[0] = 0x00;
  tex_data[1] = 0xff;
  tex_data[2] = 0x00;
  tex_data[3] = 0xff;
  tex_3d = cogl_texture_3d_new_from_data (test_ctx,
                                          1, 1, 1, /* width/height/depth */
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          4, /* rowstride */
                                          4, /* image_stride */
                                          tex_data,
                                          NULL);
  cogl_pipeline_set_layer_texture (pipeline, 1, tex_3d);

  cogl_pipeline_set_layer_combine (pipeline, 0,
                                   "RGBA = REPLACE(PREVIOUS)",
                                   NULL);
  cogl_pipeline_set_layer_combine (pipeline, 1,
                                   "RGBA = ADD(TEXTURE_0, TEXTURE_1)",
                                   NULL);

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 0, 0, 10, 10);

  test_utils_check_pixel (test_fb, 5, 5, 0xffff00ff);

  cogl_object_unref (tex_2d);
  cogl_object_unref (tex_3d);
  cogl_object_unref (pipeline);
}

void
test_texture_3d (void)
{
  TestState state;

  state.fb_width = cogl_framebuffer_get_width (test_fb);
  state.fb_height = cogl_framebuffer_get_height (test_fb);

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0, /* x_1, y_1 */
                                 state.fb_width, /* x_2 */
                                 state.fb_height /* y_2 */,
                                 -1, 100 /* near/far */);

  draw_frame (&state);
  validate_result ();

  test_multi_texture (&state);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
