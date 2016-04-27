/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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

#ifndef __COGL_PIPELINE_LAYER_STATE_PRIVATE_H
#define __COGL_PIPELINE_LAYER_STATE_PRIVATE_H

#include "cogl-pipeline-layer-state.h"
#include "cogl-pipeline-private.h"

CoglPipelineLayer *
_cogl_pipeline_set_layer_unit (CoglPipeline *required_owner,
                               CoglPipelineLayer *layer,
                               int unit_index);

CoglPipelineFilter
_cogl_pipeline_get_layer_min_filter (CoglPipeline *pipeline,
                                     int layer_index);

CoglPipelineFilter
_cogl_pipeline_get_layer_mag_filter (CoglPipeline *pipeline,
                                     int layer_index);

CoglBool
_cogl_pipeline_layer_texture_type_equal (CoglPipelineLayer *authority0,
                                         CoglPipelineLayer *authority1,
                                         CoglPipelineEvalFlags flags);

CoglBool
_cogl_pipeline_layer_texture_data_equal (CoglPipelineLayer *authority0,
                                         CoglPipelineLayer *authority1,
                                         CoglPipelineEvalFlags flags);

CoglBool
_cogl_pipeline_layer_combine_state_equal (CoglPipelineLayer *authority0,
                                          CoglPipelineLayer *authority1);

CoglBool
_cogl_pipeline_layer_combine_constant_equal (CoglPipelineLayer *authority0,
                                             CoglPipelineLayer *authority1);

CoglBool
_cogl_pipeline_layer_sampler_equal (CoglPipelineLayer *authority0,
                                    CoglPipelineLayer *authority1);

CoglBool
_cogl_pipeline_layer_user_matrix_equal (CoglPipelineLayer *authority0,
                                        CoglPipelineLayer *authority1);

CoglBool
_cogl_pipeline_layer_point_sprite_coords_equal (CoglPipelineLayer *authority0,
                                                CoglPipelineLayer *authority1);

CoglBool
_cogl_pipeline_layer_vertex_snippets_equal (CoglPipelineLayer *authority0,
                                            CoglPipelineLayer *authority1);

CoglBool
_cogl_pipeline_layer_fragment_snippets_equal (CoglPipelineLayer *authority0,
                                              CoglPipelineLayer *authority1);

void
_cogl_pipeline_layer_hash_unit_state (CoglPipelineLayer *authority,
                                      CoglPipelineLayer **authorities,
                                      CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_texture_type_state (CoglPipelineLayer *authority,
                                              CoglPipelineLayer **authorities,
                                              CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_texture_data_state (CoglPipelineLayer *authority,
                                              CoglPipelineLayer **authorities,
                                              CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_sampler_state (CoglPipelineLayer *authority,
                                         CoglPipelineLayer **authorities,
                                         CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_combine_state (CoglPipelineLayer *authority,
                                         CoglPipelineLayer **authorities,
                                         CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_combine_constant_state (CoglPipelineLayer *authority,
                                                  CoglPipelineLayer **authorities,
                                                  CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_user_matrix_state (CoglPipelineLayer *authority,
                                             CoglPipelineLayer **authorities,
                                             CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_point_sprite_state (CoglPipelineLayer *authority,
                                              CoglPipelineLayer **authorities,
                                              CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_vertex_snippets_state (CoglPipelineLayer *authority,
                                                 CoglPipelineLayer **authorities,
                                                 CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_fragment_snippets_state (CoglPipelineLayer *authority,
                                                   CoglPipelineLayer **authorities,
                                                   CoglPipelineHashState *state);

#endif /* __COGL_PIPELINE_LAYER_STATE_PRIVATE_H */
