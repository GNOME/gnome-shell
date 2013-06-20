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

CoglPipeline *
_cogl_pipeline_cache_get_fragment_template (CoglPipelineCache *cache,
                                            CoglPipeline *key_pipeline)
{
  return _cogl_pipeline_hash_table_get (&cache->fragment_hash,
                                        key_pipeline);
}

CoglPipeline *
_cogl_pipeline_cache_get_vertex_template (CoglPipelineCache *cache,
                                          CoglPipeline *key_pipeline)
{
  return _cogl_pipeline_hash_table_get (&cache->vertex_hash,
                                        key_pipeline);
}

CoglPipeline *
_cogl_pipeline_cache_get_combined_template (CoglPipelineCache *cache,
                                            CoglPipeline *key_pipeline)
{
  return _cogl_pipeline_hash_table_get (&cache->combined_hash,
                                        key_pipeline);
}
