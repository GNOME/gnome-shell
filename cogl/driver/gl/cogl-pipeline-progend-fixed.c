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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-pipeline-private.h"
#include "cogl-pipeline-state-private.h"

#ifdef COGL_PIPELINE_PROGEND_FIXED

#include "cogl-context.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"

static CoglBool
_cogl_pipeline_progend_fixed_start (CoglPipeline *pipeline)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_FIXED)))
    return FALSE;

  if (!_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_GL_FIXED))
    return FALSE;

  /* Vertex snippets are only supported in the GLSL fragend */
  if (_cogl_pipeline_has_vertex_snippets (pipeline))
    return FALSE;

  /* Fragment snippets are only supported in the GLSL fragend */
  if (_cogl_pipeline_has_fragment_snippets (pipeline))
    return FALSE;

  /* If there is a user program then the appropriate backend for that
   * language should handle it. */
  if (cogl_pipeline_get_user_program (pipeline))
    return FALSE;

  /* The fixed progend can't handle the per-vertex point size
   * attribute */
  if (cogl_pipeline_get_per_vertex_point_size (pipeline))
    return FALSE;

  return TRUE;
}

static void
_cogl_pipeline_progend_fixed_pre_paint (CoglPipeline *pipeline,
                                        CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;

  if (ctx->current_projection_entry)
    _cogl_matrix_entry_flush_to_gl_builtins (ctx,
                                             ctx->current_projection_entry,
                                             COGL_MATRIX_PROJECTION,
                                             framebuffer,
                                             FALSE /* enable flip */);
  if (ctx->current_modelview_entry)
    _cogl_matrix_entry_flush_to_gl_builtins (ctx,
                                             ctx->current_modelview_entry,
                                             COGL_MATRIX_MODELVIEW,
                                             framebuffer,
                                             FALSE /* enable flip */);
}

const CoglPipelineProgend _cogl_pipeline_fixed_progend =
  {
    COGL_PIPELINE_VERTEND_FIXED,
    COGL_PIPELINE_FRAGEND_FIXED,
    _cogl_pipeline_progend_fixed_start,
    NULL, /* end */
    NULL, /* pre_change_notify */
    NULL, /* layer_pre_change_notify */
    _cogl_pipeline_progend_fixed_pre_paint
  };

#endif /* COGL_PIPELINE_PROGEND_FIXED */
