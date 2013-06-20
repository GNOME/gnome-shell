/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
