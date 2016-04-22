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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_DEPTH_STATE_H__
#define __COGL_DEPTH_STATE_H__

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-depth-state
 * @short_description: Functions for describing the depth testing
 *                     state of your GPU.
 */

/**
 * CoglDepthState:
 *
 * Since: 2.0
 */
typedef struct {
  /*< private >*/
  uint32_t COGL_PRIVATE (magic);

  CoglBool COGL_PRIVATE (test_enabled);
  CoglDepthTestFunction COGL_PRIVATE (test_function);
  CoglBool COGL_PRIVATE (write_enabled);
  float COGL_PRIVATE (range_near);
  float COGL_PRIVATE (range_far);

  uint32_t COGL_PRIVATE (padding0);
  uint32_t COGL_PRIVATE (padding1);
  uint32_t COGL_PRIVATE (padding2);
  uint32_t COGL_PRIVATE (padding3);
  uint32_t COGL_PRIVATE (padding4);
  uint32_t COGL_PRIVATE (padding5);
  uint32_t COGL_PRIVATE (padding6);
  uint32_t COGL_PRIVATE (padding7);
  uint32_t COGL_PRIVATE (padding8);
  uint32_t COGL_PRIVATE (padding9);
} CoglDepthState;

/**
 * cogl_depth_state_init:
 * @state: A #CoglDepthState struct
 *
 * Initializes the members of @state to their default values.
 *
 * You should never pass an un initialized #CoglDepthState structure
 * to cogl_pipeline_set_depth_state().
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_depth_state_init (CoglDepthState *state);

/**
 * cogl_depth_state_set_test_enabled:
 * @state: A #CoglDepthState struct
 * @enable: The enable state you want
 *
 * Enables or disables depth testing according to the value of
 * @enable.
 *
 * If depth testing is enable then the #CoglDepthTestFunction set
 * using cogl_depth_state_set_test_function() us used to evaluate
 * the depth value of incoming fragments against the corresponding
 * value stored in the current depth buffer, and if the test passes
 * then the fragments depth value is used to update the depth buffer.
 * (unless you have disabled depth writing via
 * cogl_depth_state_set_write_enabled())
 *
 * By default depth testing is disabled.
 *
 * NB: this won't directly affect the state of the GPU. You have
 * to then set the state on a #CoglPipeline using
 * cogl_pipeline_set_depth_state()
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_depth_state_set_test_enabled (CoglDepthState *state,
                                   CoglBool enable);

/**
 * cogl_depth_state_get_test_enabled:
 * @state: A #CoglDepthState struct
 *
 * Gets the current depth test enabled state as previously set by
 * cogl_depth_state_set_test_enabled().
 *
 * Returns: The pipeline's current depth test enabled state.
 * Since: 2.0
 * Stability: Unstable
 */
CoglBool
cogl_depth_state_get_test_enabled (CoglDepthState *state);

/**
 * cogl_depth_state_set_write_enabled:
 * @state: A #CoglDepthState struct
 * @enable: The enable state you want
 *
 * Enables or disables depth buffer writing according to the value of
 * @enable. Normally when depth testing is enabled and the comparison
 * between a fragment's depth value and the corresponding depth buffer
 * value passes then the fragment's depth is written to the depth
 * buffer unless writing is disabled here.
 *
 * By default depth writing is enabled
 *
 * NB: this won't directly affect the state of the GPU. You have
 * to then set the state on a #CoglPipeline using
 * cogl_pipeline_set_depth_state()
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_depth_state_set_write_enabled (CoglDepthState *state,
                                    CoglBool enable);

/**
 * cogl_depth_state_get_write_enabled:
 * @state: A #CoglDepthState struct
 *
 * Gets the depth writing enable state as set by the corresponding
 * cogl_depth_state_set_write_enabled().
 *
 * Returns: The current depth writing enable state
 * Since: 2.0
 * Stability: Unstable
 */
CoglBool
cogl_depth_state_get_write_enabled (CoglDepthState *state);

/**
 * cogl_depth_state_set_test_function:
 * @state: A #CoglDepthState struct
 * @function: The #CoglDepthTestFunction to set
 *
 * Sets the #CoglDepthTestFunction used to compare the depth value of
 * an incoming fragment against the corresponding value in the current
 * depth buffer.
 *
 * By default the depth test function is %COGL_DEPTH_TEST_FUNCTION_LESS
 *
 * NB: this won't directly affect the state of the GPU. You have
 * to then set the state on a #CoglPipeline using
 * cogl_pipeline_set_depth_state()
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_depth_state_set_test_function (CoglDepthState *state,
                                    CoglDepthTestFunction function);

/**
 * cogl_depth_state_get_test_function:
 * @state: A #CoglDepthState struct
 *
 * Gets the current depth test enable state as previously set via
 * cogl_depth_state_set_test_enabled().
 *
 * Returns: The current depth test enable state.
 * Since: 2.0
 * Stability: Unstable
 */
CoglDepthTestFunction
cogl_depth_state_get_test_function (CoglDepthState *state);

/**
 * cogl_depth_state_set_range:
 * @state: A #CoglDepthState object
 * @near_val: The near component of the desired depth range which will be
 * clamped to the range [0, 1]
 * @far_val: The far component of the desired depth range which will be
 * clamped to the range [0, 1]
 *
 * Sets the range to map depth values in normalized device coordinates
 * to before writing out to a depth buffer.
 *
 * After your geometry has be transformed, clipped and had perspective
 * division applied placing it in normalized device
 * coordinates all depth values between the near and far z clipping
 * planes are in the range -1 to 1. Before writing any depth value to
 * the depth buffer though the value is mapped into the range [0, 1].
 *
 * With this function you can change the range which depth values are
 * mapped too although the range must still lye within the range [0,
 * 1].
 *
 * If your driver does not support this feature (for example you are
 * using GLES 1 drivers) then if you don't use the default range
 * values you will get an error reported when calling
 * cogl_pipeline_set_depth_state (). You can check ahead of time for
 * the %COGL_FEATURE_ID_DEPTH_RANGE feature with
 * cogl_has_feature() to know if this function will succeed.
 *
 * By default normalized device coordinate depth values are mapped to
 * the full range of depth buffer values, [0, 1].
 *
 * NB: this won't directly affect the state of the GPU. You have
 * to then set the state on a #CoglPipeline using
 * cogl_pipeline_set_depth_state().
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_depth_state_set_range (CoglDepthState *state,
                            float near_val,
                            float far_val);

/**
 * cogl_depth_state_get_range:
 * @state: A #CoglDepthState object
 * @near_val: A pointer to store the near component of the depth range
 * @far_val: A pointer to store the far component of the depth range
 *
 * Gets the current range to which normalized depth values are mapped
 * before writing to the depth buffer. This corresponds to the range
 * set with cogl_depth_state_set_range().
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_depth_state_get_range (CoglDepthState *state,
                            float *near_val,
                            float *far_val);

COGL_END_DECLS

#endif /* __COGL_DEPTH_STATE_H__ */
