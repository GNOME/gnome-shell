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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-pipeline-private.h"
#include "cogl-pipeline-state-private.h"

#ifdef COGL_PIPELINE_PROGEND_FIXED_ARBFP

#include "cogl-context.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-program-private.h"

static CoglBool
_cogl_pipeline_progend_fixed_arbfp_start (CoglPipeline *pipeline)
{
  CoglHandle user_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_FIXED)))
    return FALSE;

  if (!_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_GL_FIXED))
    return FALSE;

  /* Vertex snippets are only supported in the GLSL fragend */
  if (_cogl_pipeline_has_vertex_snippets (pipeline))
    return FALSE;

  /* Validate that we can handle the fragment state using ARBfp
   */

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_ARBFP))
    return FALSE;

  /* Fragment snippets are only supported in the GLSL fragend */
  if (_cogl_pipeline_has_fragment_snippets (pipeline))
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);
  if (user_program &&
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_ARBFP)
    return FALSE;

  /* The ARBfp progend can't handle the per-vertex point size
   * attribute */
  if (cogl_pipeline_get_per_vertex_point_size (pipeline))
    return FALSE;

  return TRUE;
}

static void
_cogl_pipeline_progend_fixed_arbfp_pre_paint (CoglPipeline *pipeline,
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

const CoglPipelineProgend _cogl_pipeline_fixed_arbfp_progend =
  {
    COGL_PIPELINE_VERTEND_FIXED,
    COGL_PIPELINE_FRAGEND_ARBFP,
    _cogl_pipeline_progend_fixed_arbfp_start,
    NULL, /* end */
    NULL, /* pre_change_notify */
    NULL, /* layer_pre_change_notify */
    _cogl_pipeline_progend_fixed_arbfp_pre_paint
  };

#endif /* COGL_PIPELINE_PROGEND_FIXED_ARBFP */
