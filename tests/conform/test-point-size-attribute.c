#include <cogl/cogl.h>

#include "test-utils.h"

/* This test assumes the GL driver supports point sizes up to 16
   pixels. Cogl should probably have some way of querying the size so
   we start from that instead */
#define MAX_POINT_SIZE 16
#define MIN_POINT_SIZE 4
#define N_POINTS (MAX_POINT_SIZE - MIN_POINT_SIZE + 1)
/* The size of the area that we'll paint each point in */
#define POINT_BOX_SIZE (MAX_POINT_SIZE * 2)

typedef struct
{
  float x, y;
  float point_size;
} PointVertex;

static int
calc_coord_offset (int pos, int pos_index, int point_size)
{
  switch (pos_index)
    {
    case 0: return pos - point_size / 2 - 2;
    case 1: return pos - point_size / 2 + 2;
    case 2: return pos + point_size / 2 - 2;
    case 3: return pos + point_size / 2 + 2;
    }

  g_assert_not_reached ();
}

static void
verify_point_size (CoglFramebuffer *test_fb,
                   int x_pos,
                   int y_pos,
                   int point_size)
{
  int y, x;

  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      {
        CoglBool in_point = x >= 1 && x <= 2 && y >= 1 && y <= 2;
        uint32_t expected_pixel = in_point ? 0x00ff00ff : 0xff0000ff;

        test_utils_check_pixel (test_fb,
                                calc_coord_offset (x_pos, x, point_size),
                                calc_coord_offset (y_pos, y, point_size),
                                expected_pixel);
      }
}

static CoglPrimitive *
create_primitive (const char *attribute_name)
{
  PointVertex vertices[N_POINTS];
  CoglAttributeBuffer *buffer;
  CoglAttribute *attributes[2];
  CoglPrimitive *prim;
  int i;

  for (i = 0; i < N_POINTS; i++)
    {
      vertices[i].x = i * POINT_BOX_SIZE + POINT_BOX_SIZE / 2;
      vertices[i].y = POINT_BOX_SIZE / 2;
      vertices[i].point_size = MAX_POINT_SIZE - i;
    }

  buffer = cogl_attribute_buffer_new (test_ctx,
                                      sizeof (vertices),
                                      vertices);

  attributes[0] = cogl_attribute_new (buffer,
                                      "cogl_position_in",
                                      sizeof (PointVertex),
                                      G_STRUCT_OFFSET (PointVertex, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (buffer,
                                      attribute_name,
                                      sizeof (PointVertex),
                                      G_STRUCT_OFFSET (PointVertex, point_size),
                                      1, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  prim = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_POINTS,
                                             N_POINTS,
                                             attributes,
                                             2 /* n_attributes */);

  for (i = 0; i < 2; i++)
    cogl_object_unref (attributes[i]);

  return prim;
}

static void
do_test (const char *attribute_name,
         void (* pipeline_setup_func) (CoglPipeline *pipeline))
{
  int fb_width = cogl_framebuffer_get_width (test_fb);
  int fb_height = cogl_framebuffer_get_height (test_fb);
  CoglPrimitive *primitive;
  CoglPipeline *pipeline;
  int i;

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0, /* x_1, y_1 */
                                 fb_width, /* x_2 */
                                 fb_height /* y_2 */,
                                 -1, 100 /* near/far */);

  cogl_framebuffer_clear4f (test_fb,
                            COGL_BUFFER_BIT_COLOR,
                            1.0f, 0.0f, 0.0f, 1.0f);

  primitive = create_primitive (attribute_name);
  pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_color4ub (pipeline, 0x00, 0xff, 0x00, 0xff);
  cogl_pipeline_set_per_vertex_point_size (pipeline, TRUE, NULL);
  if (pipeline_setup_func)
    pipeline_setup_func (pipeline);
  cogl_framebuffer_draw_primitive (test_fb, pipeline, primitive);
  cogl_object_unref (pipeline);
  cogl_object_unref (primitive);

  /* Verify all of the points where drawn at the right size */
  for (i = 0; i < N_POINTS; i++)
    verify_point_size (test_fb,
                       i * POINT_BOX_SIZE + POINT_BOX_SIZE / 2, /* x */
                       POINT_BOX_SIZE / 2, /* y */
                       MAX_POINT_SIZE - i /* point size */);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

void
test_point_size_attribute (void)
{
  do_test ("cogl_point_size_in", NULL);
}

static void
setup_snippet (CoglPipeline *pipeline)
{
  CoglSnippet *snippet;

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_POINT_SIZE,
                              "attribute float "
                              "my_super_duper_point_size_attrib;\n",
                              NULL);
  cogl_snippet_set_replace (snippet,
                            "cogl_point_size_out = "
                            "my_super_duper_point_size_attrib;\n");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);
}

void
test_point_size_attribute_snippet (void)
{
  do_test ("my_super_duper_point_size_attrib", setup_snippet);
}
