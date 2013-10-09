#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

void
test_pipeline_shader_state (void)
{
  CoglOffscreen *offscreen;
  CoglFramebuffer *fb;
  CoglPipeline *base_pipeline;
  CoglPipeline *draw_pipeline;
  CoglTexture2D *tex;
  CoglSnippet *snippet;

  float width = cogl_framebuffer_get_width (test_fb);
  float height = cogl_framebuffer_get_height (test_fb);

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0, width, height,
                                 -1,
                                 100);

  tex = cogl_texture_2d_new_with_size (test_ctx,
                                       128, 128, COGL_PIXEL_FORMAT_ANY);
  offscreen = cogl_offscreen_new_with_texture (tex);
  fb = offscreen;
  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);
  cogl_object_unref (offscreen);

  cogl_framebuffer_clear4f (test_fb, COGL_BUFFER_BIT_COLOR, 1, 1, 0, 1);


  /* Setup a template pipeline... */

  base_pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_layer_texture (base_pipeline, 1, tex);
  cogl_pipeline_set_color4f (base_pipeline, 1, 0, 0, 1);


  /* Derive a pipeline from the template, making a change that affects
   * fragment processing but making sure not to affect vertex
   * processing... */

  draw_pipeline = cogl_pipeline_copy (base_pipeline);
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              "cogl_color_out = vec4 (0.0, 1.0, 0.1, 1.1);");
  cogl_pipeline_add_snippet (draw_pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb, draw_pipeline,
                                   0, 0, width, height);

  cogl_object_unref (draw_pipeline);

  cogl_framebuffer_finish (test_fb);


  /* At this point we should have provoked cogl to cache some vertex
   * shader state for the draw_pipeline with the base_pipeline because
   * none of the changes made to the draw_pipeline affected vertex
   * processing. (NB: cogl will cache shader state with the oldest
   * ancestor that the state is still valid for to maximize the chance
   * that it can be used with other derived pipelines)
   *
   * Now we make a change to the base_pipeline to make sure that this
   * cached vertex shader gets invalidated.
   */

  cogl_pipeline_set_layer_texture (base_pipeline, 0, tex);


  /* Now we derive another pipeline from base_pipeline to verify that
   * it doesn't end up re-using the old cached state
   */

  draw_pipeline = cogl_pipeline_copy (base_pipeline);
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* declarations */
                              "cogl_color_out = vec4 (0.0, 0.0, 1.1, 1.1);");
  cogl_pipeline_add_snippet (draw_pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_framebuffer_draw_rectangle (test_fb, draw_pipeline,
                                   0, 0, width, height);

  cogl_object_unref (draw_pipeline);


  test_utils_check_region (test_fb, 0, 0, width, height,
                           0x0000ffff);
}
