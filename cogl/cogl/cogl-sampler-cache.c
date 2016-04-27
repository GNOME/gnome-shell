/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-sampler-cache-private.h"
#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"

#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R 0x8072
#endif

struct _CoglSamplerCache
{
  CoglContext *context;

  /* The samplers are hashed in two tables. One is using the enum
     values that Cogl exposes (so it can include the 'automatic' wrap
     mode) and the other is using the converted values that will be
     given to GL. The first is used to get a unique pointer for the
     sampler state so that pipelines only need to store a single
     pointer instead of the whole state and the second is used so that
     only a single GL sampler object will be created for each unique
     GL state. */
  GHashTable *hash_table_cogl;
  GHashTable *hash_table_gl;

  /* This is used for generated fake unique sampler object numbers
     when the sampler object extension is not supported */
  GLuint next_fake_sampler_object_number;
};

static CoglSamplerCacheWrapMode
get_real_wrap_mode (CoglSamplerCacheWrapMode wrap_mode)
{
  if (wrap_mode == COGL_SAMPLER_CACHE_WRAP_MODE_AUTOMATIC)
    return COGL_SAMPLER_CACHE_WRAP_MODE_CLAMP_TO_EDGE;

  return wrap_mode;
}

static void
canonicalize_key (CoglSamplerCacheEntry *key)
{
  /* This converts the wrap modes to the enums that will actually be
     given to GL so that it can be used as a key to get a unique GL
     sampler object for the state */
  key->wrap_mode_s = get_real_wrap_mode (key->wrap_mode_s);
  key->wrap_mode_t = get_real_wrap_mode (key->wrap_mode_t);
  key->wrap_mode_p = get_real_wrap_mode (key->wrap_mode_p);
}

static CoglBool
wrap_mode_equal_gl (CoglSamplerCacheWrapMode wrap_mode0,
                    CoglSamplerCacheWrapMode wrap_mode1)
{
  /* We want to compare the actual GLenum that will be used so that if
     two different wrap_modes actually use the same GL state we'll
     still use the same sampler object */
  return get_real_wrap_mode (wrap_mode0) == get_real_wrap_mode (wrap_mode1);
}

static CoglBool
sampler_state_equal_gl (const void *value0,
                        const void *value1)
{
  const CoglSamplerCacheEntry *state0 = value0;
  const CoglSamplerCacheEntry *state1 = value1;

  return (state0->mag_filter == state1->mag_filter &&
          state0->min_filter == state1->min_filter &&
          wrap_mode_equal_gl (state0->wrap_mode_s, state1->wrap_mode_s) &&
          wrap_mode_equal_gl (state0->wrap_mode_t, state1->wrap_mode_t) &&
          wrap_mode_equal_gl (state0->wrap_mode_p, state1->wrap_mode_p));
}

static unsigned int
hash_wrap_mode_gl (unsigned int hash,
                   CoglSamplerCacheWrapMode wrap_mode)
{
  /* We want to hash the actual GLenum that will be used so that if
     two different wrap_modes actually use the same GL state we'll
     still use the same sampler object */
  GLenum real_wrap_mode = get_real_wrap_mode (wrap_mode);

  return _cogl_util_one_at_a_time_hash (hash,
                                        &real_wrap_mode,
                                        sizeof (real_wrap_mode));
}

static unsigned int
hash_sampler_state_gl (const void *key)
{
  const CoglSamplerCacheEntry *entry = key;
  unsigned int hash = 0;

  hash = _cogl_util_one_at_a_time_hash (hash, &entry->mag_filter,
                                        sizeof (entry->mag_filter));
  hash = _cogl_util_one_at_a_time_hash (hash, &entry->min_filter,
                                        sizeof (entry->min_filter));
  hash = hash_wrap_mode_gl (hash, entry->wrap_mode_s);
  hash = hash_wrap_mode_gl (hash, entry->wrap_mode_t);
  hash = hash_wrap_mode_gl (hash, entry->wrap_mode_p);

  return _cogl_util_one_at_a_time_mix (hash);
}

static CoglBool
sampler_state_equal_cogl (const void *value0,
                          const void *value1)
{
  const CoglSamplerCacheEntry *state0 = value0;
  const CoglSamplerCacheEntry *state1 = value1;

  return (state0->mag_filter == state1->mag_filter &&
          state0->min_filter == state1->min_filter &&
          state0->wrap_mode_s == state1->wrap_mode_s &&
          state0->wrap_mode_t == state1->wrap_mode_t &&
          state0->wrap_mode_p == state1->wrap_mode_p);
}

static unsigned int
hash_sampler_state_cogl (const void *key)
{
  const CoglSamplerCacheEntry *entry = key;
  unsigned int hash = 0;

  hash = _cogl_util_one_at_a_time_hash (hash, &entry->mag_filter,
                                        sizeof (entry->mag_filter));
  hash = _cogl_util_one_at_a_time_hash (hash, &entry->min_filter,
                                        sizeof (entry->min_filter));
  hash = _cogl_util_one_at_a_time_hash (hash, &entry->wrap_mode_s,
                                        sizeof (entry->wrap_mode_s));
  hash = _cogl_util_one_at_a_time_hash (hash, &entry->wrap_mode_t,
                                        sizeof (entry->wrap_mode_t));
  hash = _cogl_util_one_at_a_time_hash (hash, &entry->wrap_mode_p,
                                        sizeof (entry->wrap_mode_p));

  return _cogl_util_one_at_a_time_mix (hash);
}

CoglSamplerCache *
_cogl_sampler_cache_new (CoglContext *context)
{
  CoglSamplerCache *cache = g_new (CoglSamplerCache, 1);

  /* No reference is taken on the context because it would create a
     circular reference */
  cache->context = context;

  cache->hash_table_gl = g_hash_table_new (hash_sampler_state_gl,
                                           sampler_state_equal_gl);
  cache->hash_table_cogl = g_hash_table_new (hash_sampler_state_cogl,
                                             sampler_state_equal_cogl);
  cache->next_fake_sampler_object_number = 1;

  return cache;
}

static void
set_wrap_mode (CoglContext *context,
               GLuint sampler_object,
               GLenum param,
               CoglSamplerCacheWrapMode wrap_mode)
{
  GE( context, glSamplerParameteri (sampler_object,
                                    param,
                                    wrap_mode) );
}

static CoglSamplerCacheEntry *
_cogl_sampler_cache_get_entry_gl (CoglSamplerCache *cache,
                                  const CoglSamplerCacheEntry *key)
{
  CoglSamplerCacheEntry *entry;

  entry = g_hash_table_lookup (cache->hash_table_gl, key);

  if (entry == NULL)
    {
      CoglContext *context = cache->context;

      entry = g_slice_dup (CoglSamplerCacheEntry, key);

      if (_cogl_has_private_feature (context,
                                     COGL_PRIVATE_FEATURE_SAMPLER_OBJECTS))
        {
          GE( context, glGenSamplers (1, &entry->sampler_object) );

          GE( context, glSamplerParameteri (entry->sampler_object,
                                            GL_TEXTURE_MIN_FILTER,
                                            entry->min_filter) );
          GE( context, glSamplerParameteri (entry->sampler_object,
                                            GL_TEXTURE_MAG_FILTER,
                                            entry->mag_filter) );

          set_wrap_mode (context,
                         entry->sampler_object,
                         GL_TEXTURE_WRAP_S,
                         entry->wrap_mode_s);
          set_wrap_mode (context,
                         entry->sampler_object,
                         GL_TEXTURE_WRAP_T,
                         entry->wrap_mode_t);
          set_wrap_mode (context,
                         entry->sampler_object,
                         GL_TEXTURE_WRAP_R,
                         entry->wrap_mode_p);
        }
      else
        {
          /* If sampler objects aren't supported then we'll invent a
             unique number so that pipelines can still compare the
             unique state just by comparing the sampler object
             numbers */
          entry->sampler_object = cache->next_fake_sampler_object_number++;
        }

      g_hash_table_insert (cache->hash_table_gl, entry, entry);
    }

  return entry;
}

static CoglSamplerCacheEntry *
_cogl_sampler_cache_get_entry_cogl (CoglSamplerCache *cache,
                                    const CoglSamplerCacheEntry *key)
{
  CoglSamplerCacheEntry *entry;

  entry = g_hash_table_lookup (cache->hash_table_cogl, key);

  if (entry == NULL)
    {
      CoglSamplerCacheEntry canonical_key;
      CoglSamplerCacheEntry *gl_entry;

      entry = g_slice_dup (CoglSamplerCacheEntry, key);

      /* Get the sampler object number from the canonical GL version
         of the sampler state cache */
      canonical_key = *key;
      canonicalize_key (&canonical_key);
      gl_entry = _cogl_sampler_cache_get_entry_gl (cache, &canonical_key);
      entry->sampler_object = gl_entry->sampler_object;

      g_hash_table_insert (cache->hash_table_cogl, entry, entry);
    }

  return entry;
}

const CoglSamplerCacheEntry *
_cogl_sampler_cache_get_default_entry (CoglSamplerCache *cache)
{
  CoglSamplerCacheEntry key;

  key.wrap_mode_s = COGL_SAMPLER_CACHE_WRAP_MODE_AUTOMATIC;
  key.wrap_mode_t = COGL_SAMPLER_CACHE_WRAP_MODE_AUTOMATIC;
  key.wrap_mode_p = COGL_SAMPLER_CACHE_WRAP_MODE_AUTOMATIC;

  key.min_filter = GL_LINEAR;
  key.mag_filter = GL_LINEAR;

  return _cogl_sampler_cache_get_entry_cogl (cache, &key);
}

const CoglSamplerCacheEntry *
_cogl_sampler_cache_update_wrap_modes (CoglSamplerCache *cache,
                                       const CoglSamplerCacheEntry *old_entry,
                                       CoglSamplerCacheWrapMode wrap_mode_s,
                                       CoglSamplerCacheWrapMode wrap_mode_t,
                                       CoglSamplerCacheWrapMode wrap_mode_p)
{
  CoglSamplerCacheEntry key = *old_entry;

  key.wrap_mode_s = wrap_mode_s;
  key.wrap_mode_t = wrap_mode_t;
  key.wrap_mode_p = wrap_mode_p;

  return _cogl_sampler_cache_get_entry_cogl (cache, &key);
}

const CoglSamplerCacheEntry *
_cogl_sampler_cache_update_filters (CoglSamplerCache *cache,
                                    const CoglSamplerCacheEntry *old_entry,
                                    GLenum min_filter,
                                    GLenum mag_filter)
{
  CoglSamplerCacheEntry key = *old_entry;

  key.min_filter = min_filter;
  key.mag_filter = mag_filter;

  return _cogl_sampler_cache_get_entry_cogl (cache, &key);
}

static void
hash_table_free_gl_cb (void *key,
                       void *value,
                       void *user_data)
{
  CoglContext *context = user_data;
  CoglSamplerCacheEntry *entry = value;

  if (_cogl_has_private_feature (context,
                                 COGL_PRIVATE_FEATURE_SAMPLER_OBJECTS))
    GE( context, glDeleteSamplers (1, &entry->sampler_object) );

  g_slice_free (CoglSamplerCacheEntry, entry);
}

static void
hash_table_free_cogl_cb (void *key,
                         void *value,
                         void *user_data)
{
  CoglSamplerCacheEntry *entry = value;

  g_slice_free (CoglSamplerCacheEntry, entry);
}

void
_cogl_sampler_cache_free (CoglSamplerCache *cache)
{
  g_hash_table_foreach (cache->hash_table_gl,
                        hash_table_free_gl_cb,
                        cache->context);

  g_hash_table_destroy (cache->hash_table_gl);

  g_hash_table_foreach (cache->hash_table_cogl,
                        hash_table_free_cogl_cb,
                        cache->context);

  g_hash_table_destroy (cache->hash_table_cogl);

  g_free (cache);
}
