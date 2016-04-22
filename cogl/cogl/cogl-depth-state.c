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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-depth-state-private.h"
#include "cogl-depth-state.h"

void
cogl_depth_state_init (CoglDepthState *state)
{
  state->magic = COGL_DEPTH_STATE_MAGIC;

  /* The same as the GL defaults */
  state->test_enabled = FALSE;
  state->write_enabled = TRUE;
  state->test_function = COGL_DEPTH_TEST_FUNCTION_LESS;
  state->range_near = 0;
  state->range_far = 1;
}

void
cogl_depth_state_set_test_enabled (CoglDepthState *state,
                                   CoglBool enabled)
{
  _COGL_RETURN_IF_FAIL (state->magic == COGL_DEPTH_STATE_MAGIC);
  state->test_enabled = enabled;
}

CoglBool
cogl_depth_state_get_test_enabled (CoglDepthState *state)
{
  _COGL_RETURN_VAL_IF_FAIL (state->magic == COGL_DEPTH_STATE_MAGIC, FALSE);
  return state->test_enabled;
}

void
cogl_depth_state_set_write_enabled (CoglDepthState *state,
                                    CoglBool enabled)
{
  _COGL_RETURN_IF_FAIL (state->magic == COGL_DEPTH_STATE_MAGIC);
  state->write_enabled = enabled;
}

CoglBool
cogl_depth_state_get_write_enabled (CoglDepthState *state)
{
  _COGL_RETURN_VAL_IF_FAIL (state->magic == COGL_DEPTH_STATE_MAGIC, FALSE);
  return state->write_enabled;
}

void
cogl_depth_state_set_test_function (CoglDepthState *state,
                                    CoglDepthTestFunction function)
{
  _COGL_RETURN_IF_FAIL (state->magic == COGL_DEPTH_STATE_MAGIC);
  state->test_function = function;
}

CoglDepthTestFunction
cogl_depth_state_get_test_function (CoglDepthState *state)
{
  _COGL_RETURN_VAL_IF_FAIL (state->magic == COGL_DEPTH_STATE_MAGIC, FALSE);
  return state->test_function;
}

void
cogl_depth_state_set_range (CoglDepthState *state,
                            float near,
                            float far)
{
  _COGL_RETURN_IF_FAIL (state->magic == COGL_DEPTH_STATE_MAGIC);
  state->range_near = near;
  state->range_far = far;
}

void
cogl_depth_state_get_range (CoglDepthState *state,
                            float *near_out,
                            float *far_out)
{
  _COGL_RETURN_IF_FAIL (state->magic == COGL_DEPTH_STATE_MAGIC);
  *near_out = state->range_near;
  *far_out = state->range_far;
}
