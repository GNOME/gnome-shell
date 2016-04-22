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
#include "cogl-pipeline-cache.h"

typedef struct
{
  CoglPipelineCacheEntry parent;

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

  /* The number of unique pipelines that had been created when this
   * pipeline was last accessed */
  int age;
} CoglPipelineHashTableEntry;

static void
value_destroy_cb (void *value)
{
  CoglPipelineHashTableEntry *entry = value;

  cogl_object_unref (entry->parent.pipeline);

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

  return _cogl_pipeline_equal (entry_a->parent.pipeline,
                               entry_b->parent.pipeline,
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
  /* We'll only start pruning once we get to 16 unique pipelines */
  hash->expected_min_size = 8;
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

static void
collect_prunable_entries_cb (void *key,
                             void *value,
                             void *user_data)
{
  GQueue *entries = user_data;
  CoglPipelineCacheEntry *entry = value;

  if (entry->usage_count == 0)
    g_queue_push_tail (entries, entry);
}

static int
compare_pipeline_age_cb (const void *a,
                         const void *b)
{
  const CoglPipelineHashTableEntry *ae = a;
  const CoglPipelineHashTableEntry *be = b;

  return be->age - ae->age;
}

static void
prune_old_pipelines (CoglPipelineHashTable *hash)
{
  GQueue entries;
  GList *l;
  int i;

  /* Collect all of the prunable entries into a GQueue */
  g_queue_init (&entries);
  g_hash_table_foreach (hash->table,
                        collect_prunable_entries_cb,
                        &entries);

  /* Sort the entries by increasing order of age */
  entries.head = g_list_sort (entries.head, compare_pipeline_age_cb);

  /* The +1 is to include the pipeline that we're about to add */
  hash->expected_min_size = (g_hash_table_size (hash->table) -
                             entries.length +
                             1);

  /* Remove oldest half of the prunable pipelines. We still want to
   * keep some of the prunable entries that are recently used because
   * it's not unlikely that the application will recreate the same
   * pipeline */
  for (l = entries.head, i = 0; i < entries.length / 2; l = l->next, i++)
    {
      CoglPipelineCacheEntry *entry = l->data;

      g_hash_table_remove (hash->table, entry);
    }

  g_list_free (entries.head);
}

CoglPipelineCacheEntry *
_cogl_pipeline_hash_table_get (CoglPipelineHashTable *hash,
                               CoglPipeline *key_pipeline)
{
  CoglPipelineHashTableEntry dummy_entry;
  CoglPipelineHashTableEntry *entry;
  unsigned int copy_state;

  dummy_entry.parent.pipeline = key_pipeline;
  dummy_entry.hash = hash;
  dummy_entry.hash_value = _cogl_pipeline_hash (key_pipeline,
                                                hash->main_state,
                                                hash->layer_state,
                                                0);
  entry = g_hash_table_lookup (hash->table, &dummy_entry);

  if (entry)
    {
      entry->age = hash->n_unique_pipelines;
      return &entry->parent;
    }

  if (hash->n_unique_pipelines == 50)
    g_warning ("Over 50 separate %s have been generated which is very "
               "unusual, so something is probably wrong!\n",
               hash->debug_string);

  /* If we are going to have more than twice the expected minimum
   * number of pipelines in the hash then we'll try pruning and update
   * the minimum */
  if (g_hash_table_size (hash->table) >= hash->expected_min_size * 2)
    prune_old_pipelines (hash);

  entry = g_slice_new (CoglPipelineHashTableEntry);
  entry->parent.usage_count = 0;
  entry->hash = hash;
  entry->hash_value = dummy_entry.hash_value;
  entry->age = hash->n_unique_pipelines;

  copy_state = hash->main_state;
  if (hash->layer_state)
    copy_state |= COGL_PIPELINE_STATE_LAYERS;

  /* Create a new pipeline that is a child of the root pipeline
   * instead of a normal copy so that the template pipeline won't hold
   * a reference to the original pipeline */
  entry->parent.pipeline = _cogl_pipeline_deep_copy (key_pipeline,
                                                     copy_state,
                                                     hash->layer_state);

  g_hash_table_insert (hash->table, entry, entry);

  hash->n_unique_pipelines++;

  return &entry->parent;
}
