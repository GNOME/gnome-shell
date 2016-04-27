#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

#define LONG_ARRAY_SIZE 128

typedef struct _TestState
{
  CoglPipeline *pipeline_red;
  CoglPipeline *pipeline_green;
  CoglPipeline *pipeline_blue;

  CoglPipeline *matrix_pipeline;
  CoglPipeline *vector_pipeline;
  CoglPipeline *int_pipeline;

  CoglPipeline *long_pipeline;
  int long_uniform_locations[LONG_ARRAY_SIZE];
} TestState;

static const char
color_source[] =
  "uniform float red, green, blue;\n"
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  cogl_color_out = vec4 (red, green, blue, 1.0);\n"
  "}\n";

static const char
matrix_source[] =
  "uniform mat4 matrix_array[4];\n"
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  vec4 color = vec4 (0.0, 0.0, 0.0, 1.0);\n"
  "  int i;\n"
  "\n"
  "  for (i = 0; i < 4; i++)\n"
  "    color = matrix_array[i] * color;\n"
  "\n"
  "  cogl_color_out = color;\n"
  "}\n";

static const char
vector_source[] =
  "uniform vec4 vector_array[2];\n"
  "uniform vec3 short_vector;\n"
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  cogl_color_out = (vector_array[0] +\n"
  "                    vector_array[1] +\n"
  "                    vec4 (short_vector, 1.0));\n"
  "}\n";

static const char
int_source[] =
  "uniform ivec4 vector_array[2];\n"
  "uniform int single_value;\n"
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  cogl_color_out = (vec4 (vector_array[0]) +\n"
  "                    vec4 (vector_array[1]) +\n"
  "                    vec4 (float (single_value), 0.0, 0.0, 255.0)) / 255.0;\n"
  "}\n";

static const char
long_source[] =
  "uniform int long_array[" G_STRINGIFY (LONG_ARRAY_SIZE) "];\n"
  "const int last_index = " G_STRINGIFY (LONG_ARRAY_SIZE) " - 1;\n"
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  cogl_color_out = vec4 (float (long_array[last_index]), 0.0, 0.0, 1.0);\n"
  "}\n";

static CoglPipeline *
create_pipeline_for_shader (TestState *state, const char *shader_source)
{
  CoglPipeline *pipeline;
  CoglHandle shader;
  CoglHandle program;

  pipeline = cogl_pipeline_new (test_ctx);

  shader = cogl_create_shader (COGL_SHADER_TYPE_FRAGMENT);
  cogl_shader_source (shader, shader_source);

  program = cogl_create_program ();
  cogl_program_attach_shader (program, shader);

  cogl_pipeline_set_user_program (pipeline, program);

  cogl_handle_unref (shader);
  cogl_handle_unref (program);

  return pipeline;
}

static void
init_state (TestState *state)
{
  int uniform_location;

  state->pipeline_red = create_pipeline_for_shader (state, color_source);

  uniform_location =
    cogl_pipeline_get_uniform_location (state->pipeline_red, "red");
  cogl_pipeline_set_uniform_1f (state->pipeline_red, uniform_location, 1.0f);
  uniform_location =
    cogl_pipeline_get_uniform_location (state->pipeline_red, "green");
  cogl_pipeline_set_uniform_1f (state->pipeline_red, uniform_location, 0.0f);
  uniform_location =
    cogl_pipeline_get_uniform_location (state->pipeline_red, "blue");
  cogl_pipeline_set_uniform_1f (state->pipeline_red, uniform_location, 0.0f);

  state->pipeline_green = cogl_pipeline_copy (state->pipeline_red);
  uniform_location =
    cogl_pipeline_get_uniform_location (state->pipeline_green, "green");
  cogl_pipeline_set_uniform_1f (state->pipeline_green, uniform_location, 1.0f);

  state->pipeline_blue = cogl_pipeline_copy (state->pipeline_red);
  uniform_location =
    cogl_pipeline_get_uniform_location (state->pipeline_blue, "blue");
  cogl_pipeline_set_uniform_1f (state->pipeline_blue, uniform_location, 1.0f);

  state->matrix_pipeline = create_pipeline_for_shader (state, matrix_source);
  state->vector_pipeline = create_pipeline_for_shader (state, vector_source);
  state->int_pipeline = create_pipeline_for_shader (state, int_source);

  state->long_pipeline = NULL;
}

static void
init_long_pipeline_state (TestState *state)
{
  int i;

  state->long_pipeline = create_pipeline_for_shader (state, long_source);

  /* This tries to lookup a large number of uniform names to make sure
     that the bitmask of overriden uniforms flows over the size of a
     single long so that it has to resort to allocating it */
  for (i = 0; i < LONG_ARRAY_SIZE; i++)
    {
      char *uniform_name = g_strdup_printf ("long_array[%i]", i);
      state->long_uniform_locations[i] =
        cogl_pipeline_get_uniform_location (state->long_pipeline,
                                            uniform_name);
      g_free (uniform_name);
    }
}

static void
destroy_state (TestState *state)
{
  cogl_object_unref (state->pipeline_red);
  cogl_object_unref (state->pipeline_green);
  cogl_object_unref (state->pipeline_blue);
  cogl_object_unref (state->matrix_pipeline);
  cogl_object_unref (state->vector_pipeline);
  cogl_object_unref (state->int_pipeline);

  if (state->long_pipeline)
    cogl_object_unref (state->long_pipeline);
}

static void
paint_pipeline (CoglPipeline *pipeline, int pos)
{
  cogl_framebuffer_draw_rectangle (test_fb, pipeline,
                                   pos * 10, 0, pos * 10 + 10, 10);
}

static void
paint_color_pipelines (TestState *state)
{
  CoglPipeline *temp_pipeline;
  int uniform_location;
  int i;

  /* Paint with the first pipeline that sets the uniforms to bright
     red */
  paint_pipeline (state->pipeline_red, 0);

  /* Paint with the two other pipelines. These inherit from the red
     pipeline and only override one other component. The values for
     the two other components should be inherited from the red
     pipeline. */
  paint_pipeline (state->pipeline_green, 1);
  paint_pipeline (state->pipeline_blue, 2);

  /* Try modifying a single pipeline for multiple rectangles */
  temp_pipeline = cogl_pipeline_copy (state->pipeline_green);
  uniform_location = cogl_pipeline_get_uniform_location (temp_pipeline,
                                                         "green");

  for (i = 0; i <= 8; i++)
    {
      cogl_pipeline_set_uniform_1f (temp_pipeline, uniform_location,
                                    i / 8.0f);
      paint_pipeline (temp_pipeline, i + 3);
    }

  cogl_object_unref (temp_pipeline);
}

static void
paint_matrix_pipeline (CoglPipeline *pipeline)
{
  CoglMatrix matrices[4];
  float matrix_floats[16 * 4];
  int uniform_location;
  int i;

  for (i = 0; i < 4; i++)
    cogl_matrix_init_identity (matrices + i);

  /* Use the first matrix to make the color red */
  cogl_matrix_translate (matrices + 0, 1.0f, 0.0f, 0.0f);

  /* Rotate the vertex so that it ends up green */
  cogl_matrix_rotate (matrices + 1, 90.0f, 0.0f, 0.0f, 1.0f);

  /* Scale the vertex so it ends up halved */
  cogl_matrix_scale (matrices + 2, 0.5f, 0.5f, 0.5f);

  /* Add a blue component in the final matrix. The final matrix is
     uploaded as transposed so we need to transpose first to cancel
     that out */
  cogl_matrix_translate (matrices + 3, 0.0f, 0.0f, 1.0f);
  cogl_matrix_transpose (matrices + 3);

  for (i = 0; i < 4; i++)
    memcpy (matrix_floats + i * 16,
            cogl_matrix_get_array (matrices + i),
            sizeof (float) * 16);

  /* Set the first three matrices as transposed */
  uniform_location =
    cogl_pipeline_get_uniform_location (pipeline, "matrix_array");
  cogl_pipeline_set_uniform_matrix (pipeline,
                                    uniform_location,
                                    4, /* dimensions */
                                    3, /* count */
                                    FALSE, /* not transposed */
                                    matrix_floats);

  /* Set the last matrix as untransposed */
  uniform_location =
    cogl_pipeline_get_uniform_location (pipeline, "matrix_array[3]");
  cogl_pipeline_set_uniform_matrix (pipeline,
                                    uniform_location,
                                    4, /* dimensions */
                                    1, /* count */
                                    TRUE, /* transposed */
                                    matrix_floats + 16 * 3);

  paint_pipeline (pipeline, 12);
}

static void
paint_vector_pipeline (CoglPipeline *pipeline)
{
  float vector_array_values[] = { 1.0f, 0.0f, 0.0f, 0.0f,
                                  0.0f, 1.0f, 0.0f, 0.0f };
  float short_vector_values[] = { 0.0f, 0.0f, 1.0f };
  int uniform_location;

  uniform_location =
    cogl_pipeline_get_uniform_location (pipeline, "vector_array");
  cogl_pipeline_set_uniform_float (pipeline,
                                   uniform_location,
                                   4, /* n_components */
                                   2, /* count */
                                   vector_array_values);

  uniform_location =
    cogl_pipeline_get_uniform_location (pipeline, "short_vector");
  cogl_pipeline_set_uniform_float (pipeline,
                                   uniform_location,
                                   3, /* n_components */
                                   1, /* count */
                                   short_vector_values);

  paint_pipeline (pipeline, 13);
}

static void
paint_int_pipeline (CoglPipeline *pipeline)
{
  int vector_array_values[] = { 0x00, 0x00, 0xff, 0x00,
                                0x00, 0xff, 0x00, 0x00 };
  int single_value = 0x80;
  int uniform_location;

  uniform_location =
    cogl_pipeline_get_uniform_location (pipeline, "vector_array");
  cogl_pipeline_set_uniform_int (pipeline,
                                 uniform_location,
                                 4, /* n_components */
                                 2, /* count */
                                 vector_array_values);

  uniform_location =
    cogl_pipeline_get_uniform_location (pipeline, "single_value");
  cogl_pipeline_set_uniform_1i (pipeline,
                                uniform_location,
                                single_value);

  paint_pipeline (pipeline, 14);
}

static void
paint_long_pipeline (TestState *state)
{
  int i;

  for (i = 0; i < LONG_ARRAY_SIZE; i++)
    {
      int location = state->long_uniform_locations[i];

      cogl_pipeline_set_uniform_1i (state->long_pipeline,
                                    location,
                                    i == LONG_ARRAY_SIZE - 1);
    }

  paint_pipeline (state->long_pipeline, 15);
}

static void
paint (TestState *state)
{
  cogl_framebuffer_clear4f (test_fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  paint_color_pipelines (state);
  paint_matrix_pipeline (state->matrix_pipeline);
  paint_vector_pipeline (state->vector_pipeline);
  paint_int_pipeline (state->int_pipeline);
}

static void
check_pos (int pos, uint32_t color)
{
  test_utils_check_pixel (test_fb, pos * 10 + 5, 5, color);
}

static void
validate_result (void)
{
  int i;

  check_pos (0, 0xff0000ff);
  check_pos (1, 0xffff00ff);
  check_pos (2, 0xff00ffff);

  for (i = 0; i <= 8; i++)
    {
      int green_value = i / 8.0f * 255.0f + 0.5f;
      check_pos (i + 3, 0xff0000ff + (green_value << 16));
    }

  check_pos (12, 0x0080ffff);
  check_pos (13, 0xffffffff);
  check_pos (14, 0x80ffffff);
}

static void
validate_long_pipeline_result (void)
{
  check_pos (15, 0xff0000ff);
}

void
test_pipeline_uniforms (void)
{
  TestState state;

  init_state (&state);

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cogl_framebuffer_get_width (test_fb),
                                 cogl_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint (&state);
  validate_result ();

  /* Try the test again after querying the location of a large
     number of uniforms. This should verify that the bitmasks
     still work even if they have to allocate a separate array to
     store the bits */

  init_long_pipeline_state (&state);
  paint (&state);
  paint_long_pipeline (&state);
  validate_result ();
  validate_long_pipeline_result ();

  destroy_state (&state);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
