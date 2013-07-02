#include <cogl/cogl.h>

#include "test-utils.h"

/* Keep track of the number of textures that we've created and are
 * still alive */
static int destroyed_texture_count = 0;

#define N_TEXTURES 3

static void
free_texture_cb (void *user_data)
{
  destroyed_texture_count++;
}

static CoglTexture *
create_texture (void)
{
  static const guint8 data[] =
    { 0xff, 0xff, 0xff, 0xff };
  static CoglUserDataKey texture_data_key;
  CoglTexture2D *tex_2d;

  tex_2d = cogl_texture_2d_new_from_data (test_ctx,
                                          1, 1, /* width / height */
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          4, /* rowstride */
                                          data,
                                          NULL);

  /* Set some user data on the texture so we can track when it has
   * been destroyed */
  cogl_object_set_user_data (COGL_OBJECT (tex_2d),
                             &texture_data_key,
                             GINT_TO_POINTER (1),
                             free_texture_cb);

  return tex_2d;
}

void
test_pipeline_cache_unrefs_texture (void)
{
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);
  CoglPipeline *simple_pipeline;
  int i;

  /* Create a pipeline with three texture layers. That way we can be
   * pretty sure the pipeline will cause a unique shader to be
   * generated in the cache */
  for (i = 0; i < N_TEXTURES; i++)
    {
      CoglTexture *tex = create_texture ();
      cogl_pipeline_set_layer_texture (pipeline, i, tex);
      cogl_object_unref (tex);
    }

  /* Draw something with the pipeline to ensure it gets into the
   * pipeline cache */
  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   0, 0, 10, 10);
  cogl_framebuffer_finish (test_fb);

  /* Draw something else so that it is no longer the current flushed
   * pipeline, and the units have a different texture bound */
  simple_pipeline = cogl_pipeline_new (test_ctx);
  for (i = 0; i < N_TEXTURES; i++)
    {
      CoglColor combine_constant;
      cogl_color_init_from_4ub (&combine_constant, i, 0, 0, 255);
      cogl_pipeline_set_layer_combine_constant (simple_pipeline,
                                                i,
                                                &combine_constant);
    }
  cogl_framebuffer_draw_rectangle (test_fb, simple_pipeline, 0, 0, 10, 10);
  cogl_framebuffer_finish (test_fb);
  cogl_object_unref (simple_pipeline);

  g_assert_cmpint (destroyed_texture_count, ==, 0);

  /* Destroy the pipeline. This should immediately cause the textures
   * to be freed */
  cogl_object_unref (pipeline);

  g_assert_cmpint (destroyed_texture_count, ==, N_TEXTURES);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
