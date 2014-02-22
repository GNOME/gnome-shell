/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
