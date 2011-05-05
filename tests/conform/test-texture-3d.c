#include <clutter/clutter.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0xff, 0x0, 0xff };

#define TEX_WIDTH        4
#define TEX_HEIGHT       8
#define TEX_DEPTH        16
/* Leave four bytes of padding between each row */
#define TEX_ROWSTRIDE    (TEX_WIDTH * 4 + 4)
/* Leave four rows of padding between each image */
#define TEX_IMAGE_STRIDE ((TEX_HEIGHT + 4) * TEX_ROWSTRIDE)

static CoglHandle
create_texture_3d (void)
{
  int x, y, z;
  guint8 *data = g_malloc (TEX_IMAGE_STRIDE * TEX_DEPTH);
  guint8 *p = data;
  CoglHandle tex;
  GError *error = NULL;

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

  tex = cogl_texture_3d_new_from_data (TEX_WIDTH, TEX_HEIGHT, TEX_DEPTH,
                                       COGL_TEXTURE_NO_AUTO_MIPMAP,
                                       COGL_PIXEL_FORMAT_RGBA_8888,
                                       COGL_PIXEL_FORMAT_ANY,
                                       TEX_ROWSTRIDE,
                                       TEX_IMAGE_STRIDE,
                                       data,
                                       &error);

  if (tex == COGL_INVALID_HANDLE)
    {
      g_assert (error != NULL);
      g_warning ("Failed to create 3D texture: %s", error->message);
      g_assert_not_reached ();
    }

  g_free (data);

  return tex;
}

static void
draw_frame (void)
{
  CoglHandle tex = create_texture_3d ();
  CoglHandle material = cogl_material_new ();
  typedef struct { float x, y, s, t, r; } Vert;
  CoglHandle vbo, indices;
  Vert *verts, *v;
  int i;

  cogl_material_set_layer (material, 0, tex);
  cogl_handle_unref (tex);
  cogl_material_set_layer_filters (material, 0,
                                   COGL_MATERIAL_FILTER_NEAREST,
                                   COGL_MATERIAL_FILTER_NEAREST);
  cogl_set_source (material);
  cogl_handle_unref (material);

  /* Render the texture repeated horizontally twice using a regular
     cogl rectangle. This should end up with the r texture coordinates
     as zero */
  cogl_rectangle_with_texture_coords (0.0f, 0.0f, TEX_WIDTH * 2, TEX_HEIGHT,
                                      0.0f, 0.0f, 2.0f, 1.0f);

  /* Render all of the images in the texture using coordinates from a VBO */
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

  vbo = cogl_vertex_buffer_new (4 * TEX_DEPTH);
  cogl_vertex_buffer_add (vbo, "gl_Vertex",
                          2, COGL_ATTRIBUTE_TYPE_FLOAT, FALSE,
                          sizeof (Vert),
                          &verts->x);
  cogl_vertex_buffer_add (vbo, "gl_MultiTexCoord0",
                          3, COGL_ATTRIBUTE_TYPE_FLOAT, FALSE,
                          sizeof (Vert),
                          &verts->s);
  cogl_vertex_buffer_submit (vbo);

  g_free (verts);

  indices = cogl_vertex_buffer_indices_get_for_quads (6 * TEX_DEPTH);

  cogl_vertex_buffer_draw_elements (vbo,
                                    COGL_VERTICES_MODE_TRIANGLES,
                                    indices,
                                    0, TEX_DEPTH * 4 - 1,
                                    0, TEX_DEPTH * 6);

  cogl_handle_unref (vbo);
}

static void
validate_block (int block_x, int block_y, int z)
{
  guint8 *data, *p;
  int x, y;

  p = data = g_malloc (TEX_WIDTH * TEX_HEIGHT * 4);

  cogl_read_pixels (block_x * TEX_WIDTH, block_y * TEX_HEIGHT,
                    TEX_WIDTH, TEX_HEIGHT,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    data);

  for (y = 0; y < TEX_HEIGHT; y++)
    for (x = 0; x < TEX_WIDTH; x++)
      {
        g_assert_cmpint (p[0], ==, 255 - x * 8);
        g_assert_cmpint (p[1], ==, y * 8);
        g_assert_cmpint (p[2], ==, 255 - z * 8);
        p += 4;
      }

  g_free (data);
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
on_paint (void)
{
  draw_frame ();

  validate_result ();

  /* Comment this out to see what the test paints */
  clutter_main_quit ();
}

void
test_cogl_texture_3d (TestUtilsGTestFixture *fixture,
                      void *data)
{
  ClutterActor *stage;
  unsigned int paint_handler;

  stage = clutter_stage_get_default ();

  /* Check whether GL supports the rectangle extension. If not we'll
     just assume the test passes */
  if (cogl_features_available (COGL_FEATURE_TEXTURE_3D))
    {
      clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

      paint_handler = g_signal_connect_after (stage, "paint",
                                              G_CALLBACK (on_paint), NULL);

      clutter_actor_show (stage);

      clutter_main ();

      g_signal_handler_disconnect (stage, paint_handler);

      if (g_test_verbose ())
        g_print ("OK\n");
    }
  else if (g_test_verbose ())
    g_print ("Skipping\n");
}

