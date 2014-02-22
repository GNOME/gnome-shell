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
 */

#ifndef __COGL_SAMPLER_CACHE_PRIVATE_H
#define __COGL_SAMPLER_CACHE_PRIVATE_H

#include "cogl-context.h"
#include "cogl-gl-header.h"

/* These aren't defined in the GLES headers */
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif
#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT 0x8370
#endif

/* GL_ALWAYS is just used here as a value that is known not to clash
 * with any valid GL wrap modes.
 *
 * XXX: keep the values in sync with the CoglPipelineWrapMode enum
 * so no conversion is actually needed.
 */
typedef enum _CoglSamplerCacheWrapMode
{
  COGL_SAMPLER_CACHE_WRAP_MODE_REPEAT = GL_REPEAT,
  COGL_SAMPLER_CACHE_WRAP_MODE_MIRRORED_REPEAT = GL_MIRRORED_REPEAT,
  COGL_SAMPLER_CACHE_WRAP_MODE_CLAMP_TO_EDGE = GL_CLAMP_TO_EDGE,
  COGL_SAMPLER_CACHE_WRAP_MODE_CLAMP_TO_BORDER = GL_CLAMP_TO_BORDER,
  COGL_SAMPLER_CACHE_WRAP_MODE_AUTOMATIC = GL_ALWAYS
} CoglSamplerCacheWrapMode;

typedef struct _CoglSamplerCache CoglSamplerCache;

typedef struct _CoglSamplerCacheEntry
{
  GLuint sampler_object;

  GLenum min_filter;
  GLenum mag_filter;

  CoglSamplerCacheWrapMode wrap_mode_s;
  CoglSamplerCacheWrapMode wrap_mode_t;
  CoglSamplerCacheWrapMode wrap_mode_p;
} CoglSamplerCacheEntry;

CoglSamplerCache *
_cogl_sampler_cache_new (CoglContext *context);

const CoglSamplerCacheEntry *
_cogl_sampler_cache_get_default_entry (CoglSamplerCache *cache);

const CoglSamplerCacheEntry *
_cogl_sampler_cache_update_wrap_modes (CoglSamplerCache *cache,
                                       const CoglSamplerCacheEntry *old_entry,
                                       CoglSamplerCacheWrapMode wrap_mode_s,
                                       CoglSamplerCacheWrapMode wrap_mode_t,
                                       CoglSamplerCacheWrapMode wrap_mode_p);

const CoglSamplerCacheEntry *
_cogl_sampler_cache_update_filters (CoglSamplerCache *cache,
                                    const CoglSamplerCacheEntry *old_entry,
                                    GLenum min_filter,
                                    GLenum mag_filter);

void
_cogl_sampler_cache_free (CoglSamplerCache *cache);

#endif /* __COGL_SAMPLER_CACHE_PRIVATE_H */
