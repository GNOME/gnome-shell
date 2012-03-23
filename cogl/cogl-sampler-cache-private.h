/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#ifndef __COGL_SAMPLER_CACHE_PRIVATE_H
#define __COGL_SAMPLER_CACHE_PRIVATE_H

#include "cogl-context.h"
#include "cogl-gl-header.h"

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
