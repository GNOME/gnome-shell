#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  int stub;
} TestState;

static CoglPipeline *
create_texture_pipeline (void)
{
  CoglPipeline *pipeline;
  CoglHandle tex;
  static const guint8 tex_data[] =
    {
      0xff, 0x00, 0x00, 0xff, /* red */  0x00, 0xff, 0x00, 0xff, /* green */
      0x00, 0x00, 0xff, 0xff, /* blue */ 0xff, 0xff, 0x00, 0xff, /* yellow */
    };

  tex = cogl_texture_new_from_data (2, 2, /* width/height */
                                    COGL_TEXTURE_NO_ATLAS,
                                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                    COGL_PIXEL_FORMAT_ANY,
                                    8, /* rowstride */
                                    tex_data);

  pipeline = cogl_pipeline_new ();

  cogl_pipeline_set_layer_texture (pipeline, 0, tex);

  cogl_pipeline_set_layer_filters (pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  cogl_handle_unref (tex);

  return pipeline;
}

static void
paint (TestState *state)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  CoglColor color;
  int location;
  int i;

  cogl_color_init_from_4ub (&color, 0, 0, 0, 255);
  cogl_clear (&color, COGL_BUFFER_BIT_COLOR);

  /* Simple fragment snippet */
  pipeline = cogl_pipeline_new ();

  cogl_pipeline_set_color4ub (pipeline, 255, 0, 0, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              "cogl_color_out.g += 1.0;");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_push_source (pipeline);
  cogl_rectangle (0, 0, 10, 10);
  cogl_pop_source ();

  cogl_object_unref (pipeline);

  /* Simple vertex snippet */
  pipeline = cogl_pipeline_new ();

  cogl_pipeline_set_color4ub (pipeline, 255, 0, 0, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                              NULL,
                              "cogl_color_out.b += 1.0;");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_push_source (pipeline);
  cogl_rectangle (10, 0, 20, 10);
  cogl_pop_source ();

  cogl_object_unref (pipeline);

  /* Snippets sharing a uniform across the vertex and fragment
     hooks */
  pipeline = cogl_pipeline_new ();

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

  cogl_push_source (pipeline);
  cogl_rectangle (20, 0, 30, 10);
  cogl_pop_source ();

  cogl_object_unref (pipeline);

  /* Lots of snippets on one pipeline */
  pipeline = cogl_pipeline_new ();

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

  cogl_push_source (pipeline);
  cogl_rectangle (30, 0, 40, 10);
  cogl_pop_source ();

  cogl_object_unref (pipeline);

  /* Test that the pre string can declare variables used by the post
     string */
  pipeline = cogl_pipeline_new ();

  cogl_pipeline_set_color4ub (pipeline, 255, 255, 255, 255);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              "cogl_color_out = redvec;");
  cogl_snippet_set_pre (snippet, "vec4 redvec = vec4 (1.0, 0.0, 0.0, 1.0);");
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_push_source (pipeline);
  cogl_rectangle (40, 0, 50, 10);
  cogl_pop_source ();

  cogl_object_unref (pipeline);

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

  pipeline = cogl_pipeline_new ();
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_push_source (pipeline);
  cogl_rectangle (50, 0, 60, 10);
  cogl_pop_source ();
  cogl_object_unref (pipeline);

  pipeline = cogl_pipeline_new ();
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_push_source (pipeline);
  cogl_rectangle (60, 0, 70, 10);
  cogl_pop_source ();
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

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

  pipeline = cogl_pipeline_new ();
  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_push_source (pipeline);
  cogl_rectangle (70, 0, 80, 10);
  cogl_pop_source ();
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

  /* Check the texture lookup hook */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              NULL,
                              "cogl_texel.b += 1.0;");
  /* Flip the texture coordinates around the y axis so that it will
     get the green texel */
  cogl_snippet_set_pre (snippet, "cogl_tex_coord.x = 1.0 - cogl_tex_coord.x;");

  pipeline = create_texture_pipeline ();
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_push_source (pipeline);
  cogl_rectangle_with_texture_coords (80, 0, 90, 10,
                                      0, 0, 0, 0);
  cogl_pop_source ();
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

  /* Check replacing the texture lookup hook */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP, NULL, NULL);
  cogl_snippet_set_replace (snippet, "cogl_texel = vec4 (0.0, 0.0, 1.0, 0.0);");

  pipeline = create_texture_pipeline ();
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_push_source (pipeline);
  cogl_rectangle_with_texture_coords (90, 0, 100, 10,
                                      0, 0, 0, 0);
  cogl_pop_source ();
  cogl_object_unref (pipeline);

  cogl_object_unref (snippet);

  /* Test replacing a previous snippet */
  pipeline = create_texture_pipeline ();

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

  cogl_push_source (pipeline);
  cogl_rectangle_with_texture_coords (100, 0, 110, 10,
                                      0, 0, 0, 0);
  cogl_pop_source ();
  cogl_object_unref (pipeline);

  /* Test replacing the layer code */
  pipeline = create_texture_pipeline ();

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

  cogl_push_source (pipeline);
  cogl_rectangle_with_texture_coords (110, 0, 120, 10,
                                      0, 0, 0, 0);
  cogl_pop_source ();
  cogl_object_unref (pipeline);

  /* Test modifying the layer code */
  pipeline = cogl_pipeline_new ();

  cogl_pipeline_set_uniform_1f (pipeline,
                                cogl_pipeline_get_uniform_location (pipeline,
                                                                    "a_value"),
                                0.5);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_LAYER_FRAGMENT,
                              "uniform float a_value;",
                              "cogl_layer.g = a_value;");
  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);
  cogl_object_unref (snippet);

  cogl_push_source (pipeline);
  cogl_rectangle_with_texture_coords (120, 0, 130, 10,
                                      0, 0, 0, 0);
  cogl_pop_source ();
  cogl_object_unref (pipeline);

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

static void
validate_result (void)
{
  test_utils_check_pixel (5, 5, 0xffff00ff);
  test_utils_check_pixel (15, 5, 0xff00ffff);
  test_utils_check_pixel (25, 5, 0xff0080ff);
  test_utils_check_pixel (35, 5, 0x19334cff);
  test_utils_check_pixel (45, 5, 0xff0000ff);
  test_utils_check_pixel (55, 5, 0x00ff00ff);
  test_utils_check_pixel (65, 5, 0x00ff00ff);
  test_utils_check_pixel (75, 5, 0x808000ff);
  test_utils_check_pixel (85, 5, 0x00ffffff);
  test_utils_check_pixel (95, 5, 0x0000ffff);
  test_utils_check_pixel (105, 5, 0xff0000ff);
  test_utils_check_pixel (115, 5, 0xff00ffff);
  test_utils_check_pixel (125, 5, 0xff80ffff);
}

void
test_cogl_snippets (TestUtilsGTestFixture *fixture,
                    void *user_data)
{
  TestUtilsSharedState *shared_state = user_data;

  /* If shaders aren't supported then we can't run the test */
  if (cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    {
      TestState state;

      cogl_ortho (/* left, right */
                  0, cogl_framebuffer_get_width (shared_state->fb),
                  /* bottom, top */
                  cogl_framebuffer_get_height (shared_state->fb), 0,
                  /* z near, far */
                  -1, 100);

      paint (&state);
      validate_result ();

      if (g_test_verbose ())
        g_print ("OK\n");
    }
  else if (g_test_verbose ())
    g_print ("Skipping\n");
}
