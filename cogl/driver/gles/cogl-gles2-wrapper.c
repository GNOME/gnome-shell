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

#include <clutter/clutter-fixed.h>
#include <string.h>
#include <math.h>

/* We don't want to get the remaps from the gl* functions to the
   cogl_wrap_gl* functions in this file because we need to be able to
   call the base version */
#define COGL_GLES2_WRAPPER_NO_REMAP 1

#include "cogl.h"
#include "cogl-gles2-wrapper.h"
#include "cogl-fixed-vertex-shader.h"
#include "cogl-fixed-fragment-shader.h"
#include "cogl-context.h"
#include "cogl-shader-private.h"
#include "cogl-program.h"
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
  GE( cogl_wrap_glGetIntegerv (GL_MATRIX_MODE, &prev_mode));

  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    {
      CoglGles2WrapperTextureUnit *new_unit;

      new_unit = w->texture_units + i;
      memset (new_unit, 0, sizeof (CoglGles2WrapperTextureUnit));

      w->active_texture_unit = i;
      GE( cogl_wrap_glMatrixMode (GL_TEXTURE));
      GE( cogl_wrap_glLoadIdentity ());

      /* The real GL default is GL_MODULATE but the shader only
         supports GL_COMBINE so let's default to that instead */
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                               GL_COMBINE) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB,
                               GL_MODULATE) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB,
                               GL_PREVIOUS) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB,
                               GL_TEXTURE) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB,
                               GL_SRC_COLOR) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB,
                               GL_SRC_COLOR) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA,
                               GL_MODULATE) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA,
                               GL_PREVIOUS) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA,
                               GL_TEXTURE) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,
                               GL_SRC_COLOR) );
      GE( cogl_wrap_glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA,
                               GL_SRC_COLOR) );
    }

  GE( cogl_wrap_glMatrixMode ((GLenum) prev_mode));

  w->settings.texture_units = 0;

  w->active_texture_unit = initial_active_unit;
}

void
cogl_gles2_wrapper_init (CoglGles2Wrapper *wrapper)
{
  GLfloat default_fog_color[4] = { 0, 0, 0, 0 };

  memset (wrapper, 0, sizeof (CoglGles2Wrapper));

  /* Initialize the stacks */
  cogl_wrap_glMatrixMode (GL_PROJECTION);
  cogl_wrap_glLoadIdentity ();
  cogl_wrap_glMatrixMode (GL_MODELVIEW);
  cogl_wrap_glLoadIdentity ();

  /* The gl*ActiveTexture wrappers will initialise the texture
   * stack for the texture unit when it's first activated */
  cogl_wrap_glActiveTexture (GL_TEXTURE0);
  cogl_wrap_glClientActiveTexture (GL_TEXTURE0);

  /* Initialize the fogging options */
  cogl_wrap_glDisable (GL_FOG);
  cogl_wrap_glFogf (GL_FOG_MODE, GL_LINEAR);
  cogl_wrap_glFogf (GL_FOG_DENSITY, 1.0);
  cogl_wrap_glFogf (GL_FOG_START, 0);
  cogl_wrap_glFogf (GL_FOG_END, 1);
  cogl_wrap_glFogfv (GL_FOG_COLOR, default_fog_color);

  /* Initialize alpha testing */
  cogl_wrap_glDisable (GL_ALPHA_TEST);
  cogl_wrap_glAlphaFunc (GL_ALWAYS, 0.0f);

  initialize_texture_units (wrapper);
}

static int
cogl_gles2_get_n_args_for_combine_func (GLenum func)
{
  switch (func)
    {
    case GL_REPLACE:
      return 1;
    case GL_MODULATE:
    case GL_ADD:
    case GL_ADD_SIGNED:
    case GL_SUBTRACT:
    case GL_DOT3_RGB:
    case GL_DOT3_RGBA:
      return 2;
    case GL_INTERPOLATE:
      return 3;
    default:
      return 0;
    }
}

static gboolean
cogl_gles2_settings_equal (const CoglGles2WrapperSettings *a,
			   const CoglGles2WrapperSettings *b,
			   gboolean vertex_tests,
			   gboolean fragment_tests)
{
  int i;

  if (a->texture_units != b->texture_units)
    return FALSE;

  if (fragment_tests)
    {
      if (a->alpha_test_enabled != b->alpha_test_enabled)
	return FALSE;
      if (a->alpha_test_enabled && a->alpha_test_func != b->alpha_test_func)
	return FALSE;
    }

  if (a->fog_enabled != b->fog_enabled)
    return FALSE;

  if (vertex_tests && a->fog_enabled && a->fog_mode != b->fog_mode)
    return FALSE;

  /* Compare layer combine operation for each active unit */
  if (fragment_tests)
    for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
      if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (a->texture_units, i))
        {
          const CoglGles2WrapperTexEnv *tex_env_a = a->tex_env + i;
          const CoglGles2WrapperTexEnv *tex_env_b = b->tex_env + i;
          int arg, n_args;
          GLenum func;

          func = tex_env_a->texture_combine_rgb_func;

          if (func != tex_env_b->texture_combine_rgb_func)
            return FALSE;

          n_args = cogl_gles2_get_n_args_for_combine_func (func);

          for (arg = 0; arg < n_args; arg++)
            {
              if (tex_env_a->texture_combine_rgb_src[arg] !=
                  tex_env_b->texture_combine_rgb_src[arg])
                return FALSE;
              if (tex_env_a->texture_combine_rgb_op[arg] !=
                  tex_env_b->texture_combine_rgb_op[arg])
                return FALSE;
            }
        }

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
				     node->data)->settings,
				   TRUE, FALSE))
      return (CoglGles2WrapperShader *) node->data;

  /* Otherwise create a new shader */
  shader_source = g_string_new (cogl_fixed_vertex_shader_per_vertex_attribs);

  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
      g_string_append_printf (shader_source,
			      "attribute vec4 multi_tex_coord_attrib%d;\n",
			      i);

  /* Find the biggest enabled texture unit index */
  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
      n_texture_units = i + 1;

  g_string_append (shader_source, cogl_fixed_vertex_shader_transform_matrices);
  g_string_append (shader_source, cogl_fixed_vertex_shader_output_variables);

  if (n_texture_units > 0)
    {
      g_string_append_printf (shader_source,
                              "uniform mat4	      texture_matrix[%d];\n",
                              n_texture_units);

      g_string_append_printf (shader_source,
                              "varying vec2       tex_coord[%d];",
                              n_texture_units);
    }

  g_string_append (shader_source, cogl_fixed_vertex_shader_fogging_options);
  g_string_append (shader_source, cogl_fixed_vertex_shader_main_start);

  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
      {
        g_string_append_printf (shader_source,
                                "transformed_tex_coord = "
                                "texture_matrix[%d] "
                                " * multi_tex_coord_attrib%d;\n",
                                i, i);
        g_string_append_printf (shader_source,
                                "tex_coord[%d] = transformed_tex_coord.st "
                                " / transformed_tex_coord.q;\n",
                                i);
      }

  g_string_append (shader_source, cogl_fixed_vertex_shader_frag_color_start);

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

static void
cogl_gles2_add_arg (int unit,
                    GLenum src,
                    GLenum operand,
                    const char *swizzle,
                    GString *shader_source)
{
  char alpha_swizzle[5] = "aaaa";

  g_string_append_c (shader_source, '(');

  if (operand == GL_ONE_MINUS_SRC_COLOR || operand == GL_ONE_MINUS_SRC_ALPHA)
    g_string_append_printf (shader_source,
                            "vec4(1.0, 1.0, 1.0, 1.0).%s - ",
                            swizzle);

  /* If the operand is reading from the alpha then replace the swizzle
     with the same number of copies of the alpha */
  if (operand == GL_SRC_ALPHA || operand == GL_ONE_MINUS_SRC_ALPHA)
    {
      alpha_swizzle[strlen (swizzle)] = '\0';
      swizzle = alpha_swizzle;
    }

  switch (src)
    {
    case GL_TEXTURE:
      g_string_append_printf (shader_source,
                              "texture2D (texture_unit[%d], tex_coord[%d]).%s",
                              unit, unit, swizzle);
      break;

    case GL_CONSTANT:
      g_string_append_printf (shader_source, "combine_constant[%d].%s",
                              unit, swizzle);
      break;

    case GL_PREVIOUS:
      if (unit > 0)
        {
          g_string_append_printf (shader_source, "gl_FragColor.%s", swizzle);
          break;
        }
      /* flow through */
    case GL_PRIMARY_COLOR:
      g_string_append_printf (shader_source, "frag_color.%s", swizzle);
      break;

    default:
      if (src >= GL_TEXTURE0 &&
          src < GL_TEXTURE0 + COGL_GLES2_MAX_TEXTURE_UNITS)
        g_string_append_printf (shader_source,
                                "texture2D (texture_unit[%d], "
                                "tex_coord[%d]).%s",
                                src - GL_TEXTURE0,
                                src - GL_TEXTURE0,
                                swizzle);
      break;
    }

  g_string_append_c (shader_source, ')');
}

static void
cogl_gles2_add_operation (int unit,
                          GLenum combine_func,
                          const GLenum *sources,
                          const GLenum *operands,
                          const char *swizzle,
                          GString *shader_source)
{
  switch (combine_func)
    {
    case GL_REPLACE:
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          swizzle, shader_source);
      break;

    case GL_MODULATE:
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          swizzle, shader_source);
      g_string_append (shader_source, " * ");
      cogl_gles2_add_arg (unit, sources[1], operands[1],
                          swizzle, shader_source);
      break;

    case GL_ADD:
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          swizzle, shader_source);
      g_string_append (shader_source, " + ");
      cogl_gles2_add_arg (unit, sources[1], operands[1],
                          swizzle, shader_source);
      break;

    case GL_ADD_SIGNED:
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          swizzle, shader_source);
      g_string_append (shader_source, " + ");
      cogl_gles2_add_arg (unit, sources[1], operands[1],
                          swizzle, shader_source);
      g_string_append_printf (shader_source,
                              " - vec4(0.5, 0.5, 0.5, 0.5).%s",
                              swizzle);
      break;

    case GL_SUBTRACT:
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          swizzle, shader_source);
      g_string_append (shader_source, " - ");
      cogl_gles2_add_arg (unit, sources[1], operands[1],
                          swizzle, shader_source);
      break;

    case GL_INTERPOLATE:
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          swizzle, shader_source);
      g_string_append (shader_source, " * ");
      cogl_gles2_add_arg (unit, sources[2], operands[2],
                          swizzle, shader_source);
      g_string_append (shader_source, " + ");
      cogl_gles2_add_arg (unit, sources[1], operands[1],
                          swizzle, shader_source);
      g_string_append_printf (shader_source,
                              " * (vec4(1.0, 1.0, 1.0, 1.0).%s - ",
                              swizzle);
      cogl_gles2_add_arg (unit, sources[2], operands[2],
                          swizzle, shader_source);
      g_string_append_c (shader_source, ')');
      break;

    case GL_DOT3_RGB:
    case GL_DOT3_RGBA:
      g_string_append (shader_source, "vec4(4 * ((");
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          "r", shader_source);
      g_string_append (shader_source, " - 0.5) * (");
      cogl_gles2_add_arg (unit, sources[1], operands[1],
                          "r", shader_source);
      g_string_append (shader_source, " - 0.5) + (");
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          "g", shader_source);
      g_string_append (shader_source, " - 0.5) * (");
      cogl_gles2_add_arg (unit, sources[1], operands[1],
                          "g", shader_source);
      g_string_append (shader_source, " - 0.5) + (");
      cogl_gles2_add_arg (unit, sources[0], operands[0],
                          "b", shader_source);
      g_string_append (shader_source, " - 0.5) * (");
      cogl_gles2_add_arg (unit, sources[1], operands[1],
                          "b", shader_source);
      g_string_append_printf (shader_source, " - 0.5))).%s", swizzle);
      break;
    }
}

static gboolean
cogl_gles2_rgb_and_alpha_equal (const CoglGles2WrapperTexEnv *tex_env)
{
  int arg, n_args;

  if (tex_env->texture_combine_rgb_func != tex_env->texture_combine_alpha_func)
    return FALSE;

  n_args =
    cogl_gles2_get_n_args_for_combine_func (tex_env->texture_combine_rgb_func);

  for (arg = 0; arg < n_args; arg++)
    {
      if (tex_env->texture_combine_rgb_src[arg] !=
          tex_env->texture_combine_alpha_src[arg])
        return FALSE;

      if (tex_env->texture_combine_rgb_op[arg] != GL_SRC_COLOR ||
          tex_env->texture_combine_alpha_op[arg] != GL_SRC_ALPHA)
        return FALSE;
    }

  return TRUE;
}


static CoglGles2WrapperShader *
cogl_gles2_get_fragment_shader (const CoglGles2WrapperSettings *settings)
{
  GString *shader_source;
  GLuint shader_obj;
  CoglGles2WrapperShader *shader;
  GSList *node;
  int i;
  int n_texture_units = 0;

  _COGL_GET_GLES2_WRAPPER (w, NULL);

  /* Check if we already have a fragment shader for these settings */
  for (node = w->compiled_fragment_shaders; node; node = node->next)
    if (cogl_gles2_settings_equal (settings,
				   &((CoglGles2WrapperShader *)
				     node->data)->settings,
				   FALSE, TRUE))
      return (CoglGles2WrapperShader *) node->data;

  /* Otherwise create a new shader */
  shader_source = g_string_new (cogl_fixed_fragment_shader_variables_start);

  /* Find the biggest enabled texture unit index */
  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
      n_texture_units = i + 1;

  g_string_append (shader_source, cogl_fixed_fragment_shader_inputs);

  if (n_texture_units > 0)
    {
      g_string_append_printf (shader_source,
                              "varying vec2       tex_coord[%d];\n",
                              n_texture_units);

      g_string_append (shader_source, cogl_fixed_fragment_shader_texturing_options);
      g_string_append_printf (shader_source,
                              "uniform sampler2D  texture_unit[%d];\n",
                              n_texture_units);
    }

  g_string_append (shader_source, cogl_fixed_fragment_shader_fogging_options);

  g_string_append (shader_source, cogl_fixed_fragment_shader_main_declare);

  g_string_append (shader_source, cogl_fixed_fragment_shader_main_start);

  /* This pointless extra variable is needed to work around an
     apparent bug in the PowerVR drivers. Without it the alpha
     blending seems to stop working */
  g_string_append (shader_source,
                   "vec4 frag_color_copy = frag_color;\n");

  /* If there are no textures units enabled then we can just directly
     use the color from the vertex shader */
  if (n_texture_units == 0)
    g_string_append (shader_source, "gl_FragColor = frag_color;\n");
  else
    /* Otherwise we need to calculate the value based on the layer
       combine settings */
    for (i = 0; i < n_texture_units; i++)
      if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
        {
          const CoglGles2WrapperTexEnv *tex_env = settings->tex_env + i;

          /* If the rgb and alpha combine functions are the same then
             we can do both with a single statement, otherwise we need
             to do them separately */
          if (cogl_gles2_rgb_and_alpha_equal (tex_env))
            {
              g_string_append (shader_source, "gl_FragColor.rgba = ");
              cogl_gles2_add_operation (i,
                                        tex_env->texture_combine_rgb_func,
                                        tex_env->texture_combine_rgb_src,
                                        tex_env->texture_combine_rgb_op,
                                        "rgba",
                                        shader_source);
              g_string_append (shader_source, ";\n");
            }
          else
            {
              g_string_append (shader_source, "gl_FragColor.rgb = ");
              cogl_gles2_add_operation (i,
                                        tex_env->texture_combine_rgb_func,
                                        tex_env->texture_combine_rgb_src,
                                        tex_env->texture_combine_rgb_op,
                                        "rgb",
                                        shader_source);
              g_string_append (shader_source,
                               ";\n"
                               "gl_FragColor.a = ");
              cogl_gles2_add_operation (i,
                                        tex_env->texture_combine_alpha_func,
                                        tex_env->texture_combine_alpha_src,
                                        tex_env->texture_combine_alpha_op,
                                        "a",
                                        shader_source);
              g_string_append (shader_source, ";\n");
            }
        }

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

static void
cogl_gles2_wrapper_get_locations (GLuint program,
				  CoglGles2WrapperSettings *settings,
				  CoglGles2WrapperUniforms *uniforms,
				  CoglGles2WrapperAttributes *attribs)
{
  int i;

  uniforms->mvp_matrix_uniform
    = glGetUniformLocation (program, "mvp_matrix");
  uniforms->modelview_matrix_uniform
    = glGetUniformLocation (program, "modelview_matrix");

  for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
    if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (settings->texture_units, i))
      {
        char *matrix_var_name = g_strdup_printf ("texture_matrix[%d]", i);
        char *sampler_var_name = g_strdup_printf ("texture_unit[%d]", i);
        char *tex_coord_var_name =
          g_strdup_printf ("multi_tex_coord_attrib%d", i);

        uniforms->texture_matrix_uniforms[i]
          = glGetUniformLocation (program, matrix_var_name);
        uniforms->texture_sampler_uniforms[i]
          = glGetUniformLocation (program, sampler_var_name);
        attribs->multi_texture_coords[i]
          = glGetAttribLocation (program, tex_coord_var_name);

        g_free (tex_coord_var_name);
        g_free (sampler_var_name);
        g_free (matrix_var_name);
      }
    else
      {
        uniforms->texture_matrix_uniforms[i] = -1;
        uniforms->texture_sampler_uniforms[i] = -1;
        attribs->multi_texture_coords[i] = -1;
      }

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
}

static void
cogl_gles2_wrapper_bind_attributes (GLuint program)
{
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_VERTEX_ATTRIB,
			"vertex_attrib");
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_COLOR_ATTRIB,
			"color_attrib");
  glBindAttribLocation (program, COGL_GLES2_WRAPPER_NORMAL_ATTRIB,
			"normal_attrib");
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

	  if (shader->type == COGL_SHADER_TYPE_VERTEX)
	    custom_vertex_shader = TRUE;
	  else if (shader->type == COGL_SHADER_TYPE_FRAGMENT)
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
      char shader_log[1024];
      GLint len;

      glGetProgramInfoLog (program->program, sizeof (shader_log) - 1, &len, shader_log);
      shader_log[len] = '\0';

      g_critical ("%s", shader_log);

      glDeleteProgram (program->program);
      g_slice_free (CoglGles2WrapperProgram, program);

      return NULL;
    }

  program->settings = *settings;

  cogl_gles2_wrapper_get_locations (program->program,
				    &program->settings,
				    &program->uniforms,
				    &program->attributes);

  /* We haven't tried to get a location for any of the custom uniforms
     yet */
  for (i = 0; i < COGL_GLES2_NUM_CUSTOM_UNIFORMS; i++)
    program->custom_uniforms[i] = COGL_GLES2_UNBOUND_CUSTOM_UNIFORM;

  w->compiled_programs = g_slist_append (w->compiled_programs, program);

  return program;
}

void
cogl_gles2_wrapper_deinit (CoglGles2Wrapper *wrapper)
{
  GSList *node, *next;
  int i;

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

  for (i = 0; i < COGL_GLES2_NUM_CUSTOM_UNIFORMS; i++)
    if (wrapper->custom_uniforms[i].count > 1)
      g_free (wrapper->custom_uniforms[i].v.array);
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
cogl_wrap_glMatrixMode (GLenum mode)
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
cogl_wrap_glLoadIdentity (void)
{
  CoglMatrix *matrix;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  matrix = cogl_gles2_get_current_matrix (w);

  cogl_matrix_init_identity (matrix);

  cogl_gles2_wrapper_notify_matrix_changed (w, w->matrix_mode);
}

void
cogl_wrap_glLoadMatrixf (const GLfloat *m)
{
  CoglMatrix *matrix;

  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  matrix = cogl_gles2_get_current_matrix (w);

  cogl_matrix_init_from_array (matrix, m);

  cogl_gles2_wrapper_notify_matrix_changed (w, w->matrix_mode);
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

static void
cogl_gles2_do_set_uniform (GLint location, CoglBoxedValue *value)
{
  switch (value->type)
    {
    case COGL_BOXED_NONE:
      break;

    case COGL_BOXED_INT:
      {
	int *ptr;

	if (value->count == 1)
	  ptr = value->v.int_value;
	else
	  ptr = value->v.int_array;

	switch (value->size)
	  {
	  case 1: glUniform1iv (location, value->count, ptr); break;
	  case 2: glUniform2iv (location, value->count, ptr); break;
	  case 3: glUniform3iv (location, value->count, ptr); break;
	  case 4: glUniform4iv (location, value->count, ptr); break;
	  }
      }
      break;

    case COGL_BOXED_FLOAT:
      {
	float *ptr;

	if (value->count == 1)
	  ptr = value->v.float_value;
	else
	  ptr = value->v.float_array;

	switch (value->size)
	  {
	  case 1: glUniform1fv (location, value->count, ptr); break;
	  case 2: glUniform2fv (location, value->count, ptr); break;
	  case 3: glUniform3fv (location, value->count, ptr); break;
	  case 4: glUniform4fv (location, value->count, ptr); break;
	  }
      }
      break;

    case COGL_BOXED_MATRIX:
      {
	float *ptr;

	if (value->count == 1)
	  ptr = value->v.matrix;
	else
	  ptr = value->v.float_array;

	switch (value->size)
	  {
	  case 2:
	    glUniformMatrix2fv (location, value->count, value->transpose, ptr);
	    break;
	  case 3:
	    glUniformMatrix3fv (location, value->count, value->transpose, ptr);
	    break;
	  case 4:
	    glUniformMatrix4fv (location, value->count, value->transpose, ptr);
	    break;
	  }
      }
      break;
    }
}

static void
cogl_wrap_prepare_for_draw (void)
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

      if ((w->dirty_uniforms & COGL_GLES2_DIRTY_TEXTURE_UNITS))
        {
          int i;

          /* TODO - we should probably have a per unit dirty flag too */

          for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
            {
              GLint uniform = program->uniforms.texture_sampler_uniforms[i];

              if (uniform != -1)
                glUniform1i (uniform, i);
            }
        }

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
		  cogl_gles2_do_set_uniform (program->custom_uniforms[i],
					     &w->custom_uniforms[i]);
	      }
	}

      w->dirty_custom_uniforms = 0;
    }

  if (w->dirty_attribute_pointers
      & COGL_GLES2_DIRTY_TEX_COORD_VERTEX_ATTRIB)
    {
      int i;

      /* TODO - coverage test */
      for (i = 0; i < COGL_GLES2_MAX_TEXTURE_UNITS; i++)
        if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (w->settings.texture_units, i))
          {
            GLint tex_coord_var_index;
            CoglGles2WrapperTextureUnit *texture_unit;

            texture_unit = w->texture_units + w->active_texture_unit;
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
	  CoglGles2WrapperTextureUnit *texture_unit
            = w->texture_units + w->active_texture_unit;
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
cogl_wrap_glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
  cogl_wrap_prepare_for_draw ();

  glDrawArrays (mode, first, count);
}

void
cogl_wrap_glDrawElements (GLenum mode, GLsizei count, GLenum type,
                          const GLvoid *indices)
{
  cogl_wrap_prepare_for_draw ();

  glDrawElements (mode, count, type, indices);
}

void
cogl_wrap_glTexEnvi (GLenum target, GLenum pname, GLint param)
{
  if (target == GL_TEXTURE_ENV)
    {
      CoglGles2WrapperTexEnv *tex_env;

      _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

      tex_env = w->settings.tex_env + w->active_texture_unit;

      switch (pname)
        {
        case GL_COMBINE_RGB:
          tex_env->texture_combine_rgb_func = param;
          break;
        case GL_COMBINE_ALPHA:
          tex_env->texture_combine_alpha_func = param;
          break;
        case GL_SRC0_RGB:
        case GL_SRC1_RGB:
        case GL_SRC2_RGB:
          tex_env->texture_combine_rgb_src[pname - GL_SRC0_RGB] = param;
          break;
        case GL_SRC0_ALPHA:
        case GL_SRC1_ALPHA:
        case GL_SRC2_ALPHA:
          tex_env->texture_combine_alpha_src[pname - GL_SRC0_ALPHA] = param;
          break;
        case GL_OPERAND0_RGB:
        case GL_OPERAND1_RGB:
        case GL_OPERAND2_RGB:
          tex_env->texture_combine_rgb_op[pname - GL_OPERAND0_RGB] = param;
          break;
        case GL_OPERAND0_ALPHA:
        case GL_OPERAND1_ALPHA:
        case GL_OPERAND2_ALPHA:
          tex_env->texture_combine_alpha_op[pname - GL_OPERAND0_ALPHA] = param;
          break;
        }

      w->settings_dirty = TRUE;
    }
}

void
cogl_wrap_glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params)
{
  if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_COLOR)
    {
      CoglGles2WrapperTexEnv *tex_env;

      _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

      tex_env = w->settings.tex_env + w->active_texture_unit;

      memcpy (tex_env->texture_combine_constant, params, sizeof (GLfloat) * 4);
    }
}

void
cogl_wrap_glClientActiveTexture (GLenum texture)
{
  int texture_unit_index = texture - GL_TEXTURE0;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  if (texture_unit_index < COGL_GLES2_MAX_TEXTURE_UNITS)
    w->active_client_texture_unit = texture_unit_index;
}

void
cogl_wrap_glActiveTexture (GLenum texture)
{
  int texture_unit_index = texture - GL_TEXTURE0;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  glActiveTexture (texture);

  if (texture_unit_index < COGL_GLES2_MAX_TEXTURE_UNITS)
    w->active_texture_unit = texture_unit_index;
}

void
cogl_wrap_glEnable (GLenum cap)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (cap)
    {
    case GL_TEXTURE_2D:
      if (!COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (w->settings.texture_units,
                                               w->active_texture_unit))
        {
          COGL_GLES2_TEXTURE_UNIT_SET_ENABLED (w->settings.texture_units,
                                               w->active_texture_unit,
                                               TRUE);
          w->settings_dirty = TRUE;
        }
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
      if (COGL_GLES2_TEXTURE_UNIT_IS_ENABLED (w->settings.texture_units,
                                              w->active_texture_unit))
        {
          COGL_GLES2_TEXTURE_UNIT_SET_ENABLED (w->settings.texture_units,
                                               w->active_texture_unit,
                                               FALSE);
          w->settings_dirty = TRUE;
        }
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
  CoglGles2WrapperTextureUnit *texture_unit;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (array)
    {
    case GL_VERTEX_ARRAY:
      glEnableVertexAttribArray (COGL_GLES2_WRAPPER_VERTEX_ATTRIB);
      break;
    case GL_TEXTURE_COORD_ARRAY:
      /* TODO - review if this should be in w->settings? */

      texture_unit = w->texture_units + w->active_texture_unit;
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
cogl_wrap_glDisableClientState (GLenum array)
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
cogl_wrap_glColor4f (GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
  glVertexAttrib4f (COGL_GLES2_WRAPPER_COLOR_ATTRIB, r, g, b, a);
}

void
cogl_wrap_glColor4ub (GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
  glVertexAttrib4f (COGL_GLES2_WRAPPER_COLOR_ATTRIB,
                    r/255.0, g/255.0, b/255.0, a/255.0);
}

void
cogl_wrap_glClipPlanef (GLenum plane, GLfloat *equation)
{
  /* FIXME */
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
cogl_wrap_glGetFloatv (GLenum pname, GLfloat *params)
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
cogl_wrap_glFogf (GLenum pname, GLfloat param)
{
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  switch (pname)
    {
    case GL_FOG_MODE:
      _COGL_GLES2_CHANGE_SETTING (w, fog_mode, param);
      break;

    case GL_FOG_DENSITY:
      _COGL_GLES2_CHANGE_UNIFORM (w, FOG_DENSITY, fog_density,
				   (param));
      break;

    case GL_FOG_START:
      _COGL_GLES2_CHANGE_UNIFORM (w, FOG_START, fog_start,
				   (param));
      break;

    case GL_FOG_END:
      _COGL_GLES2_CHANGE_UNIFORM (w, FOG_END, fog_end,
				   (param));
      break;
    }
}

void
cogl_wrap_glFogfv (GLenum pname, const GLfloat *params)
{
  int i;
  _COGL_GET_GLES2_WRAPPER (w, NO_RETVAL);

  if (pname == GL_FOG_COLOR)
    {
      for (i = 0; i < 4; i++)
	w->fog_color[i] =  (params[i]);

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
cogl_wrap_glMaterialfv (GLenum face, GLenum pname, const GLfloat *params)
{
  /* FIXME: the GLES 2 backend doesn't yet support lighting so this
     function can't do anything */
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
