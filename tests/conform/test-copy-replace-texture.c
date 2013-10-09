#include <cogl/cogl.h>
#include <string.h>

#include "test-utils.h"

/* Keep track of the number of textures that we've created and are
 * still alive */
static int alive_texture_mask = 0;

#define N_LAYERS 3
#define N_PIPELINES 4

#define PIPELINE_LAYER_MASK(pipeline_num)                       \
  (((1 << N_LAYERS) - 1) << (N_LAYERS * (pipeline_num) + 1))
#define LAST_PIPELINE_MASK PIPELINE_LAYER_MASK (N_PIPELINES - 1)
#define FIRST_PIPELINE_MASK PIPELINE_LAYER_MASK (0)

static void
free_texture_cb (void *user_data)
{
  int texture_num = GPOINTER_TO_INT (user_data);

  alive_texture_mask &= ~(1 << texture_num);
}

static CoglTexture *
create_texture (void)
{
  static const guint8 data[] =
    { 0xff, 0xff, 0xff, 0xff };
  static CoglUserDataKey texture_data_key;
  CoglTexture2D *tex_2d;
  static int texture_num = 1;

  alive_texture_mask |= (1 << texture_num);

  tex_2d = cogl_texture_2d_new_from_data (test_ctx,
                                          1, 1, /* width / height */
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          COGL_PIXEL_FORMAT_ANY,
                                          4, /* rowstride */
                                          data,
                                          NULL);

  /* Set some user data on the texture so we can track when it has
   * been destroyed */
  cogl_object_set_user_data (COGL_OBJECT (tex_2d),
                             &texture_data_key,
                             GINT_TO_POINTER (texture_num),
                             free_texture_cb);

  texture_num++;

  return tex_2d;
}

void
test_copy_replace_texture (void)
{
  CoglPipeline *pipelines[N_PIPELINES];
  int pipeline_num;

  /* Create a set of pipeline copies each with three of their own
   * replacement textures */
  for (pipeline_num = 0; pipeline_num < N_PIPELINES; pipeline_num++)
    {
      int layer_num;

      if (pipeline_num == 0)
        pipelines[pipeline_num] = cogl_pipeline_new (test_ctx);
      else
        pipelines[pipeline_num] =
          cogl_pipeline_copy (pipelines[pipeline_num - 1]);

      for (layer_num = 0; layer_num < N_LAYERS; layer_num++)
        {
          CoglTexture *tex = create_texture ();
          cogl_pipeline_set_layer_texture (pipelines[pipeline_num],
                                           layer_num,
                                           tex);
          cogl_object_unref (tex);
        }
    }

  /* Unref everything but the last pipeline */
  for (pipeline_num = 0; pipeline_num < N_PIPELINES - 1; pipeline_num++)
    cogl_object_unref (pipelines[pipeline_num]);

  if (alive_texture_mask && cogl_test_verbose ())
    {
      int i;

      g_print ("Alive textures:");

      for (i = 0; i < N_PIPELINES * N_LAYERS; i++)
        if ((alive_texture_mask & (1 << (i + 1))))
          g_print (" %i", i);

      g_print ("\n");
    }

  /* Ideally there should only be the textures from the last pipeline
   * left alive. We also let Cogl keep the textures from the first
   * texture alive because currently the child of the third layer in
   * the first pipeline will retain its authority on the unit index
   * state so that it can set it to 2. If there are more textures then
   * it means the pipeline isn't correctly pruning redundant
   * ancestors */
  g_assert_cmpint (alive_texture_mask & ~FIRST_PIPELINE_MASK,
                   ==,
                   LAST_PIPELINE_MASK);

  /* Clean up the last pipeline */
  cogl_object_unref (pipelines[N_PIPELINES - 1]);

  /* That should get rid of the last of the textures */
  g_assert_cmpint (alive_texture_mask, ==, 0);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
