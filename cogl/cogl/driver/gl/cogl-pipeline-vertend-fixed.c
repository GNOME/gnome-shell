/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-framebuffer-private.h"

#ifdef COGL_PIPELINE_VERTEND_FIXED

#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-program-private.h"

const CoglPipelineVertend _cogl_pipeline_fixed_vertend;

static void
_cogl_pipeline_vertend_fixed_start (CoglPipeline *pipeline,
                                    int n_layers,
                                    unsigned long pipelines_difference)
{
  _cogl_use_vertex_program (0, COGL_PIPELINE_PROGRAM_TYPE_FIXED);
}

static CoglBool
_cogl_pipeline_vertend_fixed_add_layer (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        unsigned long layers_difference,
                                        CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
  CoglTextureUnit *unit = _cogl_get_texture_unit (unit_index);

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_USER_MATRIX)
    {
      CoglPipelineLayerState state = COGL_PIPELINE_LAYER_STATE_USER_MATRIX;
      CoglPipelineLayer *authority =
        _cogl_pipeline_layer_get_authority (layer, state);
      CoglMatrixEntry *matrix_entry;

      cogl_matrix_stack_set (unit->matrix_stack,
                             &authority->big_state->matrix);

      _cogl_set_active_texture_unit (unit_index);

      matrix_entry = unit->matrix_stack->last_entry;
      _cogl_matrix_entry_flush_to_gl_builtins (ctx, matrix_entry,
                                               COGL_MATRIX_TEXTURE,
                                               framebuffer,
                                               FALSE /* enable flip */);
    }

  return TRUE;
}

static CoglBool
_cogl_pipeline_vertend_fixed_end (CoglPipeline *pipeline,
                                  unsigned long pipelines_difference)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  if (pipelines_difference & COGL_PIPELINE_STATE_POINT_SIZE)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_POINT_SIZE);

      if (authority->big_state->point_size > 0.0f)
        GE( ctx, glPointSize (authority->big_state->point_size) );
    }

  return TRUE;
}

const CoglPipelineVertend _cogl_pipeline_fixed_vertend =
{
  _cogl_pipeline_vertend_fixed_start,
  _cogl_pipeline_vertend_fixed_add_layer,
  _cogl_pipeline_vertend_fixed_end,
  NULL, /* pipeline_change_notify */
  NULL /* layer_change_notify */
};

#endif /* COGL_PIPELINE_VERTEND_FIXED */

