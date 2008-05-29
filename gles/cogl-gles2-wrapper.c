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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clutter/clutter-fixed.h>
#include <string.h>
#include <math.h>

#include "cogl.h"
#include "cogl-gles2-wrapper.h"
#include "cogl-fixed-vertex-shader.h"
#include "cogl-fixed-fragment-shader.h"
#include "cogl-context.h"

#define _COGL_GET_GLES2_WRAPPER(wvar, retval)			\
  CoglGles2Wrapper *wvar;					\
  {								\
    CoglContext *__ctxvar = _cogl_context_get_default ();	\
    if (__ctxvar == NULL) return retval;			\
    wvar = &__ctxvar->gles2;					\
  }

#define COGL_GLES2_WRAPPER_VERTEX_ATTRIB    0
#define COGL_GLES2_WRAPPER_TEX_COORD_ATTRIB 1
#define COGL_GLES2_WRAPPER_COLOR_ATTRIB     2

static GLuint
cogl_gles2_wrapper_create_shader (GLenum type, const char *source)
{
  GLuint shader;
  GLint source_len = strlen (source);
  GLint status;

  shader = glCreateShader (type);
  glShaderSource (shader, 1, &source, &source_len);
  glCompileShader (shader);

  glGetShaderiv (shader, GL_COMPILE_STATUS, &status);

  if (!status)
    {
      char log[1024];
      GLint len;

      glGetShaderInfoLog (shader, sizeof (log) - 1, &len, log);
      log[len] = '\0';

      g_critical ("%s", log);

      glDeleteShader (shader);

      return 0;
    }

  return shader;
}

void
cogl_gles2_wrapper_init (CoglGles2Wrapper *wrapper)
{
  GLint status;
  GLfixed default_fog_color[4] = { 0, 0, 0, 0 };

  memset (wrapper, 0, sizeof (CoglGles2Wrapper));

  /* Create the shader program */
  wrapper->vertex_shader
    = cogl_gles2_wrapper_create_shader (GL_VERTEX_SHADER,
					cogl_fixed_vertex_shader);

  if (wrapper->vertex_shader == 0)
    return;

  wrapper->fragment_shader
    = cogl_gles2_wrapper_create_shader (GL_FRAGMENT_SHADER,
					cogl_fixed_fragment_shader);

  if (wrapper->fragment_shader == 0)
    {
      glDeleteShader (wrapper->vertex_shader);
      return;
    }

  wrapper->program = glCreateProgram ();
  glAttachShader (wrapper->program, wrapper->fragment_shader);
  glAttachShader (wrapper->program, wrapper->vertex_shader);
  glBindAttribLocation (wrapper->program, COGL_GLES2_WRAPPER_VERTEX_ATTRIB,
			"vertex_attrib");
  glBindAttribLocation (wrapper->program, COGL_GLES2_WRAPPER_TEX_COORD_ATTRIB,
			"tex_coord_attrib");
  glBindAttribLocation (wrapper->program, COGL_GLES2_WRAPPER_COLOR_ATTRIB,
			"color_attrib");
  glLinkProgram (wrapper->program);

  glGetProgramiv (wrapper->program, GL_LINK_STATUS, &status);

  if (!status)
    {
      char log[1024];
      GLint len;

      glGetProgramInfoLog (wrapper->program, sizeof (log) - 1, &len, log);
      log[len] = '\0';

      g_critical ("%s", log);

      glDeleteProgram (wrapper->program);
      glDeleteShader (wrapper->vertex_shader);
      glDeleteShader (wrapper->fragment_shader);

      return;
    }

  glUseProgram (wrapper->program);

  wrapper->mvp_matrix_uniform
    = glGetUniformLocation (wrapper->program, "mvp_matrix");
  wrapper->modelview_matrix_uniform
    = glGetUniformLocation (wrapper->program, "modelview_matrix");
  wrapper->texture_matrix_uniform
    = glGetUniformLocation (wrapper->program, "texture_matrix");
  wrapper->texture_2d_enabled_uniform
    = glGetUniformLocation (wrapper->program, "texture_2d_enabled");
  wrapper->bound_texture_uniform
    = glGetUniformLocation (wrapper->program, "texture_unit");
  wrapper->alpha_only_uniform
    = glGetUniformLocation (wrapper->program, "alpha_only");

  wrapper->fog_enabled_uniform
    = glGetUniformLocation (wrapper->program, "fog_enabled");
  wrapper->fog_mode_uniform
    = glGetUniformLocation (wrapper->program, "fog_mode");
  wrapper->fog_density_uniform
    = glGetUniformLocation (wrapper->program, "fog_density");
  wrapper->fog_start_uniform
    = glGetUniformLocation (wrapper->program, "fog_start");
  wrapper->fog_end_uniform
    = glGetUniformLocation (wrapper->program, "fog_end");
  wrapper->fog_color_uniform
    = glGetUniformLocation (wrapper->program, "fog_color");

  wrapper->alpha_test_enabled_uniform
    = glGetUniformLocation (wrapper->program, "alpha_test_enabled");
  wrapper->alpha_test_func_uniform
    = glGetUniformLocation (wrapper->program, "alpha_test_func");
  wrapper->alpha_test_ref_uniform
    = glGetUniformLocation (wrapper->program, "alpha_test_ref");

  /* Always use the first texture unit */
  glUniform1i (wrapper->bound_texture_uniform, 0);

  /* Initialize the stacks */
  cogl_wrap_glMatrixMode (GL_TEXTURE);
  cogl_wrap_glLoadIdentity ();
  cogl_wrap_glMatrixMode (GL_PROJECTION);
  cogl_wrap_glLoadIdentity ();
  cogl_wrap_glMatrixMode (GL_MODELVIEW);
  cogl_wrap_glLoadIdentity ();

  wrapper->mvp_uptodate = GL_FALSE;

  /* Initialize the fogging options */
  cogl_wrap_glDisable (GL_FOG);
  cogl_wrap_glFogx (GL_FOG_MODE, GL_LINEAR);
  cogl_wrap_glFogx (GL_FOG_DENSITY, CFX_ONE);
  cogl_wrap_glFogx (GL_FOG_START, 0);
  cogl_wrap_glFogx (GL_FOG_END, 1);
  cogl_wrap_glFogxv (GL_FOG_COLOR, default_fog_color);

  /* Initialize alpha testing */
  cogl_wrap_glDisable (GL_ALPHA_TEST);
  cogl_wrap_glAlphaFunc (GL_ALWAYS, 0.0f);
}

void
cogl_gles2_wrapper_deinit (CoglGles2Wrapper *wrapper)
{
  if (wrapper->program)
    {
      glDeleteProgram (wrapper->program);
      wrapper->program = 0;
    }
  if (wrapper->vertex_shader)
    {
      glDeleteShader (wrapper->vertex_shader);
      wrapper->vertex_shader = 0;
    }
  if (wrapper->fragment_shader)
    {
      glDeleteShader (wrapper->fragment_shader);
      wrapper->fragment_shader = 0;
    }
}

static void
cogl_gles2_wrapper_update_matrix (CoglGles2Wrapper *wrapper)
{
  const float *matrix;

  switch (wrapper->matrix_mode)
    {
    default:
    case GL_MODELVIEW:
    case GL_PROJECTION:
      /* Queue a recalculation of the combined modelview and
	 projection matrix at the next draw */
      wrapper->mvp_uptodate = GL_FALSE;
      break;

    case GL_TEXTURE:
      matrix = wrapper->texture_stack + wrapper->texture_stack_pos * 16;
      glUniformMatrix4fv (wrapper->texture_matrix_uniform, 1, GL_FALSE, matrix);
      break;
    }

}

void
cogl_wrap_glClearColorx (GLclampx r, GLclampx g, GLclampx b, GLclampx a)
{
  glClearColor (CLUTTER_FIXED_TO_FLOAT (r),
		CLUTTER_FIXED_TO_FLOAT (g),
		CLUTTER_FIXED_TO_FLOAT (b),
		CLUTTER_FIXED_TO_FLOAT (a));
}

void
cogl_wrap_glPushMatrix ()
{
  const float *src;
  float *dst;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  /* Get a pointer to the old and new matrix position and increment
     the stack pointer */
  switch (w->matrix_mode)
    {
    default:
    case GL_MODELVIEW:
      src = w->modelview_stack + w->modelview_stack_pos * 16;
      w->modelview_stack_pos = (w->modelview_stack_pos + 1)
	& (COGL_GLES2_MODELVIEW_STACK_SIZE - 1);
      dst = w->modelview_stack + w->modelview_stack_pos * 16;
      break;

    case GL_PROJECTION:
      src = w->projection_stack + w->projection_stack_pos * 16;
      w->projection_stack_pos = (w->projection_stack_pos + 1)
	& (COGL_GLES2_PROJECTION_STACK_SIZE - 1);
      dst = w->projection_stack + w->projection_stack_pos * 16;
      break;

    case GL_TEXTURE:
      src = w->texture_stack + w->texture_stack_pos * 16;
      w->texture_stack_pos = (w->texture_stack_pos + 1)
	& (COGL_GLES2_TEXTURE_STACK_SIZE - 1);
      dst = w->texture_stack + w->texture_stack_pos * 16;
      break;
    }

  /* Copy the old matrix to the new position */
  memcpy (dst, src, sizeof (float) * 16);
}

void
cogl_wrap_glPopMatrix ()
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  /* Decrement the stack pointer */
  switch (w->matrix_mode)
    {
    default:
    case GL_MODELVIEW:
      w->modelview_stack_pos = (w->modelview_stack_pos - 1)
	& (COGL_GLES2_MODELVIEW_STACK_SIZE - 1);
      break;

    case GL_PROJECTION:
      w->projection_stack_pos = (w->projection_stack_pos - 1)
	& (COGL_GLES2_PROJECTION_STACK_SIZE - 1);
      break;

    case GL_TEXTURE:
      w->texture_stack_pos = (w->texture_stack_pos - 1)
	& (COGL_GLES2_TEXTURE_STACK_SIZE - 1);
      break;
    }

  /* Update the matrix in the program object */
  cogl_gles2_wrapper_update_matrix (w);
}

void
cogl_wrap_glMatrixMode (GLenum mode)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  w->matrix_mode = mode;
}

static float *
cogl_gles2_get_matrix_stack_top (CoglGles2Wrapper *wrapper)
{
  switch (wrapper->matrix_mode)
    {
    default:
    case GL_MODELVIEW:
      return wrapper->modelview_stack + wrapper->modelview_stack_pos * 16;

    case GL_PROJECTION:
      return wrapper->projection_stack + wrapper->projection_stack_pos * 16;

    case GL_TEXTURE:
      return wrapper->texture_stack + wrapper->texture_stack_pos * 16;
    }
}

void
cogl_wrap_glLoadIdentity ()
{
  float *matrix;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  matrix = cogl_gles2_get_matrix_stack_top (w);
  memset (matrix, 0, sizeof (float) * 16);
  matrix[0] = 1.0f;
  matrix[5] = 1.0f;
  matrix[10] = 1.0f;
  matrix[15] = 1.0f;

  cogl_gles2_wrapper_update_matrix (w);
}

static void
cogl_gles2_wrapper_mult_matrix (float *dst, const float *a, const float *b)
{
  int i, j, k;

  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      {
	float sum = 0.0f;
	for (k = 0; k < 4; k++)
	  sum += a[k * 4 + j] * b[i * 4 + k];
	dst[i * 4 + j] = sum;
      }
}

static void
cogl_wrap_glMultMatrix (const float *m)
{
  float new_matrix[16];
  float *old_matrix;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  old_matrix = cogl_gles2_get_matrix_stack_top (w);

  cogl_gles2_wrapper_mult_matrix (new_matrix, old_matrix, m);

  memcpy (old_matrix, new_matrix, sizeof (float) * 16);

  cogl_gles2_wrapper_update_matrix (w);
}

void
cogl_wrap_glMultMatrixx (const GLfixed *m)
{
  float new_matrix[16];
  int i;

  for (i = 0; i < 16; i++)
    new_matrix[i] = CLUTTER_FIXED_TO_FLOAT (m[i]);

  cogl_wrap_glMultMatrix (new_matrix);
}

void
cogl_wrap_glScalex (GLfixed x, GLfixed y, GLfixed z)
{
  float matrix[16];

  memset (matrix, 0, sizeof (matrix));
  matrix[0] = CLUTTER_FIXED_TO_FLOAT (x);
  matrix[5] = CLUTTER_FIXED_TO_FLOAT (y);
  matrix[10] = CLUTTER_FIXED_TO_FLOAT (z);
  matrix[15] = 1.0f;

  cogl_wrap_glMultMatrix (matrix);
}

void
cogl_wrap_glTranslatex (GLfixed x, GLfixed y, GLfixed z)
{
  float matrix[16];

  memset (matrix, 0, sizeof (matrix));
  matrix[0] = 1.0f;
  matrix[5] = 1.0f;
  matrix[10] = 1.0f;
  matrix[12] = CLUTTER_FIXED_TO_FLOAT (x);
  matrix[13] = CLUTTER_FIXED_TO_FLOAT (y);
  matrix[14] = CLUTTER_FIXED_TO_FLOAT (z);
  matrix[15] = 1.0f;

  cogl_wrap_glMultMatrix (matrix);
}

void
cogl_wrap_glRotatex (GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
  float matrix[16];
  float xf = CLUTTER_FIXED_TO_FLOAT (x);
  float yf = CLUTTER_FIXED_TO_FLOAT (y);
  float zf = CLUTTER_FIXED_TO_FLOAT (z);
  float anglef = CLUTTER_FIXED_TO_FLOAT (angle) * G_PI / 180.0f;
  float c = cosf (anglef);
  float s = sinf (anglef);
  
  matrix[0]  = xf * xf * (1.0f - c) + c;
  matrix[1]  = yf * xf * (1.0f - c) + zf * s;
  matrix[2]  = xf * zf * (1.0f - c) - yf * s;
  matrix[3]  = 0.0f;

  matrix[4]  = xf * yf * (1.0f - c) - zf * s;
  matrix[5]  = yf * yf * (1.0f - c) + c;
  matrix[6]  = yf * zf * (1.0f - c) + xf * s;
  matrix[7]  = 0.0f;

  matrix[8]  = xf * zf * (1.0f - c) + yf * s;
  matrix[9]  = yf * zf * (1.0f - c) - xf * s;
  matrix[10] = zf * zf * (1.0f - c) + c;
  matrix[11] = 0.0f;

  matrix[12] = 0.0f;
  matrix[13] = 0.0f;
  matrix[14] = 0.0f;
  matrix[15] = 1.0f;

  cogl_wrap_glMultMatrix (matrix);
}

void
cogl_wrap_glOrthox (GLfixed left, GLfixed right, GLfixed bottom, GLfixed top,
		    GLfixed near, GLfixed far)
{
  float matrix[16];
  float xrange = CLUTTER_FIXED_TO_FLOAT (right - left);
  float yrange = CLUTTER_FIXED_TO_FLOAT (top - bottom);
  float zrange = CLUTTER_FIXED_TO_FLOAT (far - near);

  memset (matrix, 0, sizeof (matrix));
  matrix[0] = 2.0f / xrange;
  matrix[5] = 2.0f / yrange;
  matrix[10] = 2.0f / zrange;
  matrix[12] = CLUTTER_FIXED_TO_FLOAT (right + left) / xrange;
  matrix[13] = CLUTTER_FIXED_TO_FLOAT (top + bottom) / yrange;
  matrix[14] = CLUTTER_FIXED_TO_FLOAT (far + near) / zrange;
  matrix[15] = 1.0f;

  cogl_wrap_glMultMatrix (matrix);
}

void
cogl_wrap_glVertexPointer (GLint size, GLenum type, GLsizei stride,
			   const GLvoid *pointer)
{
  glVertexAttribPointer (COGL_GLES2_WRAPPER_VERTEX_ATTRIB, size, type,
			 GL_FALSE, stride, pointer);
}

void
cogl_wrap_glTexCoordPointer (GLint size, GLenum type, GLsizei stride,
			     const GLvoid *pointer)
{
  glVertexAttribPointer (COGL_GLES2_WRAPPER_TEX_COORD_ATTRIB, size, type,
			 GL_FALSE, stride, pointer);
}

void
cogl_wrap_glColorPointer (GLint size, GLenum type, GLsizei stride,
			  const GLvoid *pointer)
{
  glVertexAttribPointer (COGL_GLES2_WRAPPER_COLOR_ATTRIB, size, type,
			 GL_FALSE, stride, pointer);
}

void
cogl_wrap_glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  /* Make sure the modelview+projection matrix is up to date */
  if (!w->mvp_uptodate)
    {
      float mvp_matrix[16];
      const float *modelview_matrix = w->modelview_stack
	+ w->modelview_stack_pos * 16;

      cogl_gles2_wrapper_mult_matrix (mvp_matrix,
				      w->projection_stack
				      + w->projection_stack_pos * 16,
				      modelview_matrix);

      glUniformMatrix4fv (w->mvp_matrix_uniform, 1, GL_FALSE, mvp_matrix);
      glUniformMatrix4fv (w->modelview_matrix_uniform, 1, GL_FALSE,
			  modelview_matrix);

      w->mvp_uptodate = GL_TRUE;
    }

  glDrawArrays (mode, first, count);
}

void
cogl_gles2_wrapper_bind_texture (GLenum target, GLuint texture,
				 GLenum internal_format)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  glBindTexture (target, texture);

  /* We need to keep track of whether the texture is alpha-only
     because the emulation of GL_MODULATE needs to work differently in
     that case */
  glUniform1i (w->alpha_only_uniform,
	       internal_format == GL_ALPHA ? GL_TRUE : GL_FALSE);
}

void
cogl_wrap_glTexEnvx (GLenum target, GLenum pname, GLfixed param)
{
  /* This function is only used to set the texture mode once to
     GL_MODULATE. The shader is hard-coded to modulate the texture so
     nothing needs to be done here. */
}

void
cogl_wrap_glEnable (GLenum cap)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (cap)
    {
    case GL_TEXTURE_2D:
      glUniform1i (w->texture_2d_enabled_uniform, GL_TRUE);
      break;

    case GL_FOG:
      glUniform1i (w->fog_enabled_uniform, GL_TRUE);
      break;

    case GL_ALPHA_TEST:
      glUniform1i (w->alpha_test_enabled_uniform, GL_TRUE);
      break;

    default:
      glEnable (cap);
    }
}

void
cogl_wrap_glDisable (GLenum cap)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (cap)
    {
    case GL_TEXTURE_2D:
      glUniform1i (w->texture_2d_enabled_uniform, GL_FALSE);
      break;

    case GL_FOG:
      glUniform1i (w->fog_enabled_uniform, GL_FALSE);
      break;

    case GL_ALPHA_TEST:
      glUniform1i (w->alpha_test_enabled_uniform, GL_FALSE);
      break;

    default:
      glDisable (cap);
    }
}

void
cogl_wrap_glEnableClientState (GLenum array)
{
  switch (array)
    {
    case GL_VERTEX_ARRAY:
      glEnableVertexAttribArray (COGL_GLES2_WRAPPER_VERTEX_ATTRIB);
      break;
    case GL_TEXTURE_COORD_ARRAY:
      glEnableVertexAttribArray (COGL_GLES2_WRAPPER_TEX_COORD_ATTRIB);
      break;
    case GL_COLOR_ARRAY:
      glEnableVertexAttribArray (COGL_GLES2_WRAPPER_COLOR_ATTRIB);
      break;
    }
}

void
cogl_wrap_glDisableClientState (GLenum array)
{
  switch (array)
    {
    case GL_VERTEX_ARRAY:
      glDisableVertexAttribArray (COGL_GLES2_WRAPPER_VERTEX_ATTRIB);
      break;
    case GL_TEXTURE_COORD_ARRAY:
      glDisableVertexAttribArray (COGL_GLES2_WRAPPER_TEX_COORD_ATTRIB);
      break;
    case GL_COLOR_ARRAY:
      glDisableVertexAttribArray (COGL_GLES2_WRAPPER_COLOR_ATTRIB);
      break;
    }
}

void
cogl_wrap_glAlphaFunc (GLenum func, GLclampf ref)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  if (ref < 0.0f)
    ref = 0.0f;
  else if (ref > 1.0f)
    ref = 1.0f;

  glUniform1i (w->alpha_test_func_uniform, func);
  glUniform1f (w->alpha_test_ref_uniform, ref);
}

void
cogl_wrap_glColor4x (GLclampx r, GLclampx g, GLclampx b, GLclampx a)
{
  glVertexAttrib4f (COGL_GLES2_WRAPPER_COLOR_ATTRIB,
		    CLUTTER_FIXED_TO_FLOAT (r),
		    CLUTTER_FIXED_TO_FLOAT (g),
		    CLUTTER_FIXED_TO_FLOAT (b),
		    CLUTTER_FIXED_TO_FLOAT (a));
}

void
cogl_wrap_glClipPlanex (GLenum plane, GLfixed *equation)
{
  /* FIXME */
}

static void
cogl_gles2_float_array_to_fixed (int size, const GLfloat *floats,
				 GLfixed *fixeds)
{
  while (size-- > 0)
    *(fixeds++) = CLUTTER_FLOAT_TO_FIXED (*(floats++));
}

void
cogl_wrap_glGetIntegerv (GLenum pname, GLint *params)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (pname)
    {
    case GL_MAX_CLIP_PLANES:
      *params = 0;
      break;

    default:
      glGetIntegerv (pname, params);
      break;
    }
}

void
cogl_wrap_glGetFixedv (GLenum pname, GLfixed *params)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (pname)
    {
    case GL_MODELVIEW_MATRIX:
      cogl_gles2_float_array_to_fixed (16, w->modelview_stack
				       + w->modelview_stack_pos * 16,
				       params);
      break;

    case GL_PROJECTION_MATRIX:
      cogl_gles2_float_array_to_fixed (16, w->projection_stack
				       + w->projection_stack_pos * 16,
				       params);
      break;

    case GL_VIEWPORT:
      {
	GLfloat v[4];

	glGetFloatv (GL_VIEWPORT, v);
	cogl_gles2_float_array_to_fixed (4, v, params);
      }
      break;
    }
}

void
cogl_wrap_glFogx (GLenum pname, GLfixed param)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (pname)
    {
    case GL_FOG_MODE:
      glUniform1i (w->fog_mode_uniform, param);
      break;
      
    case GL_FOG_DENSITY:
      glUniform1f (w->fog_density_uniform, CLUTTER_FIXED_TO_FLOAT (param));
      break;

    case GL_FOG_START:
      glUniform1f (w->fog_start_uniform, CLUTTER_FIXED_TO_FLOAT (param));
      break;

    case GL_FOG_END:
      glUniform1f (w->fog_end_uniform, CLUTTER_FIXED_TO_FLOAT (param));
      break;
    }
}

void
cogl_wrap_glFogxv (GLenum pname, const GLfixed *params)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  if (pname == GL_FOG_COLOR)
    glUniform4f (w->fog_color_uniform,
		 CLUTTER_FIXED_TO_FLOAT (params[0]),
		 CLUTTER_FIXED_TO_FLOAT (params[1]),
		 CLUTTER_FIXED_TO_FLOAT (params[2]),
		 CLUTTER_FIXED_TO_FLOAT (params[3]));
}

void
cogl_wrap_glTexParameteri (GLenum target, GLenum pname, GLfloat param)
{
  if (pname != GL_GENERATE_MIPMAP)
    glTexParameteri (target, pname, param);
}
