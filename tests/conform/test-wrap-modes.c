#include <clutter/clutter.h>
#include <string.h>

#include "test-conform-common.h"

#define TEX_SIZE 4

static const ClutterColor stage_color = { 0x80, 0x80, 0x80, 0xff };

typedef struct _TestState
{
  ClutterActor *stage;
  CoglHandle texture;
} TestState;

static CoglHandle
create_texture (CoglTextureFlags flags)
{
  guint8 *data = g_malloc (TEX_SIZE * TEX_SIZE * 4), *p = data;
  CoglHandle tex;
  int x, y;

  for (y = 0; y < TEX_SIZE; y++)
    for (x = 0; x < TEX_SIZE; x++)
      {
        *(p++) = 0;
        *(p++) = (x & 1) * 255;
        *(p++) = (y & 1) * 255;
        *(p++) = 255;
      }

  tex = cogl_texture_new_from_data (TEX_SIZE, TEX_SIZE, flags,
                                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                    COGL_PIXEL_FORMAT_ANY,
                                    TEX_SIZE * 4,
                                    data);

  g_free (data);

  return tex;
}

static CoglHandle
create_material (TestState *state,
                 CoglMaterialWrapMode wrap_mode_s,
                 CoglMaterialWrapMode wrap_mode_t)
{
  CoglHandle material;

  material = cogl_material_new ();
  cogl_material_set_layer (material, 0, state->texture);
  cogl_material_set_layer_filters (material, 0,
                                   COGL_MATERIAL_FILTER_NEAREST,
                                   COGL_MATERIAL_FILTER_NEAREST);
  cogl_material_set_layer_wrap_mode_s (material, 0, wrap_mode_s);
  cogl_material_set_layer_wrap_mode_t (material, 0, wrap_mode_t);

  return material;
}

static CoglMaterialWrapMode
test_wrap_modes[] =
  {
    COGL_MATERIAL_WRAP_MODE_REPEAT,
    COGL_MATERIAL_WRAP_MODE_REPEAT,

    COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE,
    COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE,

    COGL_MATERIAL_WRAP_MODE_REPEAT,
    COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE,

    COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE,
    COGL_MATERIAL_WRAP_MODE_REPEAT,

    COGL_MATERIAL_WRAP_MODE_AUTOMATIC,
    COGL_MATERIAL_WRAP_MODE_AUTOMATIC,

    COGL_MATERIAL_WRAP_MODE_AUTOMATIC,
    COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE
  };

static void
draw_tests (TestState *state)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_wrap_modes); i += 2)
    {
      CoglMaterialWrapMode wrap_mode_s, wrap_mode_t;
      CoglHandle material;

      /* Create a separate material for each pair of wrap modes so
         that we can verify whether the batch splitting works */
      wrap_mode_s = test_wrap_modes[i];
      wrap_mode_t = test_wrap_modes[i + 1];
      material = create_material (state, wrap_mode_s, wrap_mode_t);
      cogl_set_source (material);
      cogl_handle_unref (material);
      /* Render the material at four times the size of the texture */
      cogl_rectangle_with_texture_coords (i * TEX_SIZE, 0,
                                          (i + 2) * TEX_SIZE, TEX_SIZE * 2,
                                          0, 0, 2, 2);
    }
}

static const CoglTextureVertex vertices[4] =
  {
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, TEX_SIZE * 2, 0.0f, 0.0f, 2.0f },
    { TEX_SIZE * 2, TEX_SIZE * 2, 0.0f, 2.0f, 2.0f },
    { TEX_SIZE * 2, 0.0f, 0.0f, 2.0f, 0.0f }
  };

static void
draw_tests_polygon (TestState *state)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_wrap_modes); i += 2)
    {
      CoglMaterialWrapMode wrap_mode_s, wrap_mode_t;
      CoglHandle material;

      wrap_mode_s = test_wrap_modes[i];
      wrap_mode_t = test_wrap_modes[i + 1];
      material = create_material (state, wrap_mode_s, wrap_mode_t);
      cogl_set_source (material);
      cogl_handle_unref (material);
      cogl_push_matrix ();
      cogl_translate (TEX_SIZE * i, 0.0f, 0.0f);
      /* Render the material at four times the size of the texture */
      cogl_polygon (vertices, G_N_ELEMENTS (vertices), FALSE);
      cogl_pop_matrix ();
    }
}

static void
draw_tests_vbo (TestState *state)
{
  CoglHandle vbo;
  int i;

  vbo = cogl_vertex_buffer_new (4);
  cogl_vertex_buffer_add (vbo, "gl_Vertex", 3,
                          COGL_ATTRIBUTE_TYPE_FLOAT, FALSE,
                          sizeof (vertices[0]),
                          &vertices[0].x);
  cogl_vertex_buffer_add (vbo, "gl_MultiTexCoord0", 2,
                          COGL_ATTRIBUTE_TYPE_FLOAT, FALSE,
                          sizeof (vertices[0]),
                          &vertices[0].tx);
  cogl_vertex_buffer_submit (vbo);

  for (i = 0; i < G_N_ELEMENTS (test_wrap_modes); i += 2)
    {
      CoglMaterialWrapMode wrap_mode_s, wrap_mode_t;
      CoglHandle material;

      wrap_mode_s = test_wrap_modes[i];
      wrap_mode_t = test_wrap_modes[i + 1];
      material = create_material (state, wrap_mode_s, wrap_mode_t);
      cogl_set_source (material);
      cogl_handle_unref (material);
      cogl_push_matrix ();
      cogl_translate (TEX_SIZE * i, 0.0f, 0.0f);
      /* Render the material at four times the size of the texture */
      cogl_vertex_buffer_draw (vbo, COGL_VERTICES_MODE_TRIANGLE_FAN, 0, 4);
      cogl_pop_matrix ();
    }

  cogl_handle_unref (vbo);
}

static void
draw_frame (TestState *state)
{
  /* Draw the tests first with a non atlased texture */
  state->texture = create_texture (COGL_TEXTURE_NO_ATLAS);
  draw_tests (state);
  cogl_handle_unref (state->texture);

  /* Draw the tests again with a possible atlased texture. This should
     end up testing software repeats */
  state->texture = create_texture (COGL_TEXTURE_NONE);
  cogl_push_matrix ();
  cogl_translate (0.0f, TEX_SIZE * 2.0f, 0.0f);
  draw_tests (state);
  cogl_pop_matrix ();
  cogl_handle_unref (state->texture);

  /* Draw the tests using cogl_polygon */
  state->texture = create_texture (COGL_TEXTURE_NO_ATLAS);
  cogl_push_matrix ();
  cogl_translate (0.0f, TEX_SIZE * 4.0f, 0.0f);
  draw_tests_polygon (state);
  cogl_pop_matrix ();
  cogl_handle_unref (state->texture);

  /* Draw the tests using a vertex buffer */
  state->texture = create_texture (COGL_TEXTURE_NO_ATLAS);
  cogl_push_matrix ();
  cogl_translate (0.0f, TEX_SIZE * 6.0f, 0.0f);
  draw_tests_vbo (state);
  cogl_pop_matrix ();
  cogl_handle_unref (state->texture);
}

static void
validate_set (TestState *state, int offset)
{
  guint8 data[TEX_SIZE * 2 * TEX_SIZE * 2 * 4], *p;
  int x, y, i;

  for (i = 0; i < G_N_ELEMENTS (test_wrap_modes); i += 2)
    {
      CoglMaterialWrapMode wrap_mode_s, wrap_mode_t;

      wrap_mode_s = test_wrap_modes[i];
      wrap_mode_t = test_wrap_modes[i + 1];

      cogl_read_pixels (i * TEX_SIZE, offset * TEX_SIZE * 2,
                        TEX_SIZE * 2, TEX_SIZE * 2,
                        COGL_READ_PIXELS_COLOR_BUFFER,
                        COGL_PIXEL_FORMAT_RGBA_8888,
                        data);

      p = data;

      for (y = 0; y < TEX_SIZE * 2; y++)
        for (x = 0; x < TEX_SIZE * 2; x++)
          {
            guint8 green, blue;

            if (x < TEX_SIZE ||
                wrap_mode_s == COGL_MATERIAL_WRAP_MODE_REPEAT ||
                wrap_mode_s == COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
              green = (x & 1) * 255;
            else
              green = ((TEX_SIZE - 1) & 1) * 255;

            if (y < TEX_SIZE ||
                wrap_mode_t == COGL_MATERIAL_WRAP_MODE_REPEAT ||
                wrap_mode_t == COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
              blue = (y & 1) * 255;
            else
              blue = ((TEX_SIZE - 1) & 1) * 255;

            g_assert_cmpint (p[0], ==, 0);
            g_assert_cmpint (p[1], ==, green);
            g_assert_cmpint (p[2], ==, blue);

            p += 4;
          }
    }
}

static void
validate_result (TestState *state)
{
  validate_set (state, 0); /* non-atlased rectangle */
#if 0 /* this doesn't currently work */
  validate_set (state, 1); /* atlased rectangle */
#endif
  validate_set (state, 2); /* cogl_polygon */
  validate_set (state, 3); /* vertex buffer */

  /* Comment this out to see what the test paints */
  clutter_main_quit ();
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  draw_frame (state);

  validate_result (state);
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_cogl_wrap_modes (TestUtilsGTestFixture *fixture,
                      void *data)
{
  TestState state;
  unsigned int idle_source;
  unsigned int paint_handler;

  state.stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (state.stage), &stage_color);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, state.stage);

  paint_handler = g_signal_connect_after (state.stage, "paint",
                                          G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (state.stage);

  clutter_main ();

  g_source_remove (idle_source);
  g_signal_handler_disconnect (state.stage, paint_handler);

  if (g_test_verbose ())
    g_print ("OK\n");
}
