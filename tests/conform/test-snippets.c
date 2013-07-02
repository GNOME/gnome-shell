#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  int fb_width, fb_height;
} TestState;

typedef void (* SnippetTestFunc) (TestState *state);

static CoglPipeline *
create_texture_pipeline (TestState *state)
{
  CoglPipeline *pipeline;
  CoglTexture *tex;
  static const uint8_t tex_data[] =
    {
      0xff, 0x00, 0x00, 0xff, /* red */  0x00, 0xff, 0x00, 0xff, /* green */
      0x00, 0x00, 0xff, 0xff, /* blue */ 0xff, 0xff, 0x00, 0xff, /* yellow */
    };

  tex = test_utils_texture_new_from_data (test_ctx,
                                          2, 2, /* width/height */
                                          TEST_UTILS_TEXTURE_NO_ATLAS,
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          8, /* rowstride */
                                          tex_data);

  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_layer_texture (pipeline, 0, tex);

  cogl_pipeline_set_layer_filters (pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  cogl_object_unref (tex);

  return pipeline;
}

static void
simple_fragment_snippet (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Simple fragment snippet */
  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_color4ub (pipeline, 255, 0, 0, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              "cogl_color_out.g += 1.0;");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 0, 0, 10, 10);

  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 5, 5, 0xffff00ff);
}

static void
simple_vertex_snippet (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Simple vertex snippet */
  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_color4ub (pipeline, 255, 0, 0, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                              NULL,
                              "cogl_color_out.b += 1.0;");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 10, 0, 20, 10);

  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 15, 5, 0xff00ffff);
}

static void
shared_uniform (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  int location;

  /* Snippets sharing a uniform across the vertex and fragment
     hooks */
  pipeline = cogl_pipeline_new (test_ctx);

  location = cogl_pipeline_get_uniform_location (pipeline, "a_value");
  cogl_pipeline_set_uniform_1f (pipeline, location, 0.25f);

  cogl_pipeline_set_color4ub (pipeline, 255, 0, 0, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                              "uniform float a_value;",
                              "cogl_color_out.b += a_value;");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              "uniform float a_value;",
                              "cogl_color_out.b += a_value;");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   20, 0, 30, 10);

  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 25, 5, 0xff0080ff);
}

static void
lots_snippets (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  int location;
  int i;

  /* Lots of snippets on one pipeline */
  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_color4ub (pipeline, 0, 0, 0, 255);

  for (i = 0; i < 3; i++)
    {
      char letter = 'x' + i;
      char *uniform_name = g_strdup_printf ("%c_value", letter);
      char *declarations = g_strdup_printf ("uniform float %s;\n",
                                            uniform_name);
      char *code = g_strdup_printf ("cogl_color_out.%c = %s;\n",
                                    letter,
                                    uniform_name);

      location = cogl_pipeline_get_uniform_location (pipeline, uniform_name);
      cogl_pipeline_set_uniform_1f (pipeline, location, (i + 1) * 0.1f);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  declarations,
                                  code);
      cogl_pipeline_add_snippet (pipeline, snippet);
      cogl_object_unref (snippet);

      g_free (code);
      g_free (uniform_name);
      g_free (declarations);
    }

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 30, 0, 40, 10);

  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 35, 5, 0x19334cff);
}

static void
shared_variable_pre_post (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Test that the pre string can declare variables used by the post
     string */
  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_color4ub (pipeline, 255, 255, 255, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              "cogl_color_out = redvec;");
  cogl_snippet_set_pre (snippet, "vec4 redvec = vec4 (1.0, 0.0, 0.0, 1.0);");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 40, 0, 50, 10);

  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 45, 5, 0xff0000ff);
}

static void
test_pipeline_caching (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Check that the pipeline caching works when unrelated pipelines
     share snippets state. It's too hard to actually assert this in
     the conformance test but at least it should be possible to see by
     setting COGL_DEBUG=show-source to check whether this shader gets
     generated twice */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              "/* This comment should only be seen ONCE\n"
                              "   when COGL_DEBUG=show-source is TRUE\n"
                              "   even though it is used in two different\n"
                              "   unrelated pipelines */",
                              "cogl_color_out = vec4 (0.0, 1.0, 0.0, 1.0);\n");

  pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 50, 0, 60, 10);
  cogl_object_unref (pipeline);

  pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 60, 0, 70, 10);
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

  test_utils_check_pixel (test_fb, 55, 5, 0x00ff00ff);
  test_utils_check_pixel (test_fb, 65, 5, 0x00ff00ff);
}

static void
test_replace_string (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Check the replace string */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT, NULL, NULL);
  cogl_snippet_set_pre (snippet,
                        "cogl_color_out = vec4 (0.0, 0.5, 0.0, 1.0);");
  /* Remove the generated output. If the replace string isn't working
     then the code from the pre string would get overwritten with
     white */
  cogl_snippet_set_replace (snippet, "/* do nothing */");
  cogl_snippet_set_post (snippet,
                         "cogl_color_out += vec4 (0.5, 0.0, 0.0, 1.0);");

  pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 70, 0, 80, 10);
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

  test_utils_check_pixel (test_fb, 75, 5, 0x808000ff);
}

static void
test_texture_lookup_hook (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Check the texture lookup hook */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              NULL,
                              "cogl_texel.b += 1.0;");
  /* Flip the texture coordinates around the y axis so that it will
     get the green texel */
  cogl_snippet_set_pre (snippet, "cogl_tex_coord.x = 1.0 - cogl_tex_coord.x;");

  pipeline = create_texture_pipeline (state);
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_framebuffer_draw_textured_rectangle (test_fb,
                                            pipeline,
                                            80, 0, 90, 10,
                                            0, 0, 0, 0);
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

  test_utils_check_pixel (test_fb, 85, 5, 0x00ffffff);
}

static void
test_multiple_samples (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Check that we can use the passed in sampler in the texture lookup
     to sample multiple times */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              NULL,
                              NULL);
  cogl_snippet_set_replace (snippet,
                            "cogl_texel = "
                            "texture2D (cogl_sampler, vec2 (0.25, 0.25)) + "
                            "texture2D (cogl_sampler, vec2 (0.75, 0.25));");

  pipeline = create_texture_pipeline (state);
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 0, 0, 10, 10);
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

  test_utils_check_pixel (test_fb, 5, 5, 0xffff00ff);
}

static void
test_replace_lookup_hook (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Check replacing the texture lookup hook */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP, NULL, NULL);
  cogl_snippet_set_replace (snippet, "cogl_texel = vec4 (0.0, 0.0, 1.0, 0.0);");

  pipeline = create_texture_pipeline (state);
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_framebuffer_draw_textured_rectangle (test_fb,
                                            pipeline,
                                            90, 0, 100, 10,
                                            0, 0, 0, 0);
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

  test_utils_check_pixel (test_fb, 95, 5, 0x0000ffff);
}

static void
test_replace_snippet (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Test replacing a previous snippet */
  pipeline = create_texture_pipeline (state);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL,
                              "cogl_color_out = vec4 (0.5, 0.5, 0.5, 1.0);");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT, NULL, NULL);
  cogl_snippet_set_pre (snippet, "cogl_color_out = vec4 (1.0, 1.0, 1.0, 1.0);");
  cogl_snippet_set_replace (snippet,
                            "cogl_color_out *= vec4 (1.0, 0.0, 0.0, 1.0);");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_textured_rectangle (test_fb,
                                            pipeline,
                                            100, 0, 110, 10,
                                            0, 0, 0, 0);
  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 105, 5, 0xff0000ff);
}

static void
test_replace_fragment_layer (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Test replacing the fragment layer code */
  pipeline = create_texture_pipeline (state);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
  cogl_snippet_set_replace (snippet, "cogl_layer = vec4 (0.0, 0.0, 1.0, 1.0);");
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_object_unref (snippet);

  /* Add a second layer which samples from the texture in the first
     layer. The snippet override should cause the first layer not to
     generate the code for the texture lookup but this second layer
     should still be able to cause it to be generated */
  cogl_pipeline_set_layer_combine (pipeline, 1,
                                   "RGB = ADD(TEXTURE_0, PREVIOUS)"
                                   "A = REPLACE(PREVIOUS)",
                                   NULL);

  cogl_framebuffer_draw_textured_rectangle (test_fb,
                                            pipeline,
                                            110, 0, 120, 10,
                                            0, 0, 0, 0);
  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 115, 5, 0xff00ffff);
}

static void
test_modify_fragment_layer (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Test modifying the fragment layer code */
  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_uniform_1f (pipeline,
                                cogl_pipeline_get_uniform_location (pipeline,
                                                                    "a_value"),
                                0.5);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_LAYER_FRAGMENT,
                              "uniform float a_value;",
                              "cogl_layer.g = a_value;");
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_textured_rectangle (test_fb,
                                            pipeline,
                                            120, 0, 130, 10,
                                            0, 0, 0, 0);
  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 125, 5, 0xff80ffff);
}

static void
test_modify_vertex_layer (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  CoglMatrix matrix;

  /* Test modifying the vertex layer code */
  pipeline = create_texture_pipeline (state);

  cogl_matrix_init_identity (&matrix);
  cogl_matrix_translate (&matrix, 0.0f, 1.0f, 0.0f);
  cogl_pipeline_set_layer_matrix (pipeline, 0, &matrix);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM,
                              NULL,
                              "cogl_tex_coord.x = 1.0;");
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_textured_rectangle (test_fb,
                                            pipeline,
                                            130, 0, 140, 10,
                                            0, 0, 0, 0);
  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 135, 5, 0xffff00ff);
}

static void
test_replace_vertex_layer (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  CoglMatrix matrix;

  /* Test replacing the vertex layer code */
  pipeline = create_texture_pipeline (state);

  cogl_matrix_init_identity (&matrix);
  cogl_matrix_translate (&matrix, 0.0f, 1.0f, 0.0f);
  cogl_pipeline_set_layer_matrix (pipeline, 0, &matrix);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM,
                              NULL,
                              NULL);
  cogl_snippet_set_replace (snippet, "cogl_tex_coord.x = 1.0;\n");
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_textured_rectangle (test_fb,
                                            pipeline,
                                            140, 0, 150, 10,
                                            0, 0, 0, 0);
  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 145, 5, 0x00ff00ff);
}

static void
test_vertex_transform_hook (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  CoglMatrix identity_matrix;
  CoglMatrix matrix;
  int location;

  /* Test the vertex transform hook */

  cogl_matrix_init_identity (&identity_matrix);

  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_color4ub (pipeline, 255, 0, 255, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX_TRANSFORM,
                              "uniform mat4 pmat;",
                              NULL);
  cogl_snippet_set_replace (snippet, "cogl_position_out = "
                            "pmat * cogl_position_in;");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  /* Copy the current projection matrix to a uniform */
  cogl_framebuffer_get_projection_matrix (test_fb, &matrix);
  location = cogl_pipeline_get_uniform_location (pipeline, "pmat");
  cogl_pipeline_set_uniform_matrix (pipeline,
                                    location,
                                    4, /* dimensions */
                                    1, /* count */
                                    FALSE, /* don't transpose */
                                    cogl_matrix_get_array (&matrix));

  /* Replace the real projection matrix with the identity. This should
     mess up the drawing unless the snippet replacement is working */
  cogl_framebuffer_set_projection_matrix (test_fb, &identity_matrix);

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 150, 0, 160, 10);
  cogl_object_unref (pipeline);

  /* Restore the projection matrix */
  cogl_framebuffer_set_projection_matrix (test_fb, &matrix);

  test_utils_check_pixel (test_fb, 155, 5, 0xff00ffff);
}

static void
test_global_vertex_hook (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  pipeline = cogl_pipeline_new (test_ctx);

  /* Creates a function in the global declarations hook which is used
   * by a subsequent snippet. The subsequent snippets replace any
   * previous snippets but this shouldn't prevent the global
   * declarations from being generated */

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX_GLOBALS,
                              /* declarations */
                              "float\n"
                              "multiply_by_two (float number)\n"
                              "{\n"
                              "  return number * 2.0;\n"
                              "}\n",
                              /* post */
                              "This string shouldn't be used so "
                              "we can safely put garbage in here.");
  cogl_snippet_set_pre (snippet,
                        "This string shouldn't be used so "
                        "we can safely put garbage in here.");
  cogl_snippet_set_replace (snippet,
                            "This string shouldn't be used so "
                            "we can safely put garbage in here.");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                              NULL, /* declarations */
                              NULL /* replace */);
  cogl_snippet_set_replace (snippet,
                            "cogl_color_out.r = multiply_by_two (0.5);\n"
                            "cogl_color_out.gba = vec3 (0.0, 0.0, 1.0);\n"
                            "cogl_position_out = cogl_position_in;\n");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   -1, 1,
                                   10.0f * 2.0f / state->fb_width - 1.0f,
                                   10.0f * 2.0f / state->fb_height - 1.0f);

  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 5, 5, 0xff0000ff);
}

static void
test_global_fragment_hook (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  pipeline = cogl_pipeline_new (test_ctx);

  /* Creates a function in the global declarations hook which is used
   * by a subsequent snippet. The subsequent snippets replace any
   * previous snippets but this shouldn't prevent the global
   * declarations from being generated */

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT_GLOBALS,
                              /* declarations */
                              "float\n"
                              "multiply_by_four (float number)\n"
                              "{\n"
                              "  return number * 4.0;\n"
                              "}\n",
                              /* post */
                              "This string shouldn't be used so "
                              "we can safely put garbage in here.");
  cogl_snippet_set_pre (snippet,
                        "This string shouldn't be used so "
                        "we can safely put garbage in here.");
  cogl_snippet_set_replace (snippet,
                            "This string shouldn't be used so "
                            "we can safely put garbage in here.");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              NULL /* replace */);
  cogl_snippet_set_replace (snippet,
                            "cogl_color_out.r = multiply_by_four (0.25);\n"
                            "cogl_color_out.gba = vec3 (0.0, 0.0, 1.0);\n");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   0, 0, 10, 10);

  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 5, 5, 0xff0000ff);
}

static void
test_snippet_order (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  /* Verify that the snippets are executed in the right order. We'll
     replace the r component of the color in the pre sections of the
     snippets and the g component in the post. The pre sections should
     be executed in the reverse order they were added and the post
     sections in the same order as they were added. Therefore the r
     component should be taken from the the second snippet and the g
     component from the first */
  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_color4ub (pipeline, 0, 0, 0, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL,
                              "cogl_color_out.g = 0.5;\n");
  cogl_snippet_set_pre (snippet, "cogl_color_out.r = 0.5;\n");
  cogl_snippet_set_replace (snippet, "cogl_color_out.ba = vec2 (0.0, 1.0);");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL,
                              "cogl_color_out.g = 1.0;\n");
  cogl_snippet_set_pre (snippet, "cogl_color_out.r = 1.0;\n");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 160, 0, 170, 10);
  cogl_object_unref (pipeline);

  test_utils_check_pixel (test_fb, 165, 5, 0x80ff00ff);
}

static void
test_naming_texture_units (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  CoglTexture *tex1, *tex2;

  /* Test that we can sample from an arbitrary texture unit by naming
     its layer number */

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL,
                              NULL);
  cogl_snippet_set_replace (snippet,
                            "cogl_color_out = "
                            "texture2D (cogl_sampler100, vec2 (0.0, 0.0)) + "
                            "texture2D (cogl_sampler200, vec2 (0.0, 0.0));");

  tex1 = test_utils_create_color_texture (test_ctx, 0xff0000ff);
  tex2 = test_utils_create_color_texture (test_ctx, 0x00ff00ff);

  pipeline = cogl_pipeline_new (test_ctx);

  cogl_pipeline_set_layer_texture (pipeline, 100, tex1);
  cogl_pipeline_set_layer_texture (pipeline, 200, tex2);

  cogl_pipeline_add_snippet (pipeline, snippet);

  cogl_framebuffer_draw_rectangle (test_fb, pipeline, 0, 0, 10, 10);

  cogl_object_unref (pipeline);
  cogl_object_unref (snippet);
  cogl_object_unref (tex1);
  cogl_object_unref (tex2);

  test_utils_check_pixel (test_fb, 5, 5, 0xffff00ff);
}

static void
test_snippet_properties (TestState *state)
{
  CoglSnippet *snippet;

  /* Sanity check modifying the snippet */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT, "foo", "bar");
  g_assert_cmpstr (cogl_snippet_get_declarations (snippet), ==, "foo");
  g_assert_cmpstr (cogl_snippet_get_post (snippet), ==, "bar");
  g_assert_cmpstr (cogl_snippet_get_replace (snippet), ==, NULL);
  g_assert_cmpstr (cogl_snippet_get_pre (snippet), ==, NULL);

  cogl_snippet_set_declarations (snippet, "fu");
  g_assert_cmpstr (cogl_snippet_get_declarations (snippet), ==, "fu");
  g_assert_cmpstr (cogl_snippet_get_post (snippet), ==, "bar");
  g_assert_cmpstr (cogl_snippet_get_replace (snippet), ==, NULL);
  g_assert_cmpstr (cogl_snippet_get_pre (snippet), ==, NULL);

  cogl_snippet_set_post (snippet, "ba");
  g_assert_cmpstr (cogl_snippet_get_declarations (snippet), ==, "fu");
  g_assert_cmpstr (cogl_snippet_get_post (snippet), ==, "ba");
  g_assert_cmpstr (cogl_snippet_get_replace (snippet), ==, NULL);
  g_assert_cmpstr (cogl_snippet_get_pre (snippet), ==, NULL);

  cogl_snippet_set_pre (snippet, "fuba");
  g_assert_cmpstr (cogl_snippet_get_declarations (snippet), ==, "fu");
  g_assert_cmpstr (cogl_snippet_get_post (snippet), ==, "ba");
  g_assert_cmpstr (cogl_snippet_get_replace (snippet), ==, NULL);
  g_assert_cmpstr (cogl_snippet_get_pre (snippet), ==, "fuba");

  cogl_snippet_set_replace (snippet, "baba");
  g_assert_cmpstr (cogl_snippet_get_declarations (snippet), ==, "fu");
  g_assert_cmpstr (cogl_snippet_get_post (snippet), ==, "ba");
  g_assert_cmpstr (cogl_snippet_get_replace (snippet), ==, "baba");
  g_assert_cmpstr (cogl_snippet_get_pre (snippet), ==, "fuba");

  g_assert_cmpint (cogl_snippet_get_hook (snippet),
                   ==,
                   COGL_SNIPPET_HOOK_FRAGMENT);
}

static SnippetTestFunc
tests[] =
  {
    simple_fragment_snippet,
    simple_vertex_snippet,
    shared_uniform,
    lots_snippets,
    shared_variable_pre_post,
    test_pipeline_caching,
    test_replace_string,
    test_texture_lookup_hook,
    test_multiple_samples,
    test_replace_lookup_hook,
    test_replace_snippet,
    test_replace_fragment_layer,
    test_modify_fragment_layer,
    test_modify_vertex_layer,
    test_replace_vertex_layer,
    test_vertex_transform_hook,
    test_global_fragment_hook,
    test_global_vertex_hook,
    test_snippet_order,
    test_naming_texture_units,
    test_snippet_properties
  };

static void
run_tests (TestState *state)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      cogl_framebuffer_clear4f (test_fb,
                                COGL_BUFFER_BIT_COLOR,
                                0, 0, 0, 1);

      tests[i] (state);
    }
}

void
test_snippets (void)
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

  run_tests (&state);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
