/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef __COGL_PIPELINE_VERTEND_GLSL_PRIVATE_H
#define __COGL_PIPELINE_VERTEND_GLSL_PRIVATE_H

#include "cogl-pipeline-private.h"

extern const CoglPipelineVertend _cogl_pipeline_glsl_vertend;

GLuint
_cogl_pipeline_vertend_glsl_get_shader (CoglPipeline *pipeline);

#endif /* __COGL_PIPELINE_VERTEND_GLSL_PRIVATE_H */

