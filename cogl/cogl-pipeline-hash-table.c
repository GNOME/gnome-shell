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
#include "cogl-pipeline-hash-table.h"

typedef struct
{
  /* The template pipeline */
  CoglPipeline *pipeline;

  /* Calculating the hash is a little bit expensive for pipelines so
   * we don't want to do it repeatedly for entries that are already in
   * the hash table. Instead we cache the value here and calculate it
   * outside of the GHashTable. */
  unsigned int hash_value;

  /* GHashTable annoyingly doesn't let us pass a user data pointer to
   * the hash and equal functions so to work around it we have to
   * store the pointer in every hash table entry. We will use this
   * entry as both the key and the value */
  CoglPipelineHashTable *hash;
} CoglPipelineHashTableEntry;

static void
value_destroy_cb (void *value)
{
  CoglPipelineHashTableEntry *entry = value;

  cogl_object_unref (entry->pipeline);

  g_slice_free (CoglPipelineHashTableEntry, entry);
}

static unsigned int
entry_hash (const void *data)
{
  const CoglPipelineHashTableEntry *entry = data;

  return entry->hash_value;
}

static CoglBool
entry_equal (const void *a,
             const void *b)
{
  const CoglPipelineHashTableEntry *entry_a = a;
  const CoglPipelineHashTableEntry *entry_b = b;
  const CoglPipelineHashTable *hash = entry_a->hash;

  return _cogl_pipeline_equal (entry_a->pipeline,
                               entry_b->pipeline,
                               hash->main_state,
                               hash->layer_state,
                               0);
}

void
_cogl_pipeline_hash_table_init (CoglPipelineHashTable *hash,
                                unsigned int main_state,
                                unsigned int layer_state,
                                const char *debug_string)
{
  hash->n_unique_pipelines = 0;
  hash->debug_string = debug_string;
  hash->main_state = main_state;
  hash->layer_state = layer_state;
  hash->table = g_hash_table_new_full (entry_hash,
                                       entry_equal,
                                       NULL, /* key destroy */
                                       value_destroy_cb);
}

void
_cogl_pipeline_hash_table_destroy (CoglPipelineHashTable *hash)
{
  g_hash_table_destroy (hash->table);
}

CoglPipeline *
_cogl_pipeline_hash_table_get (CoglPipelineHashTable *hash,
                               CoglPipeline *key_pipeline)
{
  CoglPipelineHashTableEntry dummy_entry;
  CoglPipelineHashTableEntry *entry;
  unsigned int copy_state;

  dummy_entry.pipeline = key_pipeline;
  dummy_entry.hash = hash;
  dummy_entry.hash_value = _cogl_pipeline_hash (key_pipeline,
                                                hash->main_state,
                                                hash->layer_state,
                                                0);
  entry = g_hash_table_lookup (hash->table, &dummy_entry);

  if (entry)
    return entry->pipeline;

  if (hash->n_unique_pipelines == 50)
    g_warning ("Over 50 separate %s have been generated which is very "
               "unusual, so something is probably wrong!\n",
               hash->debug_string);

  /* XXX: Any keys referenced by the hash table need to remain valid
   * all the while that there are corresponding values, so for now we
   * simply make a copy of the current authority pipeline.
   *
   * FIXME: A problem with this is that our key into the cache may
   * hold references to some arbitrary user textures which will now be
   * kept alive indefinitly which is a shame. A better solution will
   * be to derive a special "key pipeline" from the authority which
   * derives from the base Cogl pipeline (to avoid affecting the
   * lifetime of any other pipelines) and only takes a copy of the
   * state that relates to the fragment shader and references small
   * dummy textures instead of potentially large user textures.
   */
  entry = g_slice_new (CoglPipelineHashTableEntry);
  entry->pipeline = cogl_pipeline_copy (key_pipeline);
  entry->hash = hash;
  entry->hash_value = dummy_entry.hash_value;

  g_hash_table_insert (hash->table, entry, entry);

  hash->n_unique_pipelines++;

  return entry->pipeline;
}
