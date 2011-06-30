/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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

#include "cogl-pipeline-private.h"
#include "cogl-pipeline-cache.h"
#include "cogl-context-private.h"

struct _CoglPipelineCache
{
  GHashTable *fragment_hash;
  GHashTable *vertex_hash;
};

static unsigned int
pipeline_fragment_hash (const void *data)
{
  unsigned int fragment_state;
  unsigned int layer_fragment_state;

  _COGL_GET_CONTEXT (ctx, 0);

  fragment_state =
    _cogl_pipeline_get_state_for_fragment_codegen (ctx);
  layer_fragment_state =
    _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx);

  return _cogl_pipeline_hash ((CoglPipeline *)data,
                              fragment_state, layer_fragment_state,
                              0);
}

static gboolean
pipeline_fragment_equal (const void *a, const void *b)
{
  unsigned int fragment_state;
  unsigned int layer_fragment_state;

  _COGL_GET_CONTEXT (ctx, 0);

  fragment_state =
    _cogl_pipeline_get_state_for_fragment_codegen (ctx);
  layer_fragment_state =
    _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx);

  return _cogl_pipeline_equal ((CoglPipeline *)a, (CoglPipeline *)b,
                               fragment_state, layer_fragment_state,
                               0);
}

static unsigned int
pipeline_vertex_hash (const void *data)
{
  unsigned long vertex_state =
    COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN;
  unsigned long layer_vertex_state =
    COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN;

  return _cogl_pipeline_hash ((CoglPipeline *)data,
                              vertex_state, layer_vertex_state,
                              0);
}

static gboolean
pipeline_vertex_equal (const void *a, const void *b)
{
  unsigned long vertex_state =
    COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN;
  unsigned long layer_vertex_state =
    COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN;

  return _cogl_pipeline_equal ((CoglPipeline *)a, (CoglPipeline *)b,
                               vertex_state, layer_vertex_state,
                               0);
}

CoglPipelineCache *
cogl_pipeline_cache_new (void)
{
  CoglPipelineCache *cache = g_new (CoglPipelineCache, 1);

  cache->fragment_hash = g_hash_table_new_full (pipeline_fragment_hash,
                                                pipeline_fragment_equal,
                                                cogl_object_unref,
                                                cogl_object_unref);
  cache->vertex_hash = g_hash_table_new_full (pipeline_vertex_hash,
                                              pipeline_vertex_equal,
                                              cogl_object_unref,
                                              cogl_object_unref);

  return cache;
}

void
cogl_pipeline_cache_free (CoglPipelineCache *cache)
{
  g_hash_table_destroy (cache->fragment_hash);
  g_hash_table_destroy (cache->vertex_hash);
  g_free (cache);
}

CoglPipeline *
_cogl_pipeline_cache_get_fragment_template (CoglPipelineCache *cache,
                                            CoglPipeline *key_pipeline)
{
  CoglPipeline *template =
    g_hash_table_lookup (cache->fragment_hash, key_pipeline);

  if (template == NULL)
    {
      /* XXX: I wish there was a way to insert into a GHashTable with
       * a pre-calculated hash value since there is a cost to
       * calculating the hash of a CoglPipeline and in this case we
       * know we have already called _cogl_pipeline_hash during the
       * lookup so we could pass the value through to here to avoid
       * hashing it again.
       */

      /* XXX: Any keys referenced by the hash table need to remain
       * valid all the while that there are corresponding values,
       * so for now we simply make a copy of the current authority
       * pipeline.
       *
       * FIXME: A problem with this is that our key into the cache may
       * hold references to some arbitrary user textures which will
       * now be kept alive indefinitly which is a shame. A better
       * solution will be to derive a special "key pipeline" from the
       * authority which derives from the base Cogl pipeline (to avoid
       * affecting the lifetime of any other pipelines) and only takes
       * a copy of the state that relates to the fragment shader and
       * references small dummy textures instead of potentially large
       * user textures. */
      template = cogl_pipeline_copy (key_pipeline);

      g_hash_table_insert (cache->fragment_hash,
                           template,
                           cogl_object_ref (template));

      if (G_UNLIKELY (g_hash_table_size (cache->fragment_hash) > 50))
        {
          static gboolean seen = FALSE;
          if (!seen)
            g_warning ("Over 50 separate fragment shaders have been "
                       "generated which is very unusual, so something "
                       "is probably wrong!\n");
          seen = TRUE;
        }
    }

  return template;
}

CoglPipeline *
_cogl_pipeline_cache_get_vertex_template (CoglPipelineCache *cache,
                                          CoglPipeline *key_pipeline)
{
  CoglPipeline *template =
    g_hash_table_lookup (cache->vertex_hash, key_pipeline);

  if (template == NULL)
    {
      template = cogl_pipeline_copy (key_pipeline);

      g_hash_table_insert (cache->vertex_hash,
                           template,
                           cogl_object_ref (template));

      if (G_UNLIKELY (g_hash_table_size (cache->vertex_hash) > 50))
        {
          static gboolean seen = FALSE;
          if (!seen)
            g_warning ("Over 50 separate vertex shaders have been "
                       "generated which is very unusual, so something "
                       "is probably wrong!\n");
          seen = TRUE;
        }
    }

  return template;
}

CoglPipeline *
_cogl_pipeline_cache_get_combined_template (CoglPipelineCache *cache,
                                            CoglPipeline *key_pipeline)
{
  unsigned int pipeline_state_for_fragment_codegen;
  unsigned int pipeline_layer_state_for_fragment_codegen;

  _COGL_GET_CONTEXT (ctx, NULL);

  pipeline_state_for_fragment_codegen =
    _cogl_pipeline_get_state_for_fragment_codegen (ctx);
  pipeline_layer_state_for_fragment_codegen =
    _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx);

  /* Currently the vertex shader state is a subset of the fragment
     shader state so we can avoid a third hash table here by just
     using the fragment shader table. This assert should catch it if
     that ever changes */

  g_assert ((pipeline_state_for_fragment_codegen |
             COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN) ==
            pipeline_state_for_fragment_codegen);
  g_assert ((pipeline_layer_state_for_fragment_codegen |
             COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN) ==
            pipeline_layer_state_for_fragment_codegen);

  return _cogl_pipeline_cache_get_fragment_template (cache, key_pipeline);
}
