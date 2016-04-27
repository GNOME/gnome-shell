/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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
