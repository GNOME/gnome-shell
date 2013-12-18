/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2013 Intel Corporation.
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

#ifndef __COGL_PIPELINE_HASH_H__
#define __COGL_PIPELINE_HASH_H__

#include "cogl-pipeline-cache.h"

typedef struct
{
  /* Total number of pipelines that were ever added to the hash. This
   * is not decremented when a pipeline is removed. It is only used to
   * generate a warning if an unusually high number of pipelines are
   * generated */
  int n_unique_pipelines;

  /* This is the expected minimum size we could prune the hash table
   * to if we were to remove all pipelines that are not in use. This
   * is only updated after we prune the table */
  int expected_min_size;

  /* String that will be used to describe the usage of this hash table
   * in the debug warning when too many pipelines are generated. This
   * must be a static string because it won't be copied or freed */
  const char *debug_string;

  unsigned int main_state;
  unsigned int layer_state;

  GHashTable *table;
} CoglPipelineHashTable;

void
_cogl_pipeline_hash_table_init (CoglPipelineHashTable *hash,
                                unsigned int main_state,
                                unsigned int layer_state,
                                const char *debug_string);

void
_cogl_pipeline_hash_table_destroy (CoglPipelineHashTable *hash);

/*
 * Gets a pipeline from the hash that has the same state as
 * @key_pipeline according to the limited state bits passed to
 * _cogl_pipeline_hash_table_init(). If there is no matching pipelines
 * already then a copy of key_pipeline is stored in the hash so that
 * it will be used next time the function is called with a similar
 * pipeline. In that case the copy itself will be returned
 */
CoglPipelineCacheEntry *
_cogl_pipeline_hash_table_get (CoglPipelineHashTable *hash,
                               CoglPipeline *key_pipeline);

#endif /* __COGL_PIPELINE_HASH_H__ */
