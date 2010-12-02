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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>

/* We don't want to get the remaps from the gl* functions to the
   cogl_wrap_gl* functions in this file because we need to be able to
   call the base version */
#define COGL_GLES2_WRAPPER_NO_REMAP 1

#include "cogl.h"
#include "cogl-gles2-wrapper.h"
#include "cogl-fixed-vertex-shader.h"
#include "cogl-context.h"
#include "cogl-shader-private.h"
#include "cogl-shader.h"
#include "cogl-internal.h"

#define _COGL_GET_GLES2_WRAPPER(wvar, retval)			\
  CoglGles2Wrapper *wvar;					\
  {								\
    CoglContext *__ctxvar = _cogl_context_get_default ();	\
    if (__ctxvar == NULL) return retval;			\
    wvar = &__ctxvar->drv.gles2;				\
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
#define COGL_GLES2_WRAPPER_COLOR_ATTRIB     1
#define COGL_GLES2_WRAPPER_NORMAL_ATTRIB    2


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
      char shader_log[1024];
      GLint len;

      glGetShaderInfoLog (shader, sizeof (shader_log) - 1, &len, shader_log);
      shader_log[len] = '\0';

      g_critical ("%s", shader_log);

      glDeleteShader (shader);

      return 0;
    }

  return shader;
}

static void
initialize_texture_units (CoglGles2Wrapper *w)
{
  /* We save the active texture unit since we may need to temporarily
   * change this to initialise each new texture unit and we want to
   * restore the active unit afterwards */
  int initial_active_unit = w->active_texture_unit;
  GLint prev_mode;
  int i;

  /* We will need to set the matrix mode to GL_TEXTURE to
   * initialise any new texture units, so we save the current
   * mode for restoring afterwards */
  GE( _cogl_wrap_glGetIntegerv (GL_MATRIX_MODE, &prev_mode));

  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    {
      CoglGles2WrapperTextureUnit *new_unit;

      new_unit = w->texture_units + i;
      memset (new_unit, 0, sizeof (CoglGles2WrapperTextureUnit));

      w->active_texture_unit = i;
      GE( _cogl_wrap_glMatrixMode (GL_TEXTURE));
      GE( _cogl_wrap_glLoadIdentity ());
    }

  GE( _cogl_wrap_glMatrixMode ((GLenum) prev_mode));

  w->settings.texture_units = 0;

  w->active_texture_unit = initial_active_unit;
}

void
_cogl_gles2_wrapper_init (CoglGles2Wrapper *wrapper)
{
  memset (wrapper, 0, sizeof (CoglGles2Wrapper));

  /* Initialize the stacks */
  _cogl_wrap_glMatrixMode (GL_PROJECTION);
  _cogl_wrap_glLoadIdentity ();
  _cogl_wrap_glMatrixMode (GL_MODELVIEW);
  _cogl_wrap_glLoadIdentity ();

  /* The gl*ActiveTexture wrappers will initialise the texture
   * stack for the texture unit when it's first activated */
  _cogl_wrap_glActiveTexture (GL_TEXTURE0);
  _cogl_wrap_glClientActiveTexture (GL_TEXTURE0);

  /* Initialize the point size */
  _cogl_wrap_glPointSize (1.0f);

  initialize_texture_units (wrapper);
}

static gboolean
cogl_gles2_settings_equal (const CoglGles2WrapperSettings *a,
			   const CoglGles2WrapperSettings *b)
{
  if (a->texture_units != b->texture_units)
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
  int i;
  int n_texture_units = 0;

  _COGL_GET_GLES2_WRAPPER (w, NULL);

  /* Check if we already have a vertex shader for these settings */
  for (node = w->compiled_vertex_shaders; node; node = node->next)
    if (cogl_gles2_settings_equal (settings,
				   &((CoglGles2WrapperShader *)
				     node->data)->settings))
      return (CoglGles2WrapperShader *) node->data;

  /* Otherwise create a new shader */
  shader_source = g_string_new (_cogl_fixed_vertex_shader_per_vertex_attribs);

  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
      g_string_append_printf (shader_source,
			      "attribute vec4 cogl_tex_coord%d_in;\n",
			      i);

  /* Find the biggest enabled texture unit index */
  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
      n_texture_units = i + 1;

  g_string_append (shader_source, _cogl_fixed_vertex_shader_transform_matrices);
  g_string_append (shader_source, _cogl_fixed_vertex_shader_output_variables);

  if (n_texture_units > 0)
    {
      g_string_append_printf (shader_source,
                              "uniform mat4 cogl_texture_matrix[%d];\n",
                              n_texture_units);

      g_string_append_printf (shader_source,
                              "varying vec2 _cogl_tex_coord[%d];",
                              n_texture_units);
    }

  g_string_append (shader_source, _cogl_fixed_vertex_shader_fogging_options);
  g_string_append (shader_source, _cogl_fixed_vertex_shader_main_start);

  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
      {
        g_string_append_printf (shader_source,
                                "transformed_tex_coord = "
                                "cogl_texture_matrix[%d] "
                                " * cogl_tex_coord%d_in;\n",
                                i, i);
        g_string_append_printf (shader_source,
                                "_cogl_tex_coord[%d] = transformed_tex_coord.st "
                                " / transformed_tex_coord.q;\n",
                                i);
      }

  g_string_append (shader_source, _cogl_fixed_vertex_shader_frag_color_start);

  g_string_append (shader_source, _cogl_fixed_vertex_shader_end);

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

static void
cogl_gles2_wrapper_get_locations (GLuint program,
				  CoglGles2WrapperSettings *settings,
				  CoglGles2WrapperUniforms *uniforms,
				  CoglGles2WrapperAttributes *attribs)
{
  int i;

  uniforms->mvp_matrix_uniform
    = glGetUniformLocation (program, "cogl_modelview_projection_matrix");
  uniforms->modelview_matrix_uniform
    = glGetUniformLocation (program, "cogl_modelview_matrix");

  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    {
      char *matrix_var_name = g_strdup_printf ("cogl_texture_matrix[%d]", i);
      char *tex_coord_var_name =
        g_strdup_printf ("cogl_tex_coord%d_in", i);

      uniforms->texture_matrix_uniforms[i]
        = glGetUniformLocation (program, matrix_var_name);
      attribs->multi_texture_coords[i]
        = glGetAttribLocation (program, tex_coord_var_name);

      g_free (tex_coord_var_name);
      g_free (matrix_var_name);
    }

  uniforms->point_size_uniform
    = glGetUniformLocation (program, "cogl_point_size_in");
}

static void
cogl_gles2_wrapper_bind_attributes (GLuint program)
{
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_VERTEX_ATTRIB,
			"cogl_position_in");
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_COLOR_ATTRIB,
			"cogl_color_in");
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_NORMAL_ATTRIB,
			"cogl_normal_in");
}

static CoglGles2WrapperProgram *
cogl_gles2_wrapper_get_program (const CoglGles2WrapperSettings *settings)
{
  GSList *node;
  CoglGles2WrapperProgram *program;
  CoglGles2WrapperShader *vertex_shader;
  GLint status;
  gboolean custom_vertex_shader = FALSE, custom_fragment_shader = FALSE;
  GLuint shaders[16];
  GLsizei n_shaders = 0;
  int i;

  _COGL_GET_GLES2_WRAPPER (w, NULL);

  /* Check if we've already got a program for these settings */
  for (node = w->compiled_programs; node; node = node->next)
    {
      program = (CoglGles2WrapperProgram *) node->data;

      if (cogl_gles2_settings_equal (settings, &program->settings)
	  && program->settings.user_program == settings->user_program)
	return (CoglGles2WrapperProgram *) node->data;
    }

  /* Otherwise create a new program */

  if (settings->user_program)
    {
      /* We work out whether the program contains a vertex and
         fragment shader by looking at the list of attached shaders */
      glGetAttachedShaders (settings->user_program,
                            G_N_ELEMENTS (shaders),
                            &n_shaders, shaders);

      for (i = 0; i < n_shaders; i++)
        {
          GLint shader_type;

          glGetShaderiv (shaders[i], GL_SHADER_TYPE, &shader_type);

          if (shader_type == GL_VERTEX_SHADER)
            custom_vertex_shader = TRUE;
          else if (shader_type == GL_FRAGMENT_SHADER)
            custom_fragment_shader = TRUE;
        }
    }

  /* We should always have a custom shaders because the pipeline
     backend should create them for us */
  g_assert (custom_fragment_shader);
  g_assert (custom_vertex_shader);

  /* Get or create the fixed functionality shaders for these settings
     if there is no custom replacement */
  if (!custom_vertex_shader)
    {
      vertex_shader = cogl_gles2_get_vertex_shader (settings);
      if (vertex_shader == NULL)
	return NULL;
    }

  program = g_slice_new (CoglGles2WrapperProgram);

  program->program = settings->user_program;
  if (!custom_vertex_shader)
    glAttachShader (program->program, vertex_shader->shader);
  cogl_gles2_wrapper_bind_attributes (program->program);
  glLinkProgram (program->program);

  glGetProgramiv (program->program, GL_LINK_STATUS, &status);

  if (!status)
    {
      char shader_log[1024];
      GLint len;

      glGetProgramInfoLog (program->program, sizeof (shader_log) - 1, &len, shader_log);
      shader_log[len] = '\0';

      g_critical ("%s", shader_log);

      g_slice_free (CoglGles2WrapperProgram, program);

      return NULL;
    }

  program->settings = *settings;

  cogl_gles2_wrapper_get_locations (program->program,
				    &program->settings,
				    &program->uniforms,
				    &program->attributes);

  w->compiled_programs = g_slist_append (w->compiled_programs, program);

  return program;
}

void
_cogl_gles2_wrapper_deinit (CoglGles2Wrapper *wrapper)
{
  GSList *node, *next;

  for (node = wrapper->compiled_programs; node; node = next)
    {
      next = node->next;
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
}

static void
cogl_gles2_wrapper_notify_matrix_changed (CoglGles2Wrapper *wrapper,
                                          GLenum mode)
{
  CoglGles2WrapperTextureUnit *texture_unit;

  switch (mode)
    {
    case GL_MODELVIEW:
      wrapper->dirty_uniforms |= COGL_GLES2_DIRTY_MVP_MATRIX
	| COGL_GLES2_DIRTY_MODELVIEW_MATRIX;
      break;

    case GL_PROJECTION:
      wrapper->dirty_uniforms |= COGL_GLES2_DIRTY_MVP_MATRIX;
      break;

    case GL_TEXTURE:
      wrapper->dirty_uniforms |= COGL_GLES2_DIRTY_TEXTURE_MATRICES;
      texture_unit = wrapper->texture_units + wrapper->active_texture_unit;
      texture_unit->dirty_matrix = 1;
      break;

    default:
      g_critical ("%s: Unexpected matrix mode %d\n", G_STRFUNC, mode);
    }
}

void
_cogl_wrap_glMatrixMode (GLenum mode)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  w->matrix_mode = mode;
}

static CoglMatrix *
cogl_gles2_get_current_matrix (CoglGles2Wrapper *wrapper)
{
  CoglGles2WrapperTextureUnit *texture_unit;

  switch (wrapper->matrix_mode)
    {
    default:
      g_critical ("%s: Unexpected matrix mode %d\n",
                  G_STRFUNC, wrapper->matrix_mode);
      /* flow through */

    case GL_MODELVIEW:
      return &wrapper->modelview_matrix;

    case GL_PROJECTION:
      return &wrapper->projection_matrix;

    case GL_TEXTURE:
      texture_unit = wrapper->texture_units + wrapper->active_texture_unit;
      return &texture_unit->texture_matrix;
    }
}

void
_cogl_wrap_glLoadIdentity (void)
{
  CoglMatrix *matrix;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  matrix = cogl_gles2_get_current_matrix (w);

  cogl_matrix_init_identity (matrix);

  cogl_gles2_wrapper_notify_matrix_changed (w, w->matrix_mode);
}

void
_cogl_wrap_glLoadMatrixf (const GLfloat *m)
{
  CoglMatrix *matrix;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  matrix = cogl_gles2_get_current_matrix (w);

  cogl_matrix_init_from_array (matrix, m);

  cogl_gles2_wrapper_notify_matrix_changed (w, w->matrix_mode);
}

void
_cogl_wrap_glVertexPointer (GLint size, GLenum type, GLsizei stride,
			   const GLvoid *pointer)
{
  glVertexAttribPointer (COGL_GLES2_WRAPPER_VERTEX_ATTRIB, size, type,
			 GL_FALSE, stride, pointer);
}

void
_cogl_wrap_glTexCoordPointer (GLint size, GLenum type, GLsizei stride,
			     const GLvoid *pointer)
{
  int active_unit;
  CoglGles2WrapperTextureUnit *texture_unit;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  active_unit = w->active_client_texture_unit;

  texture_unit = w->texture_units + active_unit;
  texture_unit->texture_coords_size = size;
  texture_unit->texture_coords_type = type;
  texture_unit->texture_coords_stride = stride;
  texture_unit->texture_coords_pointer = pointer;

  w->dirty_attribute_pointers
    |= COGL_GLES2_DIRTY_TEX_COORD_VERTEX_ATTRIB;
}

void
_cogl_wrap_glColorPointer (GLint size, GLenum type, GLsizei stride,
                           const GLvoid *pointer)
{
  glVertexAttribPointer (COGL_GLES2_WRAPPER_COLOR_ATTRIB, size, type,
			 GL_TRUE, stride, pointer);
}

void
_cogl_wrap_glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{
  glVertexAttribPointer (COGL_GLES2_WRAPPER_NORMAL_ATTRIB, 1, type,
			 GL_FALSE, stride, pointer);
}

static void
_cogl_wrap_prepare_for_draw (void)
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
	  w->current_program = program;
	  /* All of the uniforms are probably now out of date */
	  w->dirty_uniforms = COGL_GLES2_DIRTY_ALL;
	}
      w->settings_dirty = FALSE;
    }
  else
    program = w->current_program;

  /* We always have to reassert the program even if it hasn't changed
     because the fixed-function material backend disables the program
     again in the _start function. This should go away once the GLSL
     code is generated in the GLSL material backend so it's probably
     not worth worrying about now */
  _cogl_use_fragment_program (w->settings.user_program,
                              COGL_PIPELINE_PROGRAM_TYPE_GLSL);
  _cogl_use_vertex_program (w->settings.user_program,
                            COGL_PIPELINE_PROGRAM_TYPE_GLSL);

  /* Make sure all of the uniforms are up to date */
  if (w->dirty_uniforms)
    {
      if ((w->dirty_uniforms & (COGL_GLES2_DIRTY_MVP_MATRIX
				| COGL_GLES2_DIRTY_MODELVIEW_MATRIX)))
	{
	  CoglMatrix mvp_matrix;
	  CoglMatrix *modelview_matrix = &w->modelview_matrix;
	  CoglMatrix *projection_matrix = &w->projection_matrix;

          /* FIXME: we should have a cogl_matrix_copy () function */
          memcpy (&mvp_matrix, projection_matrix, sizeof (CoglMatrix));

          cogl_matrix_multiply (&mvp_matrix, &mvp_matrix, modelview_matrix);

	  if (program->uniforms.mvp_matrix_uniform != -1)
	    glUniformMatrix4fv (program->uniforms.mvp_matrix_uniform, 1,
				GL_FALSE, (float *) &mvp_matrix);
	  if (program->uniforms.modelview_matrix_uniform != -1)
	    glUniformMatrix4fv (program->uniforms.modelview_matrix_uniform, 1,
				GL_FALSE, (float *) &modelview_matrix);
	}
      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_TEXTURE_MATRICES))
	{
	  int i;

	  /* TODO - we should probably have a per unit dirty flag too */

	  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
	    {
	      CoglGles2WrapperTextureUnit *texture_unit;
	      GLint uniform = program->uniforms.texture_matrix_uniforms[i];

              texture_unit = w->texture_units + i;
	      if (uniform != -1)
		glUniformMatrix4fv (uniform, 1, GL_FALSE,
                                    (float *) &texture_unit->texture_matrix);
	    }
	}

      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_POINT_SIZE))
        glUniform1f (program->uniforms.point_size_uniform,
                     w->point_size);

      w->dirty_uniforms = 0;
    }

  if (w->dirty_attribute_pointers
      & COGL_GLES2_DIRTY_TEX_COORD_VERTEX_ATTRIB)
    {
      int i;

      /* TODO - coverage test */
      for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
        {
          GLint tex_coord_var_index;
          CoglGles2WrapperTextureUnit *texture_unit;

          texture_unit = w->texture_units + i;
          if (!texture_unit->texture_coords_enabled)
            continue;

          /* TODO - we should probably have a per unit dirty flag too */

          /* TODO - coverage test */
          tex_coord_var_index = program->attributes.multi_texture_coords[i];
          glVertexAttribPointer (tex_coord_var_index,
                                 texture_unit->texture_coords_size,
                                 texture_unit->texture_coords_type,
                                 GL_FALSE,
                                 texture_unit->texture_coords_stride,
                                 texture_unit->texture_coords_pointer);
        }
    }

  if (w->dirty_vertex_attrib_enables)
    {
      int i;

      /* TODO - coverage test */

      /* TODO - we should probably have a per unit dirty flag too */

      for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
	{
          CoglGles2WrapperTextureUnit *texture_unit = w->texture_units + i;
          GLint attrib = program->attributes.multi_texture_coords[i];

          if (attrib != -1)
            {
              if (texture_unit->texture_coords_enabled)
                glEnableVertexAttribArray (attrib);
              else
                glDisableVertexAttribArray (attrib);
            }
	}

      w->dirty_vertex_attrib_enables = 0;
    }
}

void
_cogl_wrap_glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
  _cogl_wrap_prepare_for_draw ();

  glDrawArrays (mode, first, count);
}

void
_cogl_wrap_glDrawElements (GLenum mode, GLsizei count, GLenum type,
                          const GLvoid *indices)
{
  _cogl_wrap_prepare_for_draw ();

  glDrawElements (mode, count, type, indices);
}

void
_cogl_wrap_glClientActiveTexture (GLenum texture)
{
  int texture_unit_index = texture - GL_TEXTURE0;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  if (texture_unit_index < COGL_GLES2_MAX_TEXTURE_UNITS)
    w->active_client_texture_unit = texture_unit_index;
}

void
_cogl_wrap_glActiveTexture (GLenum texture)
{
  int texture_unit_index = texture - GL_TEXTURE0;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  glActiveTexture (texture);

  if (texture_unit_index < COGL_GLES2_MAX_TEXTURE_UNITS)
    w->active_texture_unit = texture_unit_index;
}

void
_cogl_wrap_glEnable (GLenum cap)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (cap)
    {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_3D_OES:
      if (!COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (w->settings.texture_units,
                                               w->active_texture_unit))
        {
          COGL_GLES2_TEXTURE_UNIT_SET_ENABLED (w->settings.texture_units,
                                               w->active_texture_unit,
                                               TRUE);
          w->settings_dirty = TRUE;
        }
      break;

    default:
      glEnable (cap);
    }
}

void
_cogl_wrap_glDisable (GLenum cap)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (cap)
    {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_3D_OES:
      /* If this was the last enabled texture target then we'll
         completely disable the unit */
      if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (w->settings.texture_units,
                                              w->active_texture_unit))
        {
          COGL_GLES2_TEXTURE_UNIT_SET_ENABLED (w->settings.texture_units,
                                               w->active_texture_unit,
                                               FALSE);
          w->settings_dirty = TRUE;
        }
      break;

    default:
      glDisable (cap);
    }
}

void
_cogl_wrap_glEnableClientState (GLenum array)
{
  CoglGles2WrapperTextureUnit *texture_unit;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (array)
    {
    case GL_VERTEX_ARRAY:
      glEnableVertexAttribArray (COGL_GLES2_WRAPPER_VERTEX_ATTRIB);
      break;
    case GL_TEXTURE_COORD_ARRAY:
      /* TODO - review if this should be in w->settings? */

      texture_unit = w->texture_units + w->active_client_texture_unit;
      if (texture_unit->texture_coords_enabled != 1)
	{
	  texture_unit->texture_coords_enabled = 1;
	  w->dirty_vertex_attrib_enables
	    |= COGL_GLES2_DIRTY_TEX_COORD_ATTRIB_ENABLES;
	}
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
_cogl_wrap_glDisableClientState (GLenum array)
{
  CoglGles2WrapperTextureUnit *texture_unit;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (array)
    {
    case GL_VERTEX_ARRAY:
      glDisableVertexAttribArray (COGL_GLES2_WRAPPER_VERTEX_ATTRIB);
      break;
    case GL_TEXTURE_COORD_ARRAY:

      texture_unit = w->texture_units + w->active_texture_unit;
      /* TODO - review if this should be in w->settings? */
      if (texture_unit->texture_coords_enabled != 0)
	{
	  texture_unit->texture_coords_enabled = 0;
	  w->dirty_vertex_attrib_enables
	    |= COGL_GLES2_DIRTY_TEX_COORD_ATTRIB_ENABLES;
	}
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
_cogl_wrap_glColor4f (GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
  glVertexAttrib4f (COGL_GLES2_WRAPPER_COLOR_ATTRIB, r, g, b, a);
}

void
_cogl_wrap_glColor4ub (GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
  glVertexAttrib4f (COGL_GLES2_WRAPPER_COLOR_ATTRIB,
                    r/255.0, g/255.0, b/255.0, a/255.0);
}

void
_cogl_wrap_glClipPlanef (GLenum plane, GLfloat *equation)
{
  /* FIXME */
}

void
_cogl_wrap_glGetIntegerv (GLenum pname, GLint *params)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (pname)
    {
    case GL_MAX_CLIP_PLANES:
      *params = 0;
      break;

    case GL_MATRIX_MODE:
      *params = w->matrix_mode;
      break;

    case GL_MAX_TEXTURE_UNITS:
      glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, params);
      if (*params > COGL_GLES2_MAX_TEXTURE_UNITS)
        *params = COGL_GLES2_MAX_TEXTURE_UNITS;
      break;

    default:
      glGetIntegerv (pname, params);
      break;
    }
}

void
_cogl_wrap_glGetFloatv (GLenum pname, GLfloat *params)
{
  CoglGles2WrapperTextureUnit *texture_unit;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (pname)
    {
    case GL_MODELVIEW_MATRIX:
      memcpy (params, &w->modelview_matrix, sizeof (GLfloat) * 16);
      break;

    case GL_PROJECTION_MATRIX:
      memcpy (params, &w->projection_matrix, sizeof (GLfloat) * 16);
      break;

    case GL_TEXTURE_MATRIX:
      texture_unit = w->texture_units + w->active_texture_unit;
      memcpy (params, &texture_unit->texture_matrix, sizeof (GLfloat) * 16);
      break;

    case GL_VIEWPORT:
      glGetFloatv (GL_VIEWPORT, params);
      break;
    }
}

void
_cogl_wrap_glTexParameteri (GLenum target, GLenum pname, GLfloat param)
{
  if (pname != GL_GENERATE_MIPMAP)
    glTexParameteri (target, pname, param);
}

void
_cogl_wrap_glMaterialfv (GLenum face, GLenum pname, const GLfloat *params)
{
  /* FIXME: the GLES 2 backend doesn't yet support lighting so this
     function can't do anything */
}

void
_cogl_wrap_glPointSize (GLfloat size)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  w->point_size = size;
  w->dirty_uniforms |= COGL_GLES2_DIRTY_POINT_SIZE;
}

/* This function is a massive hack to get custom GLSL programs to
   work. It's only necessary until we move the GLSL shader generation
   into the CoglMaterial. The gl_program specifies the user program to
   be used. The list of shaders will be extracted out of this and
   compiled into a new program containing any fixed function shaders
   that need to be generated. The new program will be returned. */
GLuint
_cogl_gles2_use_program (GLuint gl_program)
{
  _COGL_GET_GLES2_WRAPPER (w, 0);

  _COGL_GLES2_CHANGE_SETTING (w, user_program, gl_program);

  /* We need to bind the program immediately so that the GLSL material
     backend can update the custom uniforms */
  _cogl_wrap_prepare_for_draw ();

  return w->current_program->program;
}

void
_cogl_gles2_clear_cache_for_program (GLuint gl_program)
{
  GSList *node, *next, *last = NULL;
  CoglGles2WrapperProgram *program;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  if (w->settings.user_program == gl_program)
    {
      w->settings.user_program = 0;
      w->settings_dirty = TRUE;
    }

  /* Remove any cached programs that link against this custom program */
  for (node = w->compiled_programs; node; node = next)
    {
      next = node->next;
      program = (CoglGles2WrapperProgram *) node->data;

      if (program->settings.user_program == gl_program)
	{
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
