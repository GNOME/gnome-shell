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

#ifndef __COGL_SHADER_H
#define __COGL_SHADER_H

#include "cogl-object-private.h"
#include "cogl-shader.h"
#include "cogl-gl-header.h"
#include "cogl-pipeline.h"

typedef struct _CoglShader CoglShader;

typedef enum
{
  COGL_SHADER_LANGUAGE_GLSL,
  COGL_SHADER_LANGUAGE_ARBFP
} CoglShaderLanguage;

struct _CoglShader
{
  CoglHandleObject _parent;
  GLuint gl_handle;
  CoglPipeline *compilation_pipeline;
  CoglShaderType type;
  CoglShaderLanguage language;
  char *source;
};

void
_cogl_shader_compile_real (CoglHandle handle,
                           CoglPipeline *pipeline);

CoglShaderLanguage
_cogl_program_get_language (CoglHandle handle);

void
_cogl_shader_set_source_with_boilerplate (GLuint shader_gl_handle,
                                          GLenum shader_gl_type,
                                          int n_tex_coord_attribs,
                                          GLsizei count_in,
                                          const char **strings_in,
                                          const GLint *lengths_in);

#endif /* __COGL_SHADER_H */
