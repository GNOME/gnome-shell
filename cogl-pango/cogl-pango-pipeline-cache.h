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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __COGL_PANGO_PIPELINE_CACHE_H__
#define __COGL_PANGO_PIPELINE_CACHE_H__

#include <glib.h>

#include "cogl/cogl-context-private.h"

COGL_BEGIN_DECLS

typedef struct _CoglPangoPipelineCache
{
  CoglContext *ctx;

  GHashTable *hash_table;

  CoglPipeline *base_texture_alpha_pipeline;
  CoglPipeline *base_texture_rgba_pipeline;

  CoglBool use_mipmapping;
} CoglPangoPipelineCache;


CoglPangoPipelineCache *
_cogl_pango_pipeline_cache_new (CoglContext *ctx,
                                CoglBool use_mipmapping);

/* Returns a pipeline that can be used to render glyphs in the given
   texture. The pipeline has a new reference so it is up to the caller
   to unref it */
CoglPipeline *
_cogl_pango_pipeline_cache_get (CoglPangoPipelineCache *cache,
                                CoglTexture *texture);

void
_cogl_pango_pipeline_cache_free (CoglPangoPipelineCache *cache);

COGL_END_DECLS

#endif /* __COGL_PANGO_PIPELINE_CACHE_H__ */
