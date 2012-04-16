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
