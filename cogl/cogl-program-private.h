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

#ifndef __COGL_PROGRAM_H
#define __COGL_PROGRAM_H

#include "cogl-object-private.h"
#include "cogl-shader-private.h"

typedef struct _CoglProgram CoglProgram;

struct _CoglProgram
{
  CoglHandleObject _parent;

  GSList *attached_shaders;

  GArray *custom_uniforms;

  /* An age counter that changes whenever the list of shaders is modified */
  unsigned int age;
};

typedef struct _CoglProgramUniform CoglProgramUniform;

struct _CoglProgramUniform
{
  char *name;
  CoglBoxedValue value;
  /* The cached GL location for this uniform. This is only valid
     between calls to _cogl_program_dirty_all_uniforms */
  GLint location;
  /* Whether we have a location yet */
  unsigned int location_valid : 1;
  /* Whether the uniform value has changed since the last time the
     uniforms were flushed */
  unsigned int dirty : 1;
};

/* Internal function to flush the custom uniforms for the given use
   program. This assumes the target GL program is already bound. The
   gl_program still needs to be passed so that CoglProgram can query
   the uniform locations. gl_program_changed should be set to TRUE if
   we are flushing the uniforms against a different GL program from
   the last time it was flushed. This will cause it to requery all of
   the locations and assume that all uniforms are dirty */
void
_cogl_program_flush_uniforms (CoglProgram *program,
                              GLuint gl_program,
                              CoglBool gl_program_changed);

CoglShaderLanguage
_cogl_program_get_language (CoglHandle handle);

CoglBool
_cogl_program_has_fragment_shader (CoglHandle handle);

CoglBool
_cogl_program_has_vertex_shader (CoglHandle handle);

#endif /* __COGL_PROGRAM_H */
