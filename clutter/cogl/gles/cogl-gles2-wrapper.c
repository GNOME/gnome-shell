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
#include "cogl-shader-private.h"
#include "cogl-program.h"

#define _COGL_GET_GLES2_WRAPPER(wvar, retval)			\
  CoglGles2Wrapper *wvar;					\
  {								\
    CoglContext *__ctxvar = _cogl_context_get_default ();	\
    if (__ctxvar == NULL) return retval;			\
    wvar = &__ctxvar->gles2;					\
  }

#define _COGL_GLES2_CHANGE_SETTING(w, var, val)	\
  do						\
    if ((w)->settings.var != (val))		\
      {						\
	(w)->settings.var = (val);		\
	(w)->settings_dirty = TRUE;		\
      }						\
  while (0)

#define _COGL_GLES2_CHANGE_UNIFORM(w, flag, var, val)		\
  do								\
    if ((w)->var != (val))					\
      {								\
	(w)->var = (val);					\
	(w)->dirty_uniforms |= COGL_GLES2_DIRTY_ ## flag;	\
      }								\
  while (0)

#define COGL_GLES2_WRAPPER_VERTEX_ATTRIB    0
#define COGL_GLES2_WRAPPER_TEX_COORD_ATTRIB 1
#define COGL_GLES2_WRAPPER_COLOR_ATTRIB     2
#define COGL_GLES2_WRAPPER_NORMAL_ATTRIB    3

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
  GLfixed default_fog_color[4] = { 0, 0, 0, 0 };

  memset (wrapper, 0, sizeof (CoglGles2Wrapper));

  /* Initialize the stacks */
  cogl_wrap_glMatrixMode (GL_TEXTURE);
  cogl_wrap_glLoadIdentity ();
  cogl_wrap_glMatrixMode (GL_PROJECTION);
  cogl_wrap_glLoadIdentity ();
  cogl_wrap_glMatrixMode (GL_MODELVIEW);
  cogl_wrap_glLoadIdentity ();

  /* Initialize the fogging options */
  cogl_wrap_glDisable (GL_FOG);
  cogl_wrap_glFogx (GL_FOG_MODE, GL_LINEAR);
  cogl_wrap_glFogx (GL_FOG_DENSITY, COGL_FIXED_1);
  cogl_wrap_glFogx (GL_FOG_START, 0);
  cogl_wrap_glFogx (GL_FOG_END, 1);
  cogl_wrap_glFogxv (GL_FOG_COLOR, default_fog_color);

  /* Initialize alpha testing */
  cogl_wrap_glDisable (GL_ALPHA_TEST);
  cogl_wrap_glAlphaFunc (GL_ALWAYS, 0.0f);
}

static gboolean
cogl_gles2_settings_equal (const CoglGles2WrapperSettings *a,
			   const CoglGles2WrapperSettings *b,
			   gboolean vertex_tests,
			   gboolean fragment_tests)
{
  if (fragment_tests)
    {
      if (a->texture_2d_enabled != b->texture_2d_enabled)
	return FALSE;
      
      if (a->texture_2d_enabled && a->alpha_only != b->alpha_only)
	return FALSE;

      if (a->alpha_test_enabled != b->alpha_test_enabled)
	return FALSE;
      if (a->alpha_test_enabled && a->alpha_test_func != b->alpha_test_func)
	return FALSE;
    }

  if (a->fog_enabled != b->fog_enabled)
    return FALSE;

  if (vertex_tests && a->fog_enabled && a->fog_mode != b->fog_mode)
    return FALSE;

  return TRUE;
}

static CoglGles2WrapperShader *
cogl_gles2_get_vertex_shader (const CoglGles2WrapperSettings *settings)
{
  GString *shader_source;
  GLuint shader_obj;
  CoglGles2WrapperShader *shader;
  GSList *node;

  _COGL_GET_GLES2_WRAPPER (w, NULL);

  /* Check if we already have a vertex shader for these settings */
  for (node = w->compiled_vertex_shaders; node; node = node->next)
    if (cogl_gles2_settings_equal (settings,
				   &((CoglGles2WrapperShader *)
				     node->data)->settings,
				   TRUE, FALSE))
      return (CoglGles2WrapperShader *) node->data;

  /* Otherwise create a new shader */
  shader_source = g_string_new (cogl_fixed_vertex_shader_start);

  if (settings->fog_enabled)
    {
      g_string_append (shader_source, cogl_fixed_vertex_shader_fog_start);

      switch (settings->fog_mode)
	{
	case GL_EXP:
	  g_string_append (shader_source, cogl_fixed_vertex_shader_fog_exp);
	  break;

	case GL_EXP2:
	  g_string_append (shader_source, cogl_fixed_vertex_shader_fog_exp2);
	  break;

	default:
	  g_string_append (shader_source, cogl_fixed_vertex_shader_fog_linear);
	  break;
	}

      g_string_append (shader_source, cogl_fixed_vertex_shader_fog_end);
    }

  g_string_append (shader_source, cogl_fixed_vertex_shader_end);

  shader_obj = cogl_gles2_wrapper_create_shader (GL_VERTEX_SHADER,
						 shader_source->str);

  g_string_free (shader_source, TRUE);
  
  if (shader_obj == 0)
    return NULL;
  
  shader = g_slice_new (CoglGles2WrapperShader);
  shader->shader = shader_obj;
  shader->settings = *settings;

  w->compiled_vertex_shaders = g_slist_prepend (w->compiled_vertex_shaders,
						shader);

  return shader;
}

static CoglGles2WrapperShader *
cogl_gles2_get_fragment_shader (const CoglGles2WrapperSettings *settings)
{
  GString *shader_source;
  GLuint shader_obj;
  CoglGles2WrapperShader *shader;
  GSList *node;

  _COGL_GET_GLES2_WRAPPER (w, NULL);

  /* Check if we already have a fragment shader for these settings */
  for (node = w->compiled_fragment_shaders; node; node = node->next)
    if (cogl_gles2_settings_equal (settings,
				   &((CoglGles2WrapperShader *)
				     node->data)->settings,
				   FALSE, TRUE))
      return (CoglGles2WrapperShader *) node->data;

  /* Otherwise create a new shader */
  shader_source = g_string_new (cogl_fixed_fragment_shader_start);
  if (settings->texture_2d_enabled)
    {
      if (settings->alpha_only)
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_texture_alpha_only);
      else
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_texture);
    }
  else
    g_string_append (shader_source, cogl_fixed_fragment_shader_solid_color);

  if (settings->fog_enabled)
    g_string_append (shader_source, cogl_fixed_fragment_shader_fog);

  if (settings->alpha_test_enabled)
    switch (settings->alpha_test_func)
      {
      case GL_NEVER:
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_alpha_never);
	break;
      case GL_LESS:
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_alpha_less);
	break;
      case GL_EQUAL:
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_alpha_equal);
	break;
      case GL_LEQUAL:
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_alpha_lequal);
	break;
      case GL_GREATER:
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_alpha_greater);
	break;
      case GL_NOTEQUAL:
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_alpha_notequal);
	break;
      case GL_GEQUAL:
	g_string_append (shader_source,
			 cogl_fixed_fragment_shader_alpha_gequal);
      }

  g_string_append (shader_source, cogl_fixed_fragment_shader_end);

  shader_obj = cogl_gles2_wrapper_create_shader (GL_FRAGMENT_SHADER,
						 shader_source->str);

  g_string_free (shader_source, TRUE);
  
  if (shader_obj == 0)
    return NULL;
  
  shader = g_slice_new (CoglGles2WrapperShader);
  shader->shader = shader_obj;
  shader->settings = *settings;

  w->compiled_fragment_shaders = g_slist_prepend (w->compiled_fragment_shaders,
						  shader);

  return shader;
}

static CoglGles2WrapperProgram *
cogl_gles2_wrapper_get_program (const CoglGles2WrapperSettings *settings)
{
  GSList *node;
  CoglGles2WrapperProgram *program;
  CoglGles2WrapperShader *vertex_shader, *fragment_shader;
  GLint status;
  gboolean custom_vertex_shader = FALSE, custom_fragment_shader = FALSE;
  CoglProgram *user_program = NULL;
  int i;

  _COGL_GET_GLES2_WRAPPER (w, NULL);

  /* Check if we've already got a program for these settings */
  for (node = w->compiled_programs; node; node = node->next)
    {
      program = (CoglGles2WrapperProgram *) node->data;

      if (cogl_gles2_settings_equal (settings, &program->settings, TRUE, TRUE)
	  && program->settings.user_program == settings->user_program)
	return (CoglGles2WrapperProgram *) node->data;
    }

  /* Otherwise create a new program */

  /* Check whether the currently used custom program has vertex and
     fragment shaders */
  if (w->settings.user_program != COGL_INVALID_HANDLE)
    {
      user_program
	= _cogl_program_pointer_from_handle (w->settings.user_program);

      for (node = user_program->attached_shaders; node; node = node->next)
	{
	  CoglShader *shader
	    = _cogl_shader_pointer_from_handle ((CoglHandle) node->data);

	  if (shader->type == CGL_VERTEX_SHADER)
	    custom_vertex_shader = TRUE;
	  else if (shader->type == CGL_FRAGMENT_SHADER)
	    custom_fragment_shader = TRUE;
	}
    }

  /* Get or create the fixed functionality shaders for these settings
     if there is no custom replacement */
  if (!custom_vertex_shader)
    {
      vertex_shader = cogl_gles2_get_vertex_shader (settings);
      if (vertex_shader == NULL)
	return NULL;
    }
  if (!custom_fragment_shader)
    {
      fragment_shader = cogl_gles2_get_fragment_shader (settings);
      if (fragment_shader == NULL)
	return NULL;
    }

  program = g_slice_new (CoglGles2WrapperProgram);

  program->program = glCreateProgram ();
  if (!custom_vertex_shader)
    glAttachShader (program->program, vertex_shader->shader);
  if (!custom_fragment_shader)
    glAttachShader (program->program, fragment_shader->shader);
  if (user_program)
    for (node = user_program->attached_shaders; node; node = node->next)
      {
	CoglShader *shader
	  = _cogl_shader_pointer_from_handle ((CoglHandle) node->data);
	glAttachShader (program->program, shader->gl_handle);
      }
  cogl_gles2_wrapper_bind_attributes (program->program);
  glLinkProgram (program->program);

  glGetProgramiv (program->program, GL_LINK_STATUS, &status);

  if (!status)
    {
      char log[1024];
      GLint len;

      glGetProgramInfoLog (program->program, sizeof (log) - 1, &len, log);
      log[len] = '\0';

      g_critical ("%s", log);

      glDeleteProgram (program->program);
      g_slice_free (CoglGles2WrapperProgram, program);

      return NULL;
    }

  program->settings = *settings;
      
  cogl_gles2_wrapper_get_uniforms (program->program, &program->uniforms);

  /* We haven't tried to get a location for any of the custom uniforms
     yet */
  for (i = 0; i < COGL_GLES2_NUM_CUSTOM_UNIFORMS; i++)
    program->custom_uniforms[i] = COGL_GLES2_UNBOUND_CUSTOM_UNIFORM;
      
  w->compiled_programs = g_slist_append (w->compiled_programs, program);
      
  return program;
}

void
cogl_gles2_wrapper_bind_attributes (GLuint program)
{
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_VERTEX_ATTRIB,
			"vertex_attrib");
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_TEX_COORD_ATTRIB,
			"tex_coord_attrib");
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_COLOR_ATTRIB,
			"color_attrib");
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_NORMAL_ATTRIB,
			"normal_attrib");
}

void
cogl_gles2_wrapper_get_uniforms (GLuint program,
				 CoglGles2WrapperUniforms *uniforms)
{
  uniforms->mvp_matrix_uniform
    = glGetUniformLocation (program, "mvp_matrix");
  uniforms->modelview_matrix_uniform
    = glGetUniformLocation (program, "modelview_matrix");
  uniforms->texture_matrix_uniform
    = glGetUniformLocation (program, "texture_matrix");
  uniforms->bound_texture_uniform
    = glGetUniformLocation (program, "texture_unit");

  uniforms->fog_density_uniform
    = glGetUniformLocation (program, "fog_density");
  uniforms->fog_start_uniform
    = glGetUniformLocation (program, "fog_start");
  uniforms->fog_end_uniform
    = glGetUniformLocation (program, "fog_end");
  uniforms->fog_color_uniform
    = glGetUniformLocation (program, "fog_color");

  uniforms->alpha_test_ref_uniform
    = glGetUniformLocation (program, "alpha_test_ref");

  uniforms->texture_unit_uniform
    = glGetUniformLocation (program, "texture_unit");
}

void
cogl_gles2_wrapper_deinit (CoglGles2Wrapper *wrapper)
{
  GSList *node, *next;

  for (node = wrapper->compiled_programs; node; node = next)
    {
      next = node->next;
      glDeleteProgram (((CoglGles2WrapperProgram *) node->data)->program);
      g_slist_free1 (node);
    }
  wrapper->compiled_programs = NULL;

  for (node = wrapper->compiled_vertex_shaders; node; node = next)
    {
      next = node->next;
      glDeleteShader (((CoglGles2WrapperShader *) node->data)->shader);
      g_slist_free1 (node);
    }
  wrapper->compiled_vertex_shaders = NULL;

  for (node = wrapper->compiled_fragment_shaders; node; node = next)
    {
      next = node->next;
      glDeleteShader (((CoglGles2WrapperShader *) node->data)->shader);
      g_slist_free1 (node);
    }
  wrapper->compiled_fragment_shaders = NULL;
}

void
cogl_gles2_wrapper_update_matrix (CoglGles2Wrapper *wrapper, GLenum matrix_num)
{
  switch (matrix_num)
    {
    default:
    case GL_MODELVIEW:
      wrapper->dirty_uniforms |= COGL_GLES2_DIRTY_MVP_MATRIX
	| COGL_GLES2_DIRTY_MODELVIEW_MATRIX;
      break;

    case GL_PROJECTION:
      wrapper->dirty_uniforms |= COGL_GLES2_DIRTY_MVP_MATRIX;
      break;

    case GL_TEXTURE:
      wrapper->dirty_uniforms |= COGL_GLES2_DIRTY_TEXTURE_MATRIX;
      break;
    }
}

void
cogl_wrap_glClearColorx (GLclampx r, GLclampx g, GLclampx b, GLclampx a)
{
  glClearColor (COGL_FIXED_TO_FLOAT (r),
		COGL_FIXED_TO_FLOAT (g),
		COGL_FIXED_TO_FLOAT (b),
		COGL_FIXED_TO_FLOAT (a));
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
  cogl_gles2_wrapper_update_matrix (w, w->matrix_mode);
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

  cogl_gles2_wrapper_update_matrix (w, w->matrix_mode);
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

  cogl_gles2_wrapper_update_matrix (w, w->matrix_mode);
}

void
cogl_wrap_glMultMatrixx (const GLfixed *m)
{
  float new_matrix[16];
  int i;

  for (i = 0; i < 16; i++)
    new_matrix[i] = COGL_FIXED_TO_FLOAT (m[i]);

  cogl_wrap_glMultMatrix (new_matrix);
}

void
cogl_wrap_glFrustumx (GLfixed left, GLfixed right,
		      GLfixed bottom, GLfixed top,
		      GLfixed z_near, GLfixed z_far)
{
  float matrix[16];
  float two_near = COGL_FIXED_TO_FLOAT (2 * z_near);

  memset (matrix, 0, sizeof (matrix));

  matrix[0] = two_near / COGL_FIXED_TO_FLOAT (right - left);
  matrix[5] = two_near / COGL_FIXED_TO_FLOAT (top - bottom);
  matrix[8] = COGL_FIXED_TO_FLOAT (right + left)
    / COGL_FIXED_TO_FLOAT (right - left);
  matrix[9] = COGL_FIXED_TO_FLOAT (top + bottom)
    / COGL_FIXED_TO_FLOAT (top - bottom);
  matrix[10] = -COGL_FIXED_TO_FLOAT (z_far + z_near)
    / COGL_FIXED_TO_FLOAT (z_far - z_near);
  matrix[11] = -1.0f;
  matrix[14] = -two_near * COGL_FIXED_TO_FLOAT (z_far)
    / COGL_FIXED_TO_FLOAT (z_far - z_near);

  cogl_wrap_glMultMatrix (matrix);
}

void
cogl_wrap_glScalex (GLfixed x, GLfixed y, GLfixed z)
{
  float matrix[16];

  memset (matrix, 0, sizeof (matrix));
  matrix[0] = COGL_FIXED_TO_FLOAT (x);
  matrix[5] = COGL_FIXED_TO_FLOAT (y);
  matrix[10] = COGL_FIXED_TO_FLOAT (z);
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
  matrix[12] = COGL_FIXED_TO_FLOAT (x);
  matrix[13] = COGL_FIXED_TO_FLOAT (y);
  matrix[14] = COGL_FIXED_TO_FLOAT (z);
  matrix[15] = 1.0f;

  cogl_wrap_glMultMatrix (matrix);
}

void
cogl_wrap_glRotatex (GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
  float matrix[16];
  float xf = COGL_FIXED_TO_FLOAT (x);
  float yf = COGL_FIXED_TO_FLOAT (y);
  float zf = COGL_FIXED_TO_FLOAT (z);
  float anglef = COGL_FIXED_TO_FLOAT (angle) * G_PI / 180.0f;
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
  float xrange = COGL_FIXED_TO_FLOAT (right - left);
  float yrange = COGL_FIXED_TO_FLOAT (top - bottom);
  float zrange = COGL_FIXED_TO_FLOAT (far - near);

  memset (matrix, 0, sizeof (matrix));
  matrix[0] = 2.0f / xrange;
  matrix[5] = 2.0f / yrange;
  matrix[10] = 2.0f / zrange;
  matrix[12] = COGL_FIXED_TO_FLOAT (right + left) / xrange;
  matrix[13] = COGL_FIXED_TO_FLOAT (top + bottom) / yrange;
  matrix[14] = COGL_FIXED_TO_FLOAT (far + near) / zrange;
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
			 GL_TRUE, stride, pointer);
}

void
cogl_wrap_glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{
  glVertexAttribPointer (COGL_GLES2_WRAPPER_NORMAL_ATTRIB, 1, type,
			 GL_FALSE, stride, pointer);
}

void
cogl_wrap_glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
  CoglGles2WrapperProgram *program;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  /* Check if we need to switch programs */
  if (w->settings_dirty)
    {
      /* Find or create a program for the current settings */
      program = cogl_gles2_wrapper_get_program (&w->settings);

      if (program == NULL)
	/* Can't compile a shader so there is nothing we can do */
	return;

      /* Start using it if we aren't already */
      if (w->current_program != program)
	{
	  glUseProgram (program->program);
	  w->current_program = program;
	  /* All of the uniforms are probably now out of date */
	  w->dirty_uniforms = COGL_GLES2_DIRTY_ALL;
	  w->dirty_custom_uniforms = (1 << COGL_GLES2_NUM_CUSTOM_UNIFORMS) - 1;
	}
      w->settings_dirty = FALSE;
    }
  else
    program = w->current_program;

  /* Make sure all of the uniforms are up to date */
  if (w->dirty_uniforms)
    {
      if ((w->dirty_uniforms & (COGL_GLES2_DIRTY_MVP_MATRIX
				| COGL_GLES2_DIRTY_MODELVIEW_MATRIX)))
	{
	  float mvp_matrix[16];
	  const float *modelview_matrix = w->modelview_stack
	    + w->modelview_stack_pos * 16;

	  cogl_gles2_wrapper_mult_matrix (mvp_matrix,
					  w->projection_stack
					  + w->projection_stack_pos * 16,
					  modelview_matrix);	  

	  if (program->uniforms.mvp_matrix_uniform != -1)
	    glUniformMatrix4fv (program->uniforms.mvp_matrix_uniform, 1,
				GL_FALSE, mvp_matrix);
	  if (program->uniforms.modelview_matrix_uniform != -1)
	    glUniformMatrix4fv (program->uniforms.modelview_matrix_uniform, 1,
				GL_FALSE, modelview_matrix);
	}
      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_TEXTURE_MATRIX)
	  && program->uniforms.texture_matrix_uniform != -1)
	glUniformMatrix4fv (program->uniforms.texture_matrix_uniform, 1,
			    GL_FALSE,
			    w->texture_stack + w->texture_stack_pos * 16);

      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_FOG_DENSITY)
	  && program->uniforms.fog_density_uniform != -1)
	glUniform1f (program->uniforms.fog_density_uniform, w->fog_density);
      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_FOG_START)
	  && program->uniforms.fog_start_uniform != -1)
	glUniform1f (program->uniforms.fog_start_uniform, w->fog_start);
      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_FOG_END)
	  && program->uniforms.fog_end_uniform != -1)
	glUniform1f (program->uniforms.fog_end_uniform, w->fog_end);

      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_ALPHA_TEST_REF)
	  && program->uniforms.alpha_test_ref_uniform != -1)
	glUniform1f (program->uniforms.alpha_test_ref_uniform,
		     w->alpha_test_ref);

      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_TEXTURE_UNIT)
          && program->uniforms.texture_unit_uniform != -1)
        glUniform1i (program->uniforms.texture_unit_uniform, 0);

      w->dirty_uniforms = 0;
    }

  if (w->dirty_custom_uniforms)
    {
      int i;

      if (w->settings.user_program != COGL_INVALID_HANDLE)
	{
	  CoglProgram *user_program
	    = _cogl_program_pointer_from_handle (w->settings.user_program);
	  const char *uniform_name;

	  for (i = 0; i < COGL_GLES2_NUM_CUSTOM_UNIFORMS; i++)
	    if ((w->dirty_custom_uniforms & (1 << i))
		&& (uniform_name = user_program->custom_uniform_names[i]))
	      {
		if (program->custom_uniforms[i]
		    == COGL_GLES2_UNBOUND_CUSTOM_UNIFORM)
		  program->custom_uniforms[i]
		    = glGetUniformLocation (program->program, uniform_name);
		if (program->custom_uniforms[i] >= 0)
		  glUniform1f (program->custom_uniforms[i],
			       w->custom_uniforms[i]);
	      }
	}
      
      w->dirty_custom_uniforms = 0;
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
  _COGL_GLES2_CHANGE_SETTING (w, alpha_only, internal_format == GL_ALPHA);
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
      _COGL_GLES2_CHANGE_SETTING (w, texture_2d_enabled, TRUE);
      break;

    case GL_FOG:
      _COGL_GLES2_CHANGE_SETTING (w, fog_enabled, TRUE);
      break;

    case GL_ALPHA_TEST:
      _COGL_GLES2_CHANGE_SETTING (w, alpha_test_enabled, TRUE);
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
      _COGL_GLES2_CHANGE_SETTING (w, texture_2d_enabled, FALSE);
      break;

    case GL_FOG:
      _COGL_GLES2_CHANGE_SETTING (w, fog_enabled, FALSE);
      break;

    case GL_ALPHA_TEST:
      _COGL_GLES2_CHANGE_SETTING (w, alpha_test_enabled, FALSE);
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
    case GL_NORMAL_ARRAY:
      glEnableVertexAttribArray (COGL_GLES2_WRAPPER_NORMAL_ATTRIB);
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
    case GL_NORMAL_ARRAY:
      glDisableVertexAttribArray (COGL_GLES2_WRAPPER_NORMAL_ATTRIB);
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

  _COGL_GLES2_CHANGE_SETTING (w, alpha_test_func, func);
  _COGL_GLES2_CHANGE_UNIFORM (w, ALPHA_TEST_REF, alpha_test_ref, ref);
}

void
cogl_wrap_glColor4x (GLclampx r, GLclampx g, GLclampx b, GLclampx a)
{
  glVertexAttrib4f (COGL_GLES2_WRAPPER_COLOR_ATTRIB,
		    COGL_FIXED_TO_FLOAT (r),
		    COGL_FIXED_TO_FLOAT (g),
		    COGL_FIXED_TO_FLOAT (b),
		    COGL_FIXED_TO_FLOAT (a));
}

void
cogl_wrap_glClipPlanex (GLenum plane, GLfixed *equation)
{
  /* FIXME */
}

static void
cogl_gles2_float_array_to_fixed (int            size,
                                 const GLfloat *floats,
				 GLfixed       *fixeds)
{
  while (size-- > 0)
    *(fixeds++) = COGL_FIXED_FROM_FLOAT (*(floats++));
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
      _COGL_GLES2_CHANGE_SETTING (w, fog_mode, param);
      break;
      
    case GL_FOG_DENSITY:
      _COGL_GLES2_CHANGE_UNIFORM (w, FOG_DENSITY, fog_density,
				  COGL_FIXED_TO_FLOAT (param));
      break;

    case GL_FOG_START:
      _COGL_GLES2_CHANGE_UNIFORM (w, FOG_START, fog_start,
				  COGL_FIXED_TO_FLOAT (param));
      break;

    case GL_FOG_END:
      _COGL_GLES2_CHANGE_UNIFORM (w, FOG_END, fog_end,
				  COGL_FIXED_TO_FLOAT (param));
      break;
    }
}

void
cogl_wrap_glFogxv (GLenum pname, const GLfixed *params)
{
  int i;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  if (pname == GL_FOG_COLOR)
    {
      for (i = 0; i < 4; i++)
	w->fog_color[i] = COGL_FIXED_TO_FLOAT (params[i]);

      w->dirty_uniforms |= COGL_GLES2_DIRTY_FOG_COLOR;
    }
}

void
cogl_wrap_glTexParameteri (GLenum target, GLenum pname, GLfloat param)
{
  if (pname != GL_GENERATE_MIPMAP)
    glTexParameteri (target, pname, param);
}

void
_cogl_gles2_clear_cache_for_program (CoglHandle user_program)
{
  GSList *node, *next, *last = NULL;
  CoglGles2WrapperProgram *program;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  /* Remove any cached programs that link against this custom program */
  for (node = w->compiled_programs; node; node = next)
    {
      next = node->next;
      program = (CoglGles2WrapperProgram *) node->data;

      if (program->settings.user_program == user_program)
	{
	  glDeleteProgram (program->program);

	  if (last)
	    last->next = next;
	  else
	    w->compiled_programs = next;

	  g_slist_free1 (node);
	}
      else
	last = node;
    }
}
