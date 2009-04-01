/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __COGL_PROGRAM_H
#define __COGL_PROGRAM_H

#include "cogl-gles2-wrapper.h"
#include "cogl-handle.h"

typedef struct _CoglProgram CoglProgram;

struct _CoglProgram
{
  CoglHandleObject   _parent;

  GSList            *attached_shaders;

  char              *custom_uniform_names[COGL_GLES2_NUM_CUSTOM_UNIFORMS];
};

CoglProgram *_cogl_program_pointer_from_handle (CoglHandle handle);

#endif /* __COGL_PROGRAM_H */
