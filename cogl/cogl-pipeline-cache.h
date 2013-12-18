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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_PIPELINE_CACHE_H__
#define __COGL_PIPELINE_CACHE_H__

#include "cogl-pipeline.h"

typedef struct _CoglPipelineCache CoglPipelineCache;

typedef struct
{
  CoglPipeline *pipeline;

  /* Number of usages of this template. If this drops to zero then it
   * will be a candidate for removal from the cache */
  int usage_count;
} CoglPipelineCacheEntry;

CoglPipelineCache *
_cogl_pipeline_cache_new (void);

void
_cogl_pipeline_cache_free (CoglPipelineCache *cache);

/*
 * Gets a pipeline from the cache that has the same state as
 * @key_pipeline for the state in
 * COGL_PIPELINE_STATE_AFFECTS_FRAGMENT_CODEGEN. If there is no
 * matching pipline already then a copy of key_pipeline is stored in
 * the cache so that it will be used next time the function is called
 * with a similar pipeline. In that case the copy itself will be
 * returned
 */
CoglPipelineCacheEntry *
_cogl_pipeline_cache_get_fragment_template (CoglPipelineCache *cache,
                                            CoglPipeline *key_pipeline);

/*
 * Gets a pipeline from the cache that has the same state as
 * @key_pipeline for the state in
 * COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN. If there is no
 * matching pipline already then a copy of key_pipeline is stored in
 * the cache so that it will be used next time the function is called
 * with a similar pipeline. In that case the copy itself will be
 * returned
 */
CoglPipelineCacheEntry *
_cogl_pipeline_cache_get_vertex_template (CoglPipelineCache *cache,
                                          CoglPipeline *key_pipeline);

/*
 * Gets a pipeline from the cache that has the same state as
 * @key_pipeline for the combination of the state state in
 * COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN and
 * COGL_PIPELINE_STATE_AFFECTS_FRAGMENT_CODEGEN. If there is no
 * matching pipline already then a copy of key_pipeline is stored in
 * the cache so that it will be used next time the function is called
 * with a similar pipeline. In that case the copy itself will be
 * returned
 */
CoglPipelineCacheEntry *
_cogl_pipeline_cache_get_combined_template (CoglPipelineCache *cache,
                                            CoglPipeline *key_pipeline);

#endif /* __COGL_PIPELINE_CACHE_H__ */
