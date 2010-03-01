/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_SHADER_H
#define __COGL_SHADER_H

#include "cogl-handle.h"

typedef struct _CoglShader CoglShader;

struct _CoglShader
{
  CoglHandleObject   _parent;
  GLhandleARB        gl_handle;
};

CoglShader *_cogl_shader_pointer_from_handle (CoglHandle handle);

#endif /* __COGL_SHADER_H */
