/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011, 2013 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <test-fixtures/test-unit.h>

#include "cogl-context-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-cache.h"
#include "cogl-pipeline-hash-table.h"

struct _CoglPipelineCache
{
  CoglPipelineHashTable fragment_hash;
  CoglPipelineHashTable vertex_hash;
  CoglPipelineHashTable combined_hash;
};

CoglPipelineCache *
_cogl_pipeline_cache_new (void)
{
  CoglPipelineCache *cache = g_new (CoglPipelineCache, 1);
  unsigned long vertex_state;
  unsigned long layer_vertex_state;
  unsigned int fragment_state;
  unsigned int layer_fragment_state;

  _COGL_GET_CONTEXT (ctx, 0);

  vertex_state =
    _cogl_pipeline_get_state_for_vertex_codegen (ctx);
  layer_vertex_state =
    COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN;
  fragment_state =
    _cogl_pipeline_get_state_for_fragment_codegen (ctx);
  layer_fragment_state =
    _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx);

  _cogl_pipeline_hash_table_init (&cache->vertex_hash,
                                  vertex_state,
                                  layer_vertex_state,
                                  "vertex shaders");
  _cogl_pipeline_hash_table_init (&cache->fragment_hash,
                                  fragment_state,
                                  layer_fragment_state,
                                  "fragment shaders");
  _cogl_pipeline_hash_table_init (&cache->combined_hash,
                                  vertex_state | fragment_state,
                                  layer_vertex_state | layer_fragment_state,
                                  "programs");

  return cache;
}

void
_cogl_pipeline_cache_free (CoglPipelineCache *cache)
{
  _cogl_pipeline_hash_table_destroy (&cache->fragment_hash);
  _cogl_pipeline_hash_table_destroy (&cache->vertex_hash);
  _cogl_pipeline_hash_table_destroy (&cache->combined_hash);
  g_free (cache);
}

CoglPipelineCacheEntry *
_cogl_pipeline_cache_get_fragment_template (CoglPipelineCache *cache,
                                            CoglPipeline *key_pipeline)
{
  return _cogl_pipeline_hash_table_get (&cache->fragment_hash,
                                        key_pipeline);
}

CoglPipelineCacheEntry *
_cogl_pipeline_cache_get_vertex_template (CoglPipelineCache *cache,
                                          CoglPipeline *key_pipeline)
{
  return _cogl_pipeline_hash_table_get (&cache->vertex_hash,
                                        key_pipeline);
}

CoglPipelineCacheEntry *
_cogl_pipeline_cache_get_combined_template (CoglPipelineCache *cache,
                                            CoglPipeline *key_pipeline)
{
  return _cogl_pipeline_hash_table_get (&cache->combined_hash,
                                        key_pipeline);
}

#ifdef ENABLE_UNIT_TESTS

static void
create_pipelines (CoglPipeline **pipelines,
                  int n_pipelines)
{
  int i;

  for (i = 0; i < n_pipelines; i++)
    {
      char *source = g_strdup_printf ("  cogl_color_out = "
                                      "vec4 (%f, 0.0, 0.0, 1.0);\n",
                                      i / 255.0f);
      CoglSnippet *snippet =
        cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                          NULL, /* declarations */
                          source);

      g_free (source);

      pipelines[i] = cogl_pipeline_new (test_ctx);
      cogl_pipeline_add_snippet (pipelines[i], snippet);
      cogl_object_unref (snippet);
    }

  /* Test that drawing with them works. This should create the entries
   * in the cache */
  for (i = 0; i < n_pipelines; i++)
    {
      cogl_framebuffer_draw_rectangle (test_fb,
                                       pipelines[i],
                                       i, 0,
                                       i + 1, 1);
      test_utils_check_pixel_rgb (test_fb, i, 0, i, 0, 0);
    }

}

UNIT_TEST (check_pipeline_pruning,
           TEST_REQUIREMENT_GLSL, /* requirements */
           0 /* no failure cases */)
{
  CoglPipeline *pipelines[18];
  int fb_width, fb_height;
  CoglPipelineHashTable *fragment_hash =
    &test_ctx->pipeline_cache->fragment_hash;
  CoglPipelineHashTable *combined_hash =
    &test_ctx->pipeline_cache->combined_hash;
  int i;

  fb_width = cogl_framebuffer_get_width (test_fb);
  fb_height = cogl_framebuffer_get_height (test_fb);

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 fb_width,
                                 fb_height,
                                 -1,
                                 100);

  /* Create 18 unique pipelines. This should end up being more than
   * the initial expected minimum size so it will trigger the garbage
   * collection. However all of the pipelines will be in use so they
   * won't be collected */
  create_pipelines (pipelines, 18);

  /* These pipelines should all have unique entries in the cache. We
   * should have run the garbage collection once and at that point the
   * expected minimum size would have been 17 */
  g_assert_cmpint (g_hash_table_size (fragment_hash->table), ==, 18);
  g_assert_cmpint (g_hash_table_size (combined_hash->table), ==, 18);
  g_assert_cmpint (fragment_hash->expected_min_size, ==, 17);
  g_assert_cmpint (combined_hash->expected_min_size, ==, 17);

  /* Destroy the original pipelines and create some new ones. This
   * should run the garbage collector again but this time the
   * pipelines won't be in use so it should free some of them */
  for (i = 0; i < 18; i++)
    cogl_object_unref (pipelines[i]);

  create_pipelines (pipelines, 18);

  /* The garbage collection should have freed half of the original 18
   * pipelines which means there should now be 18*1.5 = 27 */
  g_assert_cmpint (g_hash_table_size (fragment_hash->table), ==, 27);
  g_assert_cmpint (g_hash_table_size (combined_hash->table), ==, 27);
  /* The 35th pipeline would have caused the garbage collection. At
   * that point there would be 35-18=17 used unique pipelines. */
  g_assert_cmpint (fragment_hash->expected_min_size, ==, 17);
  g_assert_cmpint (combined_hash->expected_min_size, ==, 17);

  for (i = 0; i < 18; i++)
    cogl_object_unref (pipelines[i]);
}

#endif /* ENABLE_UNIT_TESTS */
