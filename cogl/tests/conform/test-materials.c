#include "config.h"

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

#define QUAD_WIDTH 20

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24)
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16)
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8)
#define MASK_ALPHA(COLOR) (COLOR & 0xff)

typedef struct _TestState
{
  ClutterGeometry stage_geom;
} TestState;

static void
check_quad (int quad_x, int quad_y, uint32_t color)
{
  test_utils_check_pixel (x * QUAD_WIDTH + (QUAD_WIDTH / 2),
                          y * QUAD_WIDTH + (QUAD_WIDTH / 2),
                          color);
}

static void
test_material_with_primitives (TestState *state,
                               int x, int y,
                               uint32_t color)
{
  CoglTextureVertex verts[4] = {
    { .x = 0,          .y = 0,          .z = 0 },
    { .x = 0,          .y = QUAD_WIDTH, .z = 0 },
    { .x = QUAD_WIDTH, .y = QUAD_WIDTH, .z = 0 },
    { .x = QUAD_WIDTH, .y = 0,          .z = 0 },
  };
  CoglHandle vbo;

  cogl_push_matrix ();

  cogl_translate (x * QUAD_WIDTH, y * QUAD_WIDTH, 0);

  cogl_rectangle (0, 0, QUAD_WIDTH, QUAD_WIDTH);

  cogl_translate (0, QUAD_WIDTH, 0);
  cogl_polygon (verts, 4, FALSE);

  cogl_translate (0, QUAD_WIDTH, 0);
  vbo = cogl_vertex_buffer_new (4);
  cogl_vertex_buffer_add (vbo,
                          "gl_Vertex",
                          2, /* n components */
                          COGL_ATTRIBUTE_TYPE_FLOAT,
                          FALSE, /* normalized */
                          sizeof (CoglTextureVertex), /* stride */
                          verts);
  cogl_vertex_buffer_draw (vbo,
                           COGL_VERTICES_MODE_TRIANGLE_FAN,
                           0, /* first */
                           4); /* count */
  cogl_handle_unref (vbo);

  cogl_pop_matrix ();

  check_quad (x, y,   color);
  check_quad (x, y+1, color);
  check_quad (x, y+2, color);
}

static void
test_invalid_texture_layers (TestState *state, int x, int y)
{
  CoglHandle        material = cogl_material_new ();

  /* explicitly create a layer with an invalid handle. This may be desireable
   * if the user also sets a texture combine string that e.g. refers to a
   * constant color. */
  cogl_material_set_layer (material, 0, NULL);

  cogl_set_source (material);

  cogl_handle_unref (material);

  /* We expect a white fallback material to be used */
  test_material_with_primitives (state, x, y, 0xffffffff);
}

static void
test_using_all_layers (TestState *state, int x, int y)
{
  CoglHandle material = cogl_material_new ();
  uint8_t white_pixel[] = { 0xff, 0xff, 0xff, 0xff };
  uint8_t red_pixel[] = { 0xff, 0x00, 0x00, 0xff };
  CoglHandle white_texture;
  CoglHandle red_texture;
  GLint n_layers;
  int i;

  /* Create a material that uses the maximum number of layers. All but
     the last layer will use a solid white texture. The last layer
     will use a red texture. The layers will all be modulated together
     so the final fragment should be red. */

  white_texture = test_utils_texture_new_from_data (1, 1, TEST_UTILS_TEXTURE_NONE,
                                              COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                              COGL_PIXEL_FORMAT_ANY,
                                              4, white_pixel);
  red_texture = test_utils_texture_new_from_data (1, 1, TEST_UTILS_TEXTURE_NONE,
                                            COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                            COGL_PIXEL_FORMAT_ANY,
                                            4, red_pixel);

  /* FIXME: Cogl doesn't provide a way to query the maximum number of
     texture layers so for now we'll just ask GL directly. */
#ifdef HAVE_COGL_GLES2
  {
    GLint n_image_units, n_attribs;
    /* GLES 2 doesn't have GL_MAX_TEXTURE_UNITS and it uses
       GL_MAX_TEXTURE_IMAGE_UNITS instead */
    glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &n_image_units);
    /* Cogl needs a vertex attrib for each layer to upload the texture
       coordinates */
    glGetIntegerv (GL_MAX_VERTEX_ATTRIBS, &n_attribs);
    /* We can't use two of the attribs because they are used by the
       position and color */
    n_attribs -= 2;
    n_layers = MIN (n_attribs, n_image_units);
  }
#else
  glGetIntegerv (GL_MAX_TEXTURE_UNITS, &n_layers);
#endif
  /* FIXME: is this still true? */
  /* Cogl currently can't cope with more than 32 layers so we'll also
     limit the maximum to that. */
  if (n_layers > 32)
    n_layers = 32;

  for (i = 0; i < n_layers; i++)
    {
      cogl_material_set_layer_filters (material, i,
                                       COGL_MATERIAL_FILTER_NEAREST,
                                       COGL_MATERIAL_FILTER_NEAREST);
      cogl_material_set_layer (material, i,
                               i == n_layers - 1 ? red_texture : white_texture);
    }

  cogl_set_source (material);

  cogl_handle_unref (material);
  cogl_handle_unref (white_texture);
  cogl_handle_unref (red_texture);

  /* We expect the final fragment to be red */
  test_material_with_primitives (state, x, y, 0xff0000ff);
}

static void
test_invalid_texture_layers_with_constant_colors (TestState *state,
                                                  int x, int y)
{
  CoglHandle material = cogl_material_new ();
  CoglColor constant_color;

  /* explicitly create a layer with an invalid handle */
  cogl_material_set_layer (material, 0, NULL);

  /* ignore the fallback texture on the layer and use a constant color
     instead */
  cogl_color_init_from_4ub (&constant_color, 0, 0, 255, 255);
  cogl_material_set_layer_combine (material, 0,
                                   "RGBA=REPLACE(CONSTANT)",
                                   NULL);
  cogl_material_set_layer_combine_constant (material, 0, &constant_color);

  cogl_set_source (material);

  cogl_handle_unref (material);

  /* We expect the final fragments to be green */
  test_material_with_primitives (state, x, y, 0x0000ffff);
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  test_invalid_texture_layers (state,
                               0, 0 /* position */
                               );
  test_invalid_texture_layers_with_constant_colors (state,
                                                    1, 0 /* position */
                                                    );
  test_using_all_layers (state,
                         2, 0 /* position */
                         );

  /* Comment this out if you want visual feedback for what this test paints */
#if 1
  clutter_main_quit ();
#endif
}

static CoglBool
queue_redraw (void *stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_materials (TestUtilsGTestFixture *fixture,
                     void *data)
{
  TestState state;
  ClutterActor *stage;
  ClutterActor *group;
  unsigned int idle_source;

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_get_geometry (stage, &state.stage_geom);

  group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (stage);

  clutter_main ();

  g_source_remove (idle_source);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

