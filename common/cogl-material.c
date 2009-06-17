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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#include "cogl-material-private.h"
#include "cogl-texture-private.h"
#include "cogl-blend-string.h"

#include <glib.h>
#include <string.h>

/*
 * GL/GLES compatability defines for material thingies:
 */

#ifdef HAVE_COGL_GLES2
#include "../gles/cogl-gles2-wrapper.h"
#endif

#ifdef HAVE_COGL_GL
#define glActiveTexture ctx->pf_glActiveTexture
#define glClientActiveTexture ctx->pf_glClientActiveTexture
#define glBlendFuncSeparate ctx->pf_glBlendFuncSeparate
#define glBlendEquation ctx->pf_glBlendEquation
#define glBlendColor ctx->pf_glBlendColor
#define glBlendEquationSeparate ctx->pf_glBlendEquationSeparate
#endif

static void _cogl_material_free (CoglMaterial *tex);
static void _cogl_material_layer_free (CoglMaterialLayer *layer);

COGL_HANDLE_DEFINE (Material, material);
COGL_HANDLE_DEFINE (MaterialLayer, material_layer);

/* #define DISABLE_MATERIAL_CACHE 1 */

GQuark
_cogl_material_error_quark (void)
{
  return g_quark_from_static_string ("cogl-material-error-quark");
}

CoglHandle
cogl_material_new (void)
{
  /* Create new - blank - material */
  CoglMaterial *material = g_new0 (CoglMaterial, 1);
  GLfloat *unlit = material->unlit;
  GLfloat *ambient = material->ambient;
  GLfloat *diffuse = material->diffuse;
  GLfloat *specular = material->specular;
  GLfloat *emission = material->emission;

  /* Use the same defaults as the GL spec... */
  unlit[0] = 1.0; unlit[1] = 1.0; unlit[2] = 1.0; unlit[3] = 1.0;
  material->flags |= COGL_MATERIAL_FLAG_DEFAULT_COLOR;

  /* Use the same defaults as the GL spec... */
  ambient[0] = 0.2; ambient[1] = 0.2; ambient[2] = 0.2; ambient[3] = 1.0;
  diffuse[0] = 0.8; diffuse[1] = 0.8; diffuse[2] = 0.8; diffuse[3] = 1.0;
  specular[0] = 0; specular[1] = 0; specular[2] = 0; specular[3] = 1.0;
  emission[0] = 0; emission[1] = 0; emission[2] = 0; emission[3] = 1.0;
  material->flags |= COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;

  /* Use the same defaults as the GL spec... */
  material->alpha_func = COGL_MATERIAL_ALPHA_FUNC_ALWAYS;
  material->alpha_func_reference = 0.0;
  material->flags |= COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC;

  /* Not the same as the GL default, but seems saner... */
#ifndef HAVE_COGL_GLES
  material->blend_equation_rgb = GL_FUNC_ADD;
  material->blend_equation_alpha = GL_FUNC_ADD;
  material->blend_src_factor_alpha = GL_SRC_ALPHA;
  material->blend_dst_factor_alpha = GL_ONE_MINUS_SRC_ALPHA;
  material->blend_constant[0] = 0;
  material->blend_constant[1] = 0;
  material->blend_constant[2] = 0;
  material->blend_constant[3] = 0;
#endif
  material->blend_src_factor_rgb = GL_ONE;
  material->blend_dst_factor_rgb = GL_ONE_MINUS_SRC_ALPHA;
  material->flags |= COGL_MATERIAL_FLAG_DEFAULT_BLEND;

  material->layers = NULL;
  material->n_layers = 0;

  return _cogl_material_handle_new (material);
}

static void
_cogl_material_free (CoglMaterial *material)
{
  /* Frees material resources but its handle is not
     released! Do that separately before this! */

  g_list_foreach (material->layers,
		  (GFunc)cogl_handle_unref, NULL);
  g_list_free (material->layers);
  g_free (material);
}


static void
handle_automatic_blend_enable (CoglMaterial *material)
{
  GList *tmp;

  /* XXX: If we expose manual control over ENABLE_BLEND, we'll add
   * a flag to know when it's user configured, so we don't trash it */

  material->flags &= ~COGL_MATERIAL_FLAG_ENABLE_BLEND;

  /* XXX: Uncomment this to disable all blending */
#if 0
  return;
#endif

  for (tmp = material->layers; tmp != NULL; tmp = tmp->next)
    {
      CoglMaterialLayer *layer = tmp->data;

      /* NB: A layer may have a combine mode set on it but not yet have an
       * associated texture. */
      if (!layer->texture)
        continue;

      if (cogl_texture_get_format (layer->texture) & COGL_A_BIT)
	material->flags |= COGL_MATERIAL_FLAG_ENABLE_BLEND;
    }

  if (material->unlit[3] != 1.0)
    material->flags |= COGL_MATERIAL_FLAG_ENABLE_BLEND;
}

/* If primitives have been logged in the journal referencing the current
 * state of this material we need to flush the journal before we can
 * modify it... */
static void
_cogl_material_pre_change_notify (CoglMaterial *material)
{
  if (material->journal_ref_count)
    _cogl_journal_flush ();
}

void
cogl_material_get_color (CoglHandle  handle,
                         CoglColor  *color)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  cogl_color_set_from_4f (color,
                          material->unlit[0],
                          material->unlit[1],
                          material->unlit[2],
                          material->unlit[3]);
}

void
cogl_material_set_color (CoglHandle       handle,
			 const CoglColor *unlit_color)
{
  CoglMaterial *material;
  GLfloat       unlit[4];

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  unlit[0] = cogl_color_get_red_float (unlit_color);
  unlit[1] = cogl_color_get_green_float (unlit_color);
  unlit[2] = cogl_color_get_blue_float (unlit_color);
  unlit[3] = cogl_color_get_alpha_float (unlit_color);
  if (memcmp (unlit, material->unlit, sizeof (unlit)) == 0)
    return;

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  memcpy (material->unlit, unlit, sizeof (unlit));

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_COLOR;
  if (unlit[0] == 1.0 &&
      unlit[1] == 1.0 &&
      unlit[2] == 1.0 &&
      unlit[3] == 1.0)
    material->flags |= COGL_MATERIAL_FLAG_DEFAULT_COLOR;

  handle_automatic_blend_enable (material);
}

void
cogl_material_set_color4ub (CoglHandle handle,
			    guint8 red,
                            guint8 green,
                            guint8 blue,
                            guint8 alpha)
{
  CoglColor color;
  cogl_color_set_from_4ub (&color, red, green, blue, alpha);
  cogl_material_set_color (handle, &color);
}

void
cogl_material_set_color4f (CoglHandle handle,
			   float red,
                           float green,
                           float blue,
                           float alpha)
{
  CoglColor color;
  cogl_color_set_from_4f (&color, red, green, blue, alpha);
  cogl_material_set_color (handle, &color);
}

void
cogl_material_get_ambient (CoglHandle  handle,
                           CoglColor  *ambient)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  cogl_color_set_from_4f (ambient,
                          material->ambient[0],
                          material->ambient[1],
                          material->ambient[2],
                          material->ambient[3]);
}

void
cogl_material_set_ambient (CoglHandle handle,
			   const CoglColor *ambient_color)
{
  CoglMaterial *material;
  GLfloat      *ambient;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  ambient = material->ambient;
  ambient[0] = cogl_color_get_red_float (ambient_color);
  ambient[1] = cogl_color_get_green_float (ambient_color);
  ambient[2] = cogl_color_get_blue_float (ambient_color);
  ambient[3] = cogl_color_get_alpha_float (ambient_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;
}

void
cogl_material_get_diffuse (CoglHandle  handle,
                           CoglColor  *diffuse)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  cogl_color_set_from_4f (diffuse,
                          material->diffuse[0],
                          material->diffuse[1],
                          material->diffuse[2],
                          material->diffuse[3]);
}

void
cogl_material_set_diffuse (CoglHandle handle,
			   const CoglColor *diffuse_color)
{
  CoglMaterial *material;
  GLfloat      *diffuse;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  diffuse = material->diffuse;
  diffuse[0] = cogl_color_get_red_float (diffuse_color);
  diffuse[1] = cogl_color_get_green_float (diffuse_color);
  diffuse[2] = cogl_color_get_blue_float (diffuse_color);
  diffuse[3] = cogl_color_get_alpha_float (diffuse_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;
}

void
cogl_material_set_ambient_and_diffuse (CoglHandle handle,
				       const CoglColor *color)
{
  cogl_material_set_ambient (handle, color);
  cogl_material_set_diffuse (handle, color);
}

void
cogl_material_get_specular (CoglHandle  handle,
                            CoglColor  *specular)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  cogl_color_set_from_4f (specular,
                          material->specular[0],
                          material->specular[1],
                          material->specular[2],
                          material->specular[3]);
}

void
cogl_material_set_specular (CoglHandle handle,
			    const CoglColor *specular_color)
{
  CoglMaterial *material;
  GLfloat      *specular;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  specular = material->specular;
  specular[0] = cogl_color_get_red_float (specular_color);
  specular[1] = cogl_color_get_green_float (specular_color);
  specular[2] = cogl_color_get_blue_float (specular_color);
  specular[3] = cogl_color_get_alpha_float (specular_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;
}

float
cogl_material_get_shininess (CoglHandle handle)
{
  CoglMaterial *material;

  g_return_val_if_fail (cogl_is_material (handle), 0);

  material = _cogl_material_pointer_from_handle (handle);

  return material->shininess;
}

void
cogl_material_set_shininess (CoglHandle handle,
			     float shininess)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  if (shininess < 0.0 || shininess > 1.0)
    g_warning ("Out of range shininess %f supplied for material\n",
	       shininess);

  material = _cogl_material_pointer_from_handle (handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  material->shininess = (GLfloat)shininess * 128.0;

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;
}

void
cogl_material_get_emission (CoglHandle  handle,
                            CoglColor  *emission)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  cogl_color_set_from_4f (emission,
                          material->emission[0],
                          material->emission[1],
                          material->emission[2],
                          material->emission[3]);
}

void
cogl_material_set_emission (CoglHandle handle,
			    const CoglColor *emission_color)
{
  CoglMaterial *material;
  GLfloat      *emission;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  emission = material->emission;
  emission[0] = cogl_color_get_red_float (emission_color);
  emission[1] = cogl_color_get_green_float (emission_color);
  emission[2] = cogl_color_get_blue_float (emission_color);
  emission[3] = cogl_color_get_alpha_float (emission_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;
}

void
cogl_material_set_alpha_test_function (CoglHandle handle,
				       CoglMaterialAlphaFunc alpha_func,
				       float alpha_reference)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  material->alpha_func = alpha_func;
  material->alpha_func_reference = (GLfloat)alpha_reference;

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC;
}

GLenum
arg_to_gl_blend_factor (CoglBlendStringArgument *arg)
{
  if (arg->source.is_zero)
    return GL_ZERO;
  if (arg->factor.is_one)
    return GL_ONE;
  else if (arg->factor.is_src_alpha_saturate)
    return GL_SRC_ALPHA_SATURATE;
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_SRC_COLOR)
    {
      if (arg->factor.source.mask == COGL_BLEND_STRING_CHANNEL_MASK_RGB)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_SRC_COLOR;
          else
            return GL_SRC_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_SRC_ALPHA;
          else
            return GL_SRC_ALPHA;
        }
    }
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_DST_COLOR)
    {
      if (arg->factor.source.mask == COGL_BLEND_STRING_CHANNEL_MASK_RGB)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_DST_COLOR;
          else
            return GL_DST_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_DST_ALPHA;
          else
            return GL_DST_ALPHA;
        }
    }
#ifndef HAVE_COGL_GLES
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_CONSTANT)
    {
      if (arg->factor.source.mask == COGL_BLEND_STRING_CHANNEL_MASK_RGB)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_CONSTANT_COLOR;
          else
            return GL_CONSTANT_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_CONSTANT_ALPHA;
          else
            return GL_CONSTANT_ALPHA;
        }
    }
#endif

  g_warning ("Unable to determine valid blend factor from blend string\n");
  return GL_ONE;
}

void
setup_blend_state (CoglBlendStringStatement *statement,
                   GLenum *blend_equation,
                   GLint *blend_src_factor,
                   GLint *blend_dst_factor)
{
#ifndef HAVE_COGL_GLES
  switch (statement->function->type)
    {
    case COGL_BLEND_STRING_FUNCTION_ADD:
      *blend_equation = GL_FUNC_ADD;
      break;
    /* TODO - add more */
    default:
      g_warning ("Unsupported blend function given");
      *blend_equation = GL_FUNC_ADD;
    }
#endif

  *blend_src_factor = arg_to_gl_blend_factor (&statement->args[0]);
  *blend_dst_factor = arg_to_gl_blend_factor (&statement->args[1]);
}

gboolean
cogl_material_set_blend (CoglHandle handle,
                         const char *blend_description,
                         GError **error)
{
  CoglMaterial *material;
  CoglBlendStringStatement statements[2];
  CoglBlendStringStatement split[2];
  CoglBlendStringStatement *rgb;
  CoglBlendStringStatement *a;
  GError *internal_error = NULL;
  int count;

  g_return_val_if_fail (cogl_is_material (handle), FALSE);

  material = _cogl_material_pointer_from_handle (handle);

  count =
    _cogl_blend_string_compile (blend_description,
                                COGL_BLEND_STRING_CONTEXT_BLENDING,
                                statements,
                                &internal_error);
  if (!count)
    {
      if (error)
	g_propagate_error (error, internal_error);
      else
	{
	  g_warning ("Cannot compile blend description: %s\n",
		     internal_error->message);
	  g_error_free (internal_error);
	}
      return FALSE;
    }

  if (statements[0].mask == COGL_BLEND_STRING_CHANNEL_MASK_RGBA)
    {
      _cogl_blend_string_split_rgba_statement (statements,
                                               &split[0], &split[1]);
      rgb = &split[0];
      a = &split[1];
    }
  else
    {
      rgb = &statements[0];
      a = &statements[1];
    }

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

#ifndef HAVE_COGL_GLES
  setup_blend_state (rgb,
                     &material->blend_equation_rgb,
                     &material->blend_src_factor_rgb,
                     &material->blend_dst_factor_rgb);
  setup_blend_state (a,
                     &material->blend_equation_alpha,
                     &material->blend_src_factor_alpha,
                     &material->blend_dst_factor_alpha);
#else
  setup_blend_state (rgb,
                     NULL,
                     &material->blend_src_factor_rgb,
                     &material->blend_dst_factor_rgb);
#endif

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_BLEND;

  return TRUE;
}

void
cogl_material_set_blend_constant (CoglHandle handle,
                                  CoglColor *constant_color)
{
#ifndef HAVE_COGL_GLES
  CoglMaterial *material;
  GLfloat      *constant;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  constant = material->blend_constant;
  constant[0] = cogl_color_get_red_float (constant_color);
  constant[1] = cogl_color_get_green_float (constant_color);
  constant[2] = cogl_color_get_blue_float (constant_color);
  constant[3] = cogl_color_get_alpha_float (constant_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_BLEND;
#endif
}

/* Asserts that a layer corresponding to the given index exists. If no
 * match is found, then a new empty layer is added.
 */
static CoglMaterialLayer *
_cogl_material_get_layer (CoglMaterial *material,
			  gint          index_,
			  gboolean      create_if_not_found)
{
  CoglMaterialLayer *layer;
  GList		    *tmp;
  CoglHandle	     layer_handle;

  for (tmp = material->layers; tmp != NULL; tmp = tmp->next)
    {
      layer =
	_cogl_material_layer_pointer_from_handle ((CoglHandle)tmp->data);
      if (layer->index == index_)
	return layer;

      /* The layers are always sorted, so at this point we know this layer
       * doesn't exist */
      if (layer->index > index_)
	break;
    }
  /* NB: if we now insert a new layer before tmp, that will maintain order.
   */

  if (!create_if_not_found)
    return NULL;

  layer = g_new0 (CoglMaterialLayer, 1);

  layer_handle = _cogl_material_layer_handle_new (layer);
  layer->index = index_;
  layer->flags = COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE;
  layer->mag_filter = COGL_MATERIAL_FILTER_LINEAR;
  layer->min_filter = COGL_MATERIAL_FILTER_LINEAR;
  layer->texture = COGL_INVALID_HANDLE;

  /* Choose the same default combine mode as OpenGL:
   * MODULATE(PREVIOUS[RGBA],TEXTURE[RGBA]) */
  layer->texture_combine_rgb_func = GL_MODULATE;
  layer->texture_combine_rgb_src[0] = GL_PREVIOUS;
  layer->texture_combine_rgb_src[1] = GL_TEXTURE;
  layer->texture_combine_rgb_op[0] = GL_SRC_COLOR;
  layer->texture_combine_rgb_op[1] = GL_SRC_COLOR;
  layer->texture_combine_alpha_func = GL_MODULATE;
  layer->texture_combine_alpha_src[0] = GL_PREVIOUS;
  layer->texture_combine_alpha_src[1] = GL_TEXTURE;
  layer->texture_combine_alpha_op[0] = GL_SRC_ALPHA;
  layer->texture_combine_alpha_op[1] = GL_SRC_ALPHA;

  cogl_matrix_init_identity (&layer->matrix);

  /* Note: see comment after for() loop above */
  material->layers =
    g_list_insert_before (material->layers, tmp, layer_handle);

  return layer;
}

void
cogl_material_set_layer (CoglHandle material_handle,
			 gint layer_index,
			 CoglHandle texture_handle)
{
  CoglMaterial	    *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (material_handle));
  g_return_if_fail (texture_handle == COGL_INVALID_HANDLE
                    || cogl_is_texture (texture_handle));

  material = _cogl_material_pointer_from_handle (material_handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  layer = _cogl_material_get_layer (material_handle, layer_index, TRUE);

  if (texture_handle == layer->texture)
    return;

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  material->n_layers = g_list_length (material->layers);
  if (material->n_layers >= CGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
    {
      if (!(material->flags & COGL_MATERIAL_FLAG_SHOWN_SAMPLER_WARNING))
	{
	  g_warning ("Your hardware does not have enough texture samplers"
		     "to handle this many texture layers");
	  material->flags |= COGL_MATERIAL_FLAG_SHOWN_SAMPLER_WARNING;
	}
      /* Note: We always make a best effort attempt to display as many
       * layers as possible, so this isn't an _error_ */
      /* Note: in the future we may support enabling/disabling layers
       * too, so it may become valid to add more than
       * MAX_COMBINED_TEXTURE_IMAGE_UNITS layers. */
    }

  if (texture_handle)
    cogl_handle_ref (texture_handle);

  if (layer->texture)
    cogl_handle_unref (layer->texture);

  layer->texture = texture_handle;

  handle_automatic_blend_enable (material);
  layer->flags |= COGL_MATERIAL_LAYER_FLAG_DIRTY;
}

static void
setup_texture_combine_state (CoglBlendStringStatement *statement,
                             GLint *texture_combine_func,
                             GLint *texture_combine_src,
                             GLint *texture_combine_op)
{
  int i;

  switch (statement->function->type)
    {
    case COGL_BLEND_STRING_FUNCTION_AUTO_COMPOSITE:
      *texture_combine_func = GL_MODULATE; /* FIXME */
      break;
    case COGL_BLEND_STRING_FUNCTION_REPLACE:
      *texture_combine_func = GL_REPLACE;
      break;
    case COGL_BLEND_STRING_FUNCTION_MODULATE:
      *texture_combine_func = GL_MODULATE;
      break;
    case COGL_BLEND_STRING_FUNCTION_ADD:
      *texture_combine_func = GL_ADD;
      break;
    case COGL_BLEND_STRING_FUNCTION_ADD_SIGNED:
      *texture_combine_func = GL_ADD_SIGNED;
      break;
    case COGL_BLEND_STRING_FUNCTION_INTERPOLATE:
      *texture_combine_func = GL_INTERPOLATE;
      break;
    case COGL_BLEND_STRING_FUNCTION_SUBTRACT:
      *texture_combine_func = GL_SUBTRACT;
      break;
    case COGL_BLEND_STRING_FUNCTION_DOT3_RGB:
      *texture_combine_func = GL_DOT3_RGB;
      break;
    case COGL_BLEND_STRING_FUNCTION_DOT3_RGBA:
      *texture_combine_func = GL_DOT3_RGBA;
      break;
    }

  for (i = 0; i < statement->function->argc; i++)
    {
      CoglBlendStringArgument *arg = &statement->args[i];

      switch (arg->source.info->type)
        {
        case COGL_BLEND_STRING_COLOR_SOURCE_CONSTANT:
          texture_combine_src[i] = GL_CONSTANT;
          break;
        case COGL_BLEND_STRING_COLOR_SOURCE_TEXTURE:
          texture_combine_src[i] = GL_TEXTURE;
          break;
        case COGL_BLEND_STRING_COLOR_SOURCE_TEXTURE_N:
          texture_combine_src[i] =
            GL_TEXTURE0 + arg->source.texture;
          break;
        case COGL_BLEND_STRING_COLOR_SOURCE_PRIMARY:
          texture_combine_src[i] = GL_PRIMARY_COLOR;
          break;
        case COGL_BLEND_STRING_COLOR_SOURCE_PREVIOUS:
          texture_combine_src[i] = GL_PREVIOUS;
          break;
        default:
          g_warning ("Unexpected texture combine source");
          texture_combine_src[i] = GL_TEXTURE;
        }

      if (arg->source.mask == COGL_BLEND_STRING_CHANNEL_MASK_RGB)
        {
          if (statement->args[i].source.one_minus)
            texture_combine_op[i] = GL_ONE_MINUS_SRC_COLOR;
          else
            texture_combine_op[i] = GL_SRC_COLOR;
        }
      else
        {
          if (statement->args[i].source.one_minus)
            texture_combine_op[i] = GL_ONE_MINUS_SRC_ALPHA;
          else
            texture_combine_op[i] = GL_SRC_ALPHA;
        }
    }
}

gboolean
cogl_material_set_layer_combine (CoglHandle handle,
				 gint layer_index,
				 const char *combine_description,
                                 GError **error)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;
  CoglBlendStringStatement statements[2];
  CoglBlendStringStatement split[2];
  CoglBlendStringStatement *rgb;
  CoglBlendStringStatement *a;
  GError *internal_error = NULL;
  int count;

  g_return_val_if_fail (cogl_is_material (handle), FALSE);

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  count =
    _cogl_blend_string_compile (combine_description,
                                COGL_BLEND_STRING_CONTEXT_TEXTURE_COMBINE,
                                statements,
                                &internal_error);
  if (!count)
    {
      if (error)
	g_propagate_error (error, internal_error);
      else
	{
	  g_warning ("Cannot compile combine description: %s\n",
		     internal_error->message);
	  g_error_free (internal_error);
	}
      return FALSE;
    }

  if (statements[0].mask == COGL_BLEND_STRING_CHANNEL_MASK_RGBA)
    {
      _cogl_blend_string_split_rgba_statement (statements,
                                               &split[0], &split[1]);
      rgb = &split[0];
      a = &split[1];
    }
  else
    {
      rgb = &statements[0];
      a = &statements[1];
    }

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  setup_texture_combine_state (rgb,
                               &layer->texture_combine_rgb_func,
                               layer->texture_combine_rgb_src,
                               layer->texture_combine_rgb_op);

  setup_texture_combine_state (a,
                               &layer->texture_combine_alpha_func,
                               layer->texture_combine_alpha_src,
                               layer->texture_combine_alpha_op);


  layer->flags |= COGL_MATERIAL_LAYER_FLAG_DIRTY;
  layer->flags &= ~COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE;
  return TRUE;
}

void
cogl_material_set_layer_combine_constant (CoglHandle handle,
				          gint layer_index,
                                          CoglColor *constant_color)
{
  CoglMaterial      *material;
  CoglMaterialLayer *layer;
  GLfloat           *constant;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  constant = layer->texture_combine_constant;
  constant[0] = cogl_color_get_red_float (constant_color);
  constant[1] = cogl_color_get_green_float (constant_color);
  constant[2] = cogl_color_get_blue_float (constant_color);
  constant[3] = cogl_color_get_alpha_float (constant_color);

  layer->flags |= COGL_MATERIAL_LAYER_FLAG_DIRTY;
  layer->flags &= ~COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE;
}

void
cogl_material_set_layer_matrix (CoglHandle material_handle,
				gint layer_index,
				CoglMatrix *matrix)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (material_handle));

  material = _cogl_material_pointer_from_handle (material_handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  layer->matrix = *matrix;

  layer->flags |= COGL_MATERIAL_LAYER_FLAG_DIRTY;
  layer->flags |= COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX;
  layer->flags &= ~COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE;
}

static void
_cogl_material_layer_free (CoglMaterialLayer *layer)
{
  if (layer->texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (layer->texture);
  g_free (layer);
}

void
cogl_material_remove_layer (CoglHandle material_handle,
			    gint layer_index)
{
  CoglMaterial	     *material;
  CoglMaterialLayer  *layer;
  GList		     *tmp;

  g_return_if_fail (cogl_is_material (material_handle));

  material = _cogl_material_pointer_from_handle (material_handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  for (tmp = material->layers; tmp != NULL; tmp = tmp->next)
    {
      layer = tmp->data;
      if (layer->index == layer_index)
	{
	  CoglHandle handle = (CoglHandle) layer;
	  cogl_handle_unref (handle);
	  material->layers = g_list_remove (material->layers, layer);
          material->n_layers--;
	  break;
	}
    }

  handle_automatic_blend_enable (material);
}

/* XXX: This API is hopfully just a stop-gap solution. Ideally cogl_enable
 * will be replaced. */
gulong
_cogl_material_get_cogl_enable_flags (CoglHandle material_handle)
{
  CoglMaterial	*material;
  gulong	 enable_flags = 0;

  _COGL_GET_CONTEXT (ctx, 0);

  g_return_val_if_fail (cogl_is_material (material_handle), 0);

  material = _cogl_material_pointer_from_handle (material_handle);

  /* Enable blending if the geometry has an associated alpha color,
   * or the material wants blending enabled. */
  if (material->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND)
    enable_flags |= COGL_ENABLE_BLEND;

  return enable_flags;
}

/* It's a bit out of the ordinary to return a const GList *, but it's
 * probably sensible to try and avoid list manipulation for every
 * primitive emitted in a scene, every frame.
 *
 * Alternatively; we could either add a _foreach function, or maybe
 * a function that gets a passed a buffer (that may be stack allocated)
 * by the caller.
 */
const GList *
cogl_material_get_layers (CoglHandle material_handle)
{
  CoglMaterial	*material;

  g_return_val_if_fail (cogl_is_material (material_handle), NULL);

  material = _cogl_material_pointer_from_handle (material_handle);

  return material->layers;
}

int
cogl_material_get_n_layers (CoglHandle material_handle)
{
  CoglMaterial *material;

  g_return_val_if_fail (cogl_is_material (material_handle), 0);

  material = _cogl_material_pointer_from_handle (material_handle);

  return material->n_layers;
}

CoglMaterialLayerType
cogl_material_layer_get_type (CoglHandle layer_handle)
{
  return COGL_MATERIAL_LAYER_TYPE_TEXTURE;
}

CoglHandle
cogl_material_layer_get_texture (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer;

  g_return_val_if_fail (cogl_is_material_layer (layer_handle),
			COGL_INVALID_HANDLE);

  layer = _cogl_material_layer_pointer_from_handle (layer_handle);
  return layer->texture;
}

gulong
_cogl_material_layer_get_flags (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer;

  g_return_val_if_fail (cogl_is_material_layer (layer_handle), 0);

  layer = _cogl_material_layer_pointer_from_handle (layer_handle);

  return layer->flags & COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX;
}

static guint
get_n_args_for_combine_func (GLint func)
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
    }
  return 0;
}

static gboolean
is_mipmap_filter (CoglMaterialFilter filter)
{
  return (filter == COGL_MATERIAL_FILTER_NEAREST_MIPMAP_NEAREST
          || filter == COGL_MATERIAL_FILTER_LINEAR_MIPMAP_NEAREST
          || filter == COGL_MATERIAL_FILTER_NEAREST_MIPMAP_LINEAR
          || filter == COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR);
}

static void
_cogl_material_layer_flush_gl_sampler_state (CoglMaterialLayer  *layer,
                                             CoglLayerInfo      *gl_layer_info)
{
  int n_rgb_func_args;
  int n_alpha_func_args;

#ifndef DISABLE_MATERIAL_CACHE
  if (!(gl_layer_info &&
        gl_layer_info->flags & COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE &&
        layer->flags & COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE))
#endif
    {
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE));

      /* Set the combiner functions... */
      GE (glTexEnvi (GL_TEXTURE_ENV,
                     GL_COMBINE_RGB,
                     layer->texture_combine_rgb_func));
      GE (glTexEnvi (GL_TEXTURE_ENV,
                     GL_COMBINE_ALPHA,
                     layer->texture_combine_alpha_func));

      /*
       * Setup the function arguments...
       */

      /* For the RGB components... */
      n_rgb_func_args =
        get_n_args_for_combine_func (layer->texture_combine_rgb_func);

      GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB,
                     layer->texture_combine_rgb_src[0]));
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB,
                     layer->texture_combine_rgb_op[0]));
      if (n_rgb_func_args > 1)
        {
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB,
                         layer->texture_combine_rgb_src[1]));
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB,
                         layer->texture_combine_rgb_op[1]));
        }
      if (n_rgb_func_args > 2)
        {
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC2_RGB,
                         layer->texture_combine_rgb_src[2]));
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_RGB,
                         layer->texture_combine_rgb_op[2]));
        }

      /* For the Alpha component */
      n_alpha_func_args =
        get_n_args_for_combine_func (layer->texture_combine_alpha_func);

      GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA,
                     layer->texture_combine_alpha_src[0]));
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,
                     layer->texture_combine_alpha_op[0]));
      if (n_alpha_func_args > 1)
        {
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA,
                         layer->texture_combine_alpha_src[1]));
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA,
                         layer->texture_combine_alpha_op[1]));
        }
      if (n_alpha_func_args > 2)
        {
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC2_ALPHA,
                         layer->texture_combine_alpha_src[2]));
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_ALPHA,
                         layer->texture_combine_alpha_op[2]));
        }

      GE (glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
                      layer->texture_combine_constant));
    }

#ifndef DISABLE_MATERIAL_CACHE
  if (gl_layer_info &&
      (gl_layer_info->flags & COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX ||
       layer->flags & COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX))
#endif
    {
      _cogl_set_current_matrix (COGL_MATRIX_TEXTURE);
      _cogl_current_matrix_load (&layer->matrix);
      _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
    }
}

/*
 * _cogl_material_flush_layers_gl_state:
 * @fallback_mask: is a bitmask of the material layers that need to be
 *    replaced with the default, fallback textures. The fallback textures are
 *    fully transparent textures so they hopefully wont contribute to the
 *    texture combining.
 *
 *    The intention of fallbacks is to try and preserve
 *    the number of layers the user is expecting so that texture coordinates
 *    they gave will mostly still correspond to the textures they intended, and
 *    have a fighting chance of looking close to their originally intended
 *    result.
 *
 * @disable_mask: is a bitmask of the material layers that will simply have
 *    texturing disabled. It's only really intended for disabling all layers
 *    > X; i.e. we'd expect to see a contiguous run of 0 starting from the LSB
 *    and at some point the remaining bits flip to 1. It might work to disable
 *    arbitrary layers; though I'm not sure a.t.m how OpenGL would take to
 *    that.
 *
 *    The intention of the disable_mask is for emitting geometry when the user
 *    hasn't supplied enough texture coordinates for all the layers and it's
 *    not possible to auto generate default texture coordinates for those
 *    layers.
 *
 * @layer0_override_texture: forcibly tells us to bind this GL texture name for
 *    layer 0 instead of plucking the gl_texture from the CoglTexture of layer
 *    0.
 *
 *    The intention of this is for any geometry that supports sliced textures.
 *    The code will can iterate each of the slices and re-flush the material
 *    forcing the GL texture of each slice in turn.
 *
 * XXX: It might also help if we could specify a texture matrix for code
 *    dealing with slicing that would be multiplied with the users own matrix.
 *
 *    Normaly texture coords in the range [0, 1] refer to the extents of the
 *    texture, but when your GL texture represents a slice of the real texture
 *    (from the users POV) then a texture matrix would be a neat way of
 *    transforming the mapping for each slice.
 *
 *    Currently for textured rectangles we manually calculate the texture
 *    coords for each slice based on the users given coords, but this solution
 *    isn't ideal, and can't be used with CoglVertexBuffers.
 */
static void
_cogl_material_flush_layers_gl_state (CoglMaterial *material,
                                      guint32       fallback_mask,
                                      guint32       disable_mask,
                                      GLuint        layer0_override_texture)
{
  GList *tmp;
  int    i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (tmp = material->layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle         layer_handle = (CoglHandle)tmp->data;
      CoglMaterialLayer *layer =
        _cogl_material_layer_pointer_from_handle (layer_handle);
      CoglLayerInfo     *gl_layer_info = NULL;
      CoglLayerInfo      new_gl_layer_info;
      CoglHandle         tex_handle;
      GLuint             gl_texture;
      GLenum             gl_target;
#ifdef HAVE_COGL_GLES2
      GLenum             gl_internal_format;
#endif

      new_gl_layer_info.layer0_overridden =
        layer0_override_texture ? TRUE : FALSE;
      new_gl_layer_info.fallback =
        (fallback_mask & (1<<i)) ? TRUE : FALSE;
      new_gl_layer_info.disabled =
        (disable_mask & (1<<i)) ? TRUE : FALSE;

      tex_handle = layer->texture;
      cogl_texture_get_gl_texture (tex_handle, &gl_texture, &gl_target);

      if (new_gl_layer_info.layer0_overridden)
        gl_texture = layer0_override_texture;
      else if (new_gl_layer_info.fallback)
        {
          if (gl_target == GL_TEXTURE_2D)
            tex_handle = ctx->default_gl_texture_2d_tex;
#ifdef HAVE_COGL_GL
          else if (gl_target == GL_TEXTURE_RECTANGLE_ARB)
            tex_handle = ctx->default_gl_texture_rect_tex;
#endif
          else
            {
              g_warning ("We don't have a default texture we can use to fill "
                         "in for an invalid material layer, since it was "
                         "using an unsupported texture target ");
              /* might get away with this... */
              tex_handle = ctx->default_gl_texture_2d_tex;
            }
          cogl_texture_get_gl_texture (tex_handle, &gl_texture, NULL);
        }

#ifdef HAVE_COGL_GLES2
      {
        CoglTexture *tex =
          _cogl_texture_pointer_from_handle (tex_handle);
        gl_internal_format = tex->gl_intformat;
      }
#endif

      GE (glActiveTexture (GL_TEXTURE0 + i));

      _cogl_texture_set_filters (layer->texture,
                                 layer->min_filter,
                                 layer->mag_filter);
      if (is_mipmap_filter (layer->min_filter)
          || is_mipmap_filter (layer->mag_filter))
        _cogl_texture_ensure_mipmaps (layer->texture);

      /* FIXME: We could be more clever here and only bind the texture
         if it is different from gl_layer_info->gl_texture to avoid
         redundant GL calls. However a few other places in Cogl and
         Clutter call glBindTexture such as ClutterGLXTexturePixmap so
         we'd need to ensure they affect the cache. Also deleting a
         texture should clear it from the cache in case a new texture
         is generated with the same number */
#ifdef HAVE_COGL_GLES2
      cogl_gles2_wrapper_bind_texture (gl_target,
                                       gl_texture,
                                       gl_internal_format);
#else
      GE (glBindTexture (gl_target, gl_texture));
#endif

      /* XXX: Once we add caching for glBindTexture state, these
       * checks should be moved back up to the top of the loop!
       */
      if (i < ctx->current_layers->len)
        {
          gl_layer_info =
            &g_array_index (ctx->current_layers, CoglLayerInfo, i);

#ifndef DISABLE_MATERIAL_CACHE
          if (gl_layer_info->handle == layer_handle &&
              !(layer->flags & COGL_MATERIAL_LAYER_FLAG_DIRTY) &&
              !(gl_layer_info->layer0_overridden ||
                new_gl_layer_info.layer0_overridden) &&
              (gl_layer_info->fallback
               == new_gl_layer_info.fallback) &&
              (gl_layer_info->disabled
               == new_gl_layer_info.disabled))
            {
              continue;
            }
#endif
        }

      /* Disable the previous target if it was different */
#ifndef DISABLE_MATERIAL_CACHE
      if (gl_layer_info &&
          gl_layer_info->gl_target != gl_target &&
          !gl_layer_info->disabled)
        {
          GE (glDisable (gl_layer_info->gl_target));
        }
#else
      if (gl_layer_info)
        GE (glDisable (gl_layer_info->gl_target));
#endif

      /* Enable/Disable the new target */
      if (!new_gl_layer_info.disabled)
        {
#ifndef DISABLE_MATERIAL_CACHE
          if (!(gl_layer_info &&
                gl_layer_info->gl_target == gl_target &&
                !gl_layer_info->disabled))
#endif
            {
  /* XXX: Debug: Comment this out to disable all texturing: */
#if 1
              GE (glEnable (gl_target));
#endif
            }
        }
      else
        {
#ifndef DISABLE_MATERIAL_CACHE
          if (!(gl_layer_info &&
                gl_layer_info->gl_target == gl_target &&
                gl_layer_info->disabled))
#endif
            {
              GE (glDisable (gl_target));
            }
        }

      _cogl_material_layer_flush_gl_sampler_state (layer, gl_layer_info);

      new_gl_layer_info.handle = layer_handle;
      new_gl_layer_info.flags = layer->flags;
      new_gl_layer_info.gl_target = gl_target;
      new_gl_layer_info.gl_texture = gl_texture;

      if (i < ctx->current_layers->len)
        *gl_layer_info = new_gl_layer_info;
      else
        g_array_append_val (ctx->current_layers, new_gl_layer_info);

      layer->flags &= ~COGL_MATERIAL_LAYER_FLAG_DIRTY;

      if ((i+1) >= CGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
	break;
    }

  /* Disable additional texture units that may have previously been in use.. */
  for (; i < ctx->current_layers->len; i++)
    {
      CoglLayerInfo *gl_layer_info =
        &g_array_index (ctx->current_layers, CoglLayerInfo, i);

#ifndef DISABLE_MATERIAL_CACHE
      if (!gl_layer_info->disabled)
#endif
        {
          GE (glActiveTexture (GL_TEXTURE0 + i));
          GE (glDisable (gl_layer_info->gl_target));
          gl_layer_info->disabled = TRUE;
        }
    }
}

static void
_cogl_material_flush_base_gl_state (CoglMaterial *material,
                                    gboolean skip_gl_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* XXX:
   * Currently we only don't update state when the flags indicate that the
   * current material uses the defaults, and the new material also uses the
   * defaults, but we could do deeper comparisons of state. */

  if (!skip_gl_color)
    {
      if (!(ctx->current_material_flags & COGL_MATERIAL_FLAG_DEFAULT_COLOR
            && material->flags & COGL_MATERIAL_FLAG_DEFAULT_COLOR) ||
          /* Assume if we were previously told to skip the color, then
           * the current color needs updating... */
          ctx->current_material_flush_options.flags &
          COGL_MATERIAL_FLUSH_SKIP_GL_COLOR)
        {
          GE (glColor4f (material->unlit[0],
                         material->unlit[1],
                         material->unlit[2],
                         material->unlit[3]));
        }
    }

  if (!(ctx->current_material_flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL
        && material->flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL))
    {
      /* FIXME - we only need to set these if lighting is enabled... */
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, material->ambient));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, material->diffuse));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, material->specular));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, material->emission));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, &material->shininess));
    }

  if (!(ctx->current_material_flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC
        && material->flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC))
    {
      /* NB: Currently the Cogl defines are compatible with the GL ones: */
      GE (glAlphaFunc (material->alpha_func, material->alpha_func_reference));
    }

  if (!(ctx->current_material_flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND
        && material->flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND))
    {
#if defined (HAVE_COGL_GLES2)
      gboolean have_blend_equation_seperate = TRUE;
#elif defined (HAVE_COGL_GL)
      gboolean have_blend_equation_seperate = FALSE;
      if (ctx->pf_glBlendEquationSeparate) /* Only GL 2.0 + */
        have_blend_equation_seperate = TRUE;
#endif

#ifndef HAVE_COGL_GLES /* GLES 1 only has glBlendFunc */
      if (material->blend_src_factor_rgb != material->blend_src_factor_alpha
          || (material->blend_src_factor_rgb !=
              material->blend_src_factor_alpha))
        {
          if (have_blend_equation_seperate &&
              material->blend_equation_rgb != material->blend_equation_alpha)
            GE (glBlendEquationSeparate (material->blend_equation_rgb,
                                         material->blend_equation_alpha));
          else
            GE (glBlendEquation (material->blend_equation_rgb));

          GE (glBlendFuncSeparate (material->blend_src_factor_rgb,
                                   material->blend_dst_factor_rgb,
                                   material->blend_src_factor_alpha,
                                   material->blend_dst_factor_alpha));
          GE (glBlendColor (material->blend_constant[0],
                            material->blend_constant[1],
                            material->blend_constant[2],
                            material->blend_constant[3]));
        }
      else
#endif
      GE (glBlendFunc (material->blend_src_factor_rgb,
                       material->blend_dst_factor_rgb));
    }
}

void
_cogl_material_flush_gl_state (CoglHandle handle,
                               CoglMaterialFlushOptions *options)
{
  CoglMaterial  *material;
  guint32        fallback_layers = 0;
  guint32        disable_layers = 0;
  GLuint         layer0_override_texture = 0;
  gboolean       skip_gl_color = FALSE;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  material = _cogl_material_pointer_from_handle (handle);

  if (options)
    {
      if (options->flags & COGL_MATERIAL_FLUSH_FALLBACK_MASK)
        fallback_layers = options->fallback_layers;
      if (options->flags & COGL_MATERIAL_FLUSH_DISABLE_MASK)
        disable_layers = options->disable_layers;
      if (options->flags & COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE)
        layer0_override_texture = options->layer0_override_texture;
      if (options->flags & COGL_MATERIAL_FLUSH_SKIP_GL_COLOR)
        skip_gl_color = TRUE;
    }

  _cogl_material_flush_base_gl_state (material,
                                      skip_gl_color);

  _cogl_material_flush_layers_gl_state (material,
                                        fallback_layers,
                                        disable_layers,
                                        layer0_override_texture);

  /* NB: we have to take a reference so that next time
   * cogl_material_flush_gl_state is called, we can compare the incomming
   * material pointer with ctx->current_material
   */
  cogl_handle_ref (handle);
  if (ctx->current_material)
    cogl_handle_unref (ctx->current_material);

  ctx->current_material = handle;
  ctx->current_material_flags = material->flags;
  if (options)
    ctx->current_material_flush_options = *options;
  else
    memset (&ctx->current_material_flush_options,
            0, sizeof (CoglMaterialFlushOptions));
}

gboolean
_cogl_material_equal (CoglHandle material0_handle,
                      CoglMaterialFlushOptions *material0_flush_options,
                      CoglHandle material1_handle,
                      CoglMaterialFlushOptions *material1_flush_options,
                      CoglMaterialEqualFlags flags)
{
  CoglMaterial  *material0;
  CoglMaterial  *material1;
  GList         *l0, *l1;

  if (!(flags & COGL_MATERIAL_EQUAL_FLAGS_ASSERT_ALL_DEFAULTS))
    {
      g_critical ("FIXME: _cogl_material_equal doesn't yet support "
                  "deep comparisons of materials");
      return FALSE;
    }
  /* Note: the following code is written with the assumption this
   * constraint will go away*/

  material0 = _cogl_material_pointer_from_handle (material0_handle);
  material1 = _cogl_material_pointer_from_handle (material1_handle);

  if (!((material0_flush_options->flags & COGL_MATERIAL_FLUSH_SKIP_GL_COLOR &&
         material1_flush_options->flags & COGL_MATERIAL_FLUSH_SKIP_GL_COLOR)))
    {
      if ((material0->flags & COGL_MATERIAL_FLAG_DEFAULT_COLOR) !=
          (material1->flags & COGL_MATERIAL_FLAG_DEFAULT_COLOR))
        return FALSE;
      else if (flags & COGL_MATERIAL_EQUAL_FLAGS_ASSERT_ALL_DEFAULTS &&
               !(material0->flags & COGL_MATERIAL_FLAG_DEFAULT_COLOR))
        return FALSE;
      else if (!memcmp (material0->unlit, material1->unlit,
                        sizeof (material0->unlit)))
        return FALSE;
    }

  if ((material0->flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL) !=
      (material1->flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL))
    return FALSE;
  else if (flags & COGL_MATERIAL_EQUAL_FLAGS_ASSERT_ALL_DEFAULTS &&
           !(material0->flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL))
    return FALSE;
#if 0
  else if (!_deep_are_gl_materials_equal ())
    return FALSE;
#endif

  if ((material0->flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC) !=
      (material1->flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC))
    return FALSE;
  else if (flags & COGL_MATERIAL_EQUAL_FLAGS_ASSERT_ALL_DEFAULTS &&
           !(material0->flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC))
    return FALSE;
#if 0
  else if (!_deep_are_alpha_funcs_equal ())
    return FALSE;
#endif

  if ((material0->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND) !=
      (material1->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND))
    return FALSE;
  /* XXX: potentially blending could be "enabled" but the blend mode
   * could be equivalent to being disabled. */

  if (material0->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND)
    {
      if ((material0->flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND) !=
          (material1->flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND))
        return FALSE;
      else if (flags & COGL_MATERIAL_EQUAL_FLAGS_ASSERT_ALL_DEFAULTS &&
               !(material0->flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND))
        return FALSE;
#if 0
      else if (!_deep_is_blend_equal ())
        return FALSE;
#endif
    }

  if (material0_flush_options->fallback_layers !=
      material1_flush_options->fallback_layers ||
      material0_flush_options->disable_layers !=
      material1_flush_options->disable_layers)
    return FALSE;

  l0 = material0->layers;
  l1 = material1->layers;

  while (l0 && l1)
    {
      CoglMaterialLayer *layer0;
      CoglMaterialLayer *layer1;

      if ((l0 == NULL && l1 != NULL) ||
          (l1 == NULL && l0 != NULL))
        return FALSE;

      layer0 = l0->data;
      layer1 = l1->data;

      if (layer0->texture != layer1->texture)
        return FALSE;

      if ((layer0->flags & COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE) !=
          (layer1->flags & COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE))
        return FALSE;
      else if (flags & COGL_MATERIAL_EQUAL_FLAGS_ASSERT_ALL_DEFAULTS &&
               !(layer0->flags & COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE))
        return FALSE;
#if 0
      else if (!_deep_are_layer_combines_equal ())
        return FALSE;
#endif

      l0 = l0->next;
      l1 = l1->next;
    }

  if ((l0 == NULL && l1 != NULL) ||
      (l1 == NULL && l0 != NULL))
    return FALSE;

  return TRUE;
}

/* While a material is referenced by the Cogl journal we can not allow
 * modifications, so this gives us a mechanism to track journal
 * references separately */
CoglHandle
_cogl_material_journal_ref (CoglHandle material_handle)
{
  CoglMaterial *material =
    material = _cogl_material_pointer_from_handle (material_handle);
  material->journal_ref_count++;
  cogl_handle_ref (material_handle);
  return material_handle;
}

void
_cogl_material_journal_unref (CoglHandle material_handle)
{
  CoglMaterial *material =
    material = _cogl_material_pointer_from_handle (material_handle);
  material->journal_ref_count--;
  cogl_handle_unref (material_handle);
}

/* TODO: Should go in cogl.c, but that implies duplication which is also
 * not ideal. */
void
cogl_set_source (CoglHandle material_handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_material (material_handle));

  if (ctx->source_material == material_handle)
    return;

  cogl_handle_ref (material_handle);

  if (ctx->source_material)
    cogl_handle_unref (ctx->source_material);

  ctx->source_material = material_handle;
}
/* TODO: add cogl_set_front_source (), and cogl_set_back_source () */

void
cogl_set_source_texture (CoglHandle texture_handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  CoglColor white;

  g_return_if_fail (texture_handle != COGL_INVALID_HANDLE);

  cogl_material_set_layer (ctx->default_material, 0, texture_handle);
  cogl_color_set_from_4ub (&white, 0xff, 0xff, 0xff, 0xff);
  cogl_material_set_color (ctx->default_material, &white);
  cogl_set_source (ctx->default_material);
}

CoglMaterialFilter
cogl_material_layer_get_min_filter (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer;

  g_return_val_if_fail (cogl_is_material_layer (layer_handle), 0);

  layer = _cogl_material_layer_pointer_from_handle (layer_handle);

  return layer->min_filter;
}

CoglMaterialFilter
cogl_material_layer_get_mag_filter (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer;

  g_return_val_if_fail (cogl_is_material_layer (layer_handle), 0);

  layer = _cogl_material_layer_pointer_from_handle (layer_handle);

  return layer->mag_filter;
}

void
cogl_material_set_layer_filters (CoglHandle         handle,
                                 gint               layer_index,
                                 CoglMaterialFilter min_filter,
                                 CoglMaterialFilter mag_filter)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material);

  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  layer->min_filter = min_filter;
  layer->mag_filter = mag_filter;
}
