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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_PIPELINE_STATE_PRIVATE_H
#define __COGL_PIPELINE_STATE_PRIVATE_H

CoglPipeline *
_cogl_pipeline_get_user_program (CoglPipeline *pipeline);

CoglBool
_cogl_pipeline_has_vertex_snippets (CoglPipeline *pipeline);

CoglBool
_cogl_pipeline_has_fragment_snippets (CoglPipeline *pipeline);

CoglBool
_cogl_pipeline_has_non_layer_vertex_snippets (CoglPipeline *pipeline);

CoglBool
_cogl_pipeline_has_non_layer_fragment_snippets (CoglPipeline *pipeline);

void
_cogl_pipeline_set_fog_state (CoglPipeline *pipeline,
                              const CoglPipelineFogState *fog_state);

CoglBool
_cogl_pipeline_color_equal (CoglPipeline *authority0,
                            CoglPipeline *authority1);

CoglBool
_cogl_pipeline_lighting_state_equal (CoglPipeline *authority0,
                                     CoglPipeline *authority1);

CoglBool
_cogl_pipeline_alpha_func_state_equal (CoglPipeline *authority0,
                                       CoglPipeline *authority1);

CoglBool
_cogl_pipeline_alpha_func_reference_state_equal (CoglPipeline *authority0,
                                                 CoglPipeline *authority1);

CoglBool
_cogl_pipeline_blend_state_equal (CoglPipeline *authority0,
                                  CoglPipeline *authority1);

CoglBool
_cogl_pipeline_depth_state_equal (CoglPipeline *authority0,
                                  CoglPipeline *authority1);

CoglBool
_cogl_pipeline_fog_state_equal (CoglPipeline *authority0,
                                CoglPipeline *authority1);

CoglBool
_cogl_pipeline_non_zero_point_size_equal (CoglPipeline *authority0,
                                          CoglPipeline *authority1);

CoglBool
_cogl_pipeline_point_size_equal (CoglPipeline *authority0,
                                 CoglPipeline *authority1);
CoglBool
_cogl_pipeline_per_vertex_point_size_equal (CoglPipeline *authority0,
                                            CoglPipeline *authority1);

CoglBool
_cogl_pipeline_logic_ops_state_equal (CoglPipeline *authority0,
                                      CoglPipeline *authority1);

CoglBool
_cogl_pipeline_user_shader_equal (CoglPipeline *authority0,
                                  CoglPipeline *authority1);

CoglBool
_cogl_pipeline_cull_face_state_equal (CoglPipeline *authority0,
                                      CoglPipeline *authority1);

CoglBool
_cogl_pipeline_uniforms_state_equal (CoglPipeline *authority0,
                                     CoglPipeline *authority1);

CoglBool
_cogl_pipeline_vertex_snippets_state_equal (CoglPipeline *authority0,
                                            CoglPipeline *authority1);

CoglBool
_cogl_pipeline_fragment_snippets_state_equal (CoglPipeline *authority0,
                                              CoglPipeline *authority1);

void
_cogl_pipeline_hash_color_state (CoglPipeline *authority,
                                 CoglPipelineHashState *state);

void
_cogl_pipeline_hash_blend_enable_state (CoglPipeline *authority,
                                        CoglPipelineHashState *state);

void
_cogl_pipeline_hash_layers_state (CoglPipeline *authority,
                                  CoglPipelineHashState *state);

void
_cogl_pipeline_hash_lighting_state (CoglPipeline *authority,
                                    CoglPipelineHashState *state);

void
_cogl_pipeline_hash_alpha_func_state (CoglPipeline *authority,
                                      CoglPipelineHashState *state);

void
_cogl_pipeline_hash_alpha_func_reference_state (CoglPipeline *authority,
                                                CoglPipelineHashState *state);

void
_cogl_pipeline_hash_blend_state (CoglPipeline *authority,
                                 CoglPipelineHashState *state);

void
_cogl_pipeline_hash_user_shader_state (CoglPipeline *authority,
                                       CoglPipelineHashState *state);

void
_cogl_pipeline_hash_depth_state (CoglPipeline *authority,
                                 CoglPipelineHashState *state);

void
_cogl_pipeline_hash_fog_state (CoglPipeline *authority,
                               CoglPipelineHashState *state);

void
_cogl_pipeline_hash_non_zero_point_size_state (CoglPipeline *authority,
                                               CoglPipelineHashState *state);

void
_cogl_pipeline_hash_point_size_state (CoglPipeline *authority,
                                      CoglPipelineHashState *state);

void
_cogl_pipeline_hash_per_vertex_point_size_state (CoglPipeline *authority,
                                                 CoglPipelineHashState *state);

void
_cogl_pipeline_hash_logic_ops_state (CoglPipeline *authority,
                                     CoglPipelineHashState *state);

void
_cogl_pipeline_hash_cull_face_state (CoglPipeline *authority,
                                     CoglPipelineHashState *state);

void
_cogl_pipeline_hash_uniforms_state (CoglPipeline *authority,
                                    CoglPipelineHashState *state);

void
_cogl_pipeline_hash_vertex_snippets_state (CoglPipeline *authority,
                                           CoglPipelineHashState *state);

void
_cogl_pipeline_hash_fragment_snippets_state (CoglPipeline *authority,
                                             CoglPipelineHashState *state);

void
_cogl_pipeline_compare_uniform_differences (unsigned long *differences,
                                            CoglPipeline *pipeline0,
                                            CoglPipeline *pipeline1);

#endif /* __COGL_PIPELINE_STATE_PRIVATE_H */
