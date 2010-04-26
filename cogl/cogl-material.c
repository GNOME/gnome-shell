/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
#include "cogl-journal-private.h"
#ifndef HAVE_COGL_GLES
#include "cogl-program.h"
#endif

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

/*
 * GL/GLES compatability defines for material thingies:
 */

#ifdef HAVE_COGL_GLES2
#include "../gles/cogl-gles2-wrapper.h"
#endif

#ifdef HAVE_COGL_GL
#define glActiveTexture ctx->drv.pf_glActiveTexture
#define glClientActiveTexture ctx->drv.pf_glClientActiveTexture
#define glBlendFuncSeparate ctx->drv.pf_glBlendFuncSeparate
#define glBlendEquation ctx->drv.pf_glBlendEquation
#define glBlendColor ctx->drv.pf_glBlendColor
#define glBlendEquationSeparate ctx->drv.pf_glBlendEquationSeparate

#define glProgramString ctx->drv.pf_glProgramString
#define glBindProgram ctx->drv.pf_glBindProgram
#define glDeletePrograms ctx->drv.pf_glDeletePrograms
#define glGenPrograms ctx->drv.pf_glGenPrograms
#define glProgramLocalParameter4fv ctx->drv.pf_glProgramLocalParameter4fv
#define glUseProgram ctx->drv.pf_glUseProgram
#endif

/* This isn't defined in the GLES headers */
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif

typedef struct _CoglMaterialBackendARBfpPrivate
{
  GString *source;
  GLuint gl_program;
  gboolean *sampled;
  int next_constant_id;
} CoglMaterialBackendARBfpPrivate;

static CoglHandle _cogl_material_layer_copy (CoglHandle layer_handle);

static void _cogl_material_free (CoglMaterial *tex);
static void _cogl_material_layer_free (CoglMaterialLayer *layer);

#if defined (HAVE_COGL_GL)

static const CoglMaterialBackend _cogl_material_glsl_backend;
static const CoglMaterialBackend _cogl_material_arbfp_backend;
static const CoglMaterialBackend _cogl_material_fixed_backend;
static const CoglMaterialBackend *backends[] =
{
  /* The fragment processing backends in order of precedence... */
  &_cogl_material_glsl_backend,
  &_cogl_material_arbfp_backend,
  &_cogl_material_fixed_backend
};
#define COGL_MATERIAL_BACKEND_GLSL       0
#define COGL_MATERIAL_BACKEND_ARBFP      1
#define COGL_MATERIAL_BACKEND_FIXED      2

#elif defined (HAVE_COGL_GLES2)

static const CoglMaterialBackend _cogl_material_glsl_backend;
static const CoglMaterialBackend _cogl_material_fixed_backend;
static const CoglMaterialBackend *backends[] =
{
  /* The fragment processing backends in order of precedence... */
  &_cogl_material_glsl_backend,
  &_cogl_material_fixed_backend
};
#define COGL_MATERIAL_BACKEND_GLSL       0
#define COGL_MATERIAL_BACKEND_FIXED      1

#else /* HAVE_COGL_GLES */

static const CoglMaterialBackend _cogl_material_fixed_backend;
static const CoglMaterialBackend *backends[] =
{
  /* The fragment processing backends in order of precedence... */
  &_cogl_material_fixed_backend
};

#define COGL_MATERIAL_BACKEND_FIXED      0

#endif

#define COGL_MATERIAL_BACKEND_DEFAULT    0
#define COGL_MATERIAL_BACKEND_UNDEFINED -1

COGL_HANDLE_DEFINE (Material, material);
COGL_HANDLE_DEFINE (MaterialLayer, material_layer);

/* #define DISABLE_MATERIAL_CACHE 1 */

GQuark
_cogl_material_error_quark (void)
{
  return g_quark_from_static_string ("cogl-material-error-quark");
}

void
_cogl_material_init_default_material (void)
{
  /* Create new - blank - material */
  CoglMaterial *material = g_slice_new0 (CoglMaterial);
  GLubyte *unlit = material->unlit;
  GLfloat *ambient = material->ambient;
  GLfloat *diffuse = material->diffuse;
  GLfloat *specular = material->specular;
  GLfloat *emission = material->emission;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  material->backend = COGL_MATERIAL_BACKEND_UNDEFINED;

  /* Use the same defaults as the GL spec... */
  unlit[0] = 0xff; unlit[1] = 0xff; unlit[2] = 0xff; unlit[3] = 0xff;
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
  material->blend_src_factor_alpha = GL_ONE;
  material->blend_dst_factor_alpha = GL_ONE_MINUS_SRC_ALPHA;
  material->blend_constant[0] = 0;
  material->blend_constant[1] = 0;
  material->blend_constant[2] = 0;
  material->blend_constant[3] = 0;
#endif
  material->blend_src_factor_rgb = GL_ONE;
  material->blend_dst_factor_rgb = GL_ONE_MINUS_SRC_ALPHA;
  material->flags |= COGL_MATERIAL_FLAG_DEFAULT_BLEND;

  material->user_program = COGL_INVALID_HANDLE;
  material->flags |= COGL_MATERIAL_FLAG_DEFAULT_USER_SHADER;

  material->layers = NULL;
  material->n_layers = 0;
  material->flags |= COGL_MATERIAL_FLAG_DEFAULT_LAYERS;

  ctx->default_material = _cogl_material_handle_new (material);
}

CoglHandle
cogl_material_copy (CoglHandle handle)
{
  CoglMaterial *material = g_slice_new (CoglMaterial);
  GList *l;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  memcpy (material, handle, sizeof (CoglMaterial));

  material->layers = g_list_copy (material->layers);
  for (l = material->layers; l; l = l->next)
    l->data = _cogl_material_layer_copy (l->data);

  return _cogl_material_handle_new (material);
}

CoglHandle
cogl_material_new (void)
{
  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  return cogl_material_copy (ctx->default_material);
}

static void
_cogl_material_backend_free_priv (CoglMaterial *material)
{
  if (material->backend != COGL_MATERIAL_BACKEND_UNDEFINED &&
      backends[material->backend]->free_priv)
    backends[material->backend]->free_priv (material);
}

static void
_cogl_material_free (CoglMaterial *material)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_material_backend_free_priv (material);

  /* Invalidate the ->current_material reference to this material since
   * it will no longer represent the current state.
   *
   * NB: we would also invalidate this if the material we being
   * modified.
   */
  ctx->current_material = COGL_INVALID_HANDLE;

  g_list_foreach (material->layers,
		  (GFunc)cogl_handle_unref, NULL);
  g_list_free (material->layers);
  g_slice_free (CoglMaterial, material);
}

static gboolean
_cogl_material_needs_blending_enabled (CoglMaterial *material,
                                       GLubyte      *override_color)
{
  GList *tmp;

  /* XXX: If we expose manual control over ENABLE_BLEND, we'll add
   * a flag to know when it's user configured, so we don't trash it */

  /* XXX: Uncomment this to disable all blending */
#if 0
  return;
#endif

  if ((override_color && override_color[3] != 0xff) ||
      material->unlit[3] != 0xff ||
      material->ambient[3] != 1.0f ||
      material->diffuse[3] != 1.0f ||
      material->specular[3] != 1.0f ||
      material->emission[3] != 1.0f)
    return TRUE;

  for (tmp = material->layers; tmp != NULL; tmp = tmp->next)
    {
      CoglMaterialLayer *layer = tmp->data;

      /* NB: A layer may have a combine mode set on it but not yet have an
       * associated texture. */
      if (!layer->texture)
        continue;

      if (cogl_texture_get_format (layer->texture) & COGL_A_BIT)
	return TRUE;
    }

  return FALSE;
}

static void
_cogl_material_set_backend (CoglMaterial *material, int backend)
{
  if (material->backend != COGL_MATERIAL_BACKEND_UNDEFINED &&
      backends[material->backend]->free_priv)
    backends[material->backend]->free_priv (material);
  material->backend = backend;
}

/* If primitives have been logged in the journal referencing the current
 * state of this material we need to flush the journal before we can
 * modify it... */
static void
_cogl_material_pre_change_notify (CoglMaterial *material,
                                  unsigned long changes,
                                  GLubyte      *new_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (material->journal_ref_count)
    {
      /* XXX: We don't usually need to flush the journal just due to
       * color changes since material colors are logged in the
       * journals vertex buffer. The exception is when the change in
       * color enables or disables the need for blending. */
      if (changes == COGL_MATERIAL_CHANGE_COLOR)
        {
          gboolean will_need_blending =
            _cogl_material_needs_blending_enabled (material, new_color);
          if (will_need_blending !=
              ((material->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND) ?
               TRUE : FALSE))
            _cogl_journal_flush ();
        }
      else
        _cogl_journal_flush ();
    }

  /* The fixed function backend has no private state and can't
   * do anything special to handle small material changes so we may as
   * well try to find a better backend whenever the material changes.
   *
   * The programmable backends may be able to cache a lot of the code
   * they generate and only need to update a small section of that
   * code in response to a material change therefore we don't want to
   * try searching for another backend when the material changes.
   */
  if (material->backend == COGL_MATERIAL_BACKEND_FIXED)
    _cogl_material_set_backend (material, COGL_MATERIAL_BACKEND_UNDEFINED);

  if (material->backend != COGL_MATERIAL_BACKEND_UNDEFINED &&
      backends[material->backend]->material_change_notify)
    backends[material->backend]->material_change_notify (material,
                                                         changes,
                                                         new_color);

  /* Invalidate any ->current_material reference to this material since
   * it will no longer represent the current state.
   *
   * NB: we also invalidate this if the material is freed
   */
  if (ctx->current_material == material)
    ctx->current_material = COGL_INVALID_HANDLE;
}

static void
handle_automatic_blend_enable (CoglMaterial *material)
{
  material->flags &= ~COGL_MATERIAL_FLAG_ENABLE_BLEND;

  if (_cogl_material_needs_blending_enabled (material, NULL))
    {
      _cogl_material_pre_change_notify (material,
                                        COGL_MATERIAL_CHANGE_ENABLE_BLEND,
                                        NULL);
      material->flags |= COGL_MATERIAL_FLAG_ENABLE_BLEND;
    }
}

static void
_cogl_material_backend_layer_change_notify (CoglMaterialLayer *layer,
                                            unsigned long changes)
{
  int backend = layer->material->backend;
  if (backend == COGL_MATERIAL_BACKEND_UNDEFINED)
    return;

  if (backends[backend]->layer_change_notify)
    backends[backend]->layer_change_notify (layer, changes);
}

static void
_cogl_material_layer_pre_change_notify (CoglMaterialLayer *layer,
                                        CoglMaterialLayerChangeFlags changes)
{
  CoglTextureUnit *unit = _cogl_get_texture_unit (layer->unit_index);

  /* Look at the texture unit corresponding to this layer, if it
   * currently has a back reference to this layer then invalidate it
   * so that next time we come to flush this layer we'll see that the
   * texture unit no longer corresponds to this layer's state.
   */
  if (unit->layer == layer)
    unit->layer = NULL;

  _cogl_material_backend_layer_change_notify (layer, changes);

  _cogl_material_pre_change_notify (layer->material,
                                    COGL_MATERIAL_CHANGE_LAYERS,
                                    NULL);
}

void
cogl_material_get_color (CoglHandle  handle,
                         CoglColor  *color)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  cogl_color_set_from_4ub (color,
                           material->unlit[0],
                           material->unlit[1],
                           material->unlit[2],
                           material->unlit[3]);
}

/* This is used heavily by the cogl journal when logging quads */
void
_cogl_material_get_colorubv (CoglHandle  handle,
                             guint8     *color)
{
  CoglMaterial *material = _cogl_material_pointer_from_handle (handle);
  memcpy (color, material->unlit, 4);
}

void
cogl_material_set_color (CoglHandle       handle,
			 const CoglColor *unlit_color)
{
  CoglMaterial *material;
  GLubyte       unlit[4];

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  unlit[0] = cogl_color_get_red_byte (unlit_color);
  unlit[1] = cogl_color_get_green_byte (unlit_color);
  unlit[2] = cogl_color_get_blue_byte (unlit_color);
  unlit[3] = cogl_color_get_alpha_byte (unlit_color);
  if (memcmp (unlit, material->unlit, sizeof (unlit)) == 0)
    return;

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material, COGL_MATERIAL_CHANGE_COLOR,
                                    unlit);

  memcpy (material->unlit, unlit, sizeof (unlit));

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_COLOR;
  if (unlit[0] == 0xff &&
      unlit[1] == 0xff &&
      unlit[2] == 0xff &&
      unlit[3] == 0xff)
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
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_GL_MATERIAL,
                                    NULL);

  ambient = material->ambient;
  ambient[0] = cogl_color_get_red_float (ambient_color);
  ambient[1] = cogl_color_get_green_float (ambient_color);
  ambient[2] = cogl_color_get_blue_float (ambient_color);
  ambient[3] = cogl_color_get_alpha_float (ambient_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;

  handle_automatic_blend_enable (material);
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
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_GL_MATERIAL,
                                    NULL);

  diffuse = material->diffuse;
  diffuse[0] = cogl_color_get_red_float (diffuse_color);
  diffuse[1] = cogl_color_get_green_float (diffuse_color);
  diffuse[2] = cogl_color_get_blue_float (diffuse_color);
  diffuse[3] = cogl_color_get_alpha_float (diffuse_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;

  handle_automatic_blend_enable (material);
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
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_GL_MATERIAL,
                                    NULL);

  specular = material->specular;
  specular[0] = cogl_color_get_red_float (specular_color);
  specular[1] = cogl_color_get_green_float (specular_color);
  specular[2] = cogl_color_get_blue_float (specular_color);
  specular[3] = cogl_color_get_alpha_float (specular_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;

  handle_automatic_blend_enable (material);
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
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_GL_MATERIAL,
                                    NULL);

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
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_GL_MATERIAL,
                                    NULL);

  emission = material->emission;
  emission[0] = cogl_color_get_red_float (emission_color);
  emission[1] = cogl_color_get_green_float (emission_color);
  emission[2] = cogl_color_get_blue_float (emission_color);
  emission[3] = cogl_color_get_alpha_float (emission_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL;

  handle_automatic_blend_enable (material);
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
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_ALPHA_FUNC,
                                    NULL);

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
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
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
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
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
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
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

  if (count == 1)
    rgb = a = statements;
  else
    {
      rgb = &statements[0];
      a = &statements[1];
    }

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_BLEND,
                                    NULL);

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
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_BLEND,
                                    NULL);

  constant = material->blend_constant;
  constant[0] = cogl_color_get_red_float (constant_color);
  constant[1] = cogl_color_get_green_float (constant_color);
  constant[2] = cogl_color_get_blue_float (constant_color);
  constant[3] = cogl_color_get_alpha_float (constant_color);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_BLEND;
#endif
}

/* XXX: for now we don't mind if the program has vertex shaders
 * attached but if we ever make a similar API public we should only
 * allow attaching of programs containing fragment shaders. Eventually
 * we will have a CoglPipeline abstraction to also cover vertex
 * processing.
 */
void
_cogl_material_set_user_program (CoglHandle handle,
                                 CoglHandle program)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  if (material->user_program == program)
    return;

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_USER_SHADER,
                                    NULL);

  _cogl_material_set_backend (material, COGL_MATERIAL_BACKEND_DEFAULT);

  if (program != COGL_INVALID_HANDLE)
    cogl_handle_ref (program);
  if (material->user_program != COGL_INVALID_HANDLE)
    cogl_handle_unref (material->user_program);
  material->user_program = program;

  if (program == COGL_INVALID_HANDLE)
    material->flags |= COGL_MATERIAL_FLAG_DEFAULT_USER_SHADER;
  else
    material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_USER_SHADER;
}

static void
texture_unit_init (CoglTextureUnit *unit, int index_)
{
  unit->index = index_;
  unit->enabled = FALSE;
  unit->enabled_gl_target = 0;
  unit->gl_texture = 0;
  unit->is_foreign = FALSE;
  unit->dirty_gl_texture = FALSE;
  unit->matrix_stack = _cogl_matrix_stack_new ();

  unit->layer = NULL;
  unit->layer_differences = COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE;
  unit->fallback = FALSE;
  unit->layer0_overridden = FALSE;
  unit->texture = COGL_INVALID_HANDLE;
}

static void
texture_unit_free (CoglTextureUnit *unit)
{
  _cogl_matrix_stack_destroy (unit->matrix_stack);
}

CoglTextureUnit *
_cogl_get_texture_unit (int index_)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  if (ctx->texture_units->len < (index_ + 1))
    {
      int i;
      int prev_len = ctx->texture_units->len;
      ctx->texture_units = g_array_set_size (ctx->texture_units, index_ + 1);
      for (i = prev_len; i <= index_; i++)
        {
          CoglTextureUnit *unit =
            &g_array_index (ctx->texture_units, CoglTextureUnit, i);

          texture_unit_init (unit, i);
        }
    }

  return &g_array_index (ctx->texture_units, CoglTextureUnit, index_);
}

void
_cogl_destroy_texture_units (void)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);
      texture_unit_free (unit);
    }
  g_array_free (ctx->texture_units, TRUE);
}

static void
set_active_texture_unit (int unit_index)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->active_texture_unit != unit_index)
    {
      GE (glActiveTexture (GL_TEXTURE0 + unit_index));
      ctx->active_texture_unit = unit_index;
    }
}

/* Note: this conceptually has slightly different semantics to
 * OpenGL's glBindTexture because Cogl never cares about tracking
 * multiple textures bound to different targets on the same texture
 * unit.
 *
 * glBindTexture lets you bind multiple textures to a single texture
 * unit if they are bound to different targets. So it does something
 * like:
 *   unit->current_texture[target] = texture;
 *
 * Cogl only lets you associate one texture with the currently active
 * texture unit, so the target is basically a redundant parameter
 * that's implicitly set on that texture.
 *
 * Technically this is just a thin wrapper around glBindTexture so
 * actually it does have the GL semantics but it seems worth
 * mentioning the conceptual difference in case anyone wonders why we
 * don't associate the gl_texture with a gl_target in the
 * CoglTextureUnit.
 */
void
_cogl_bind_gl_texture_transient (GLenum gl_target,
                                 GLuint gl_texture,
                                 gboolean is_foreign)
{
  CoglTextureUnit *unit;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  unit = _cogl_get_texture_unit (ctx->active_texture_unit);

  /* NB: If we have previously bound a foreign texture to this texture
   * unit we don't know if that texture has since been deleted and we
   * are seeing the texture name recycled */
  if (unit->gl_texture == gl_texture &&
      !unit->dirty_gl_texture &&
      !unit->is_foreign)
    return;

  GE (glBindTexture (gl_target, gl_texture));

  unit->dirty_gl_texture = TRUE;
  unit->is_foreign = is_foreign;
}

void
_cogl_delete_gl_texture (GLuint gl_texture)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (unit->gl_texture == gl_texture)
        {
          unit->gl_texture = 0;
          unit->dirty_gl_texture = FALSE;
        }
    }
}

/* Asserts that a layer corresponding to the given index exists. If no
 * match is found, then a new empty layer is added.
 */
static CoglMaterialLayer *
_cogl_material_get_layer (CoglMaterial *material,
			  int           index_,
			  gboolean      create_if_not_found)
{
  CoglMaterialLayer *layer;
  GList		    *l;
  CoglHandle	     layer_handle;
  int                i;

  for (l = material->layers, i = 0; l != NULL; l = l->next, i++)
    {
      layer = l->data;
      if (layer->index == index_)
	return layer;

      /* The layers are always sorted, so at this point we know this layer
       * doesn't exist */
      if (layer->index > index_)
	break;
    }
  /* NB: if we now insert a new layer before l, that will maintain order.
   */

  if (!create_if_not_found)
    return NULL;

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_LAYERS,
                                    NULL);

  layer = g_slice_new0 (CoglMaterialLayer);

  layer_handle = _cogl_material_layer_handle_new (layer);
  layer->material = material;
  layer->index = index_;
  layer->differences = 0;
  layer->mag_filter = COGL_MATERIAL_FILTER_LINEAR;
  layer->min_filter = COGL_MATERIAL_FILTER_LINEAR;
  layer->wrap_mode_s = COGL_MATERIAL_WRAP_MODE_AUTOMATIC;
  layer->wrap_mode_t = COGL_MATERIAL_WRAP_MODE_AUTOMATIC;
  layer->wrap_mode_r = COGL_MATERIAL_WRAP_MODE_AUTOMATIC;
  layer->texture = COGL_INVALID_HANDLE;

  layer->unit_index = i;

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
    g_list_insert_before (material->layers, l, layer_handle);

  material->flags &= ~COGL_MATERIAL_FLAG_DEFAULT_LAYERS;

  material->n_layers++;

  return layer;
}

void
cogl_material_set_layer (CoglHandle material_handle,
			 int layer_index,
			 CoglHandle texture_handle)
{
  CoglMaterial	    *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (material_handle));
  g_return_if_fail (texture_handle == COGL_INVALID_HANDLE
                    || cogl_is_texture (texture_handle));

  material = _cogl_material_pointer_from_handle (material_handle);

  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  if (texture_handle == layer->texture)
    return;

  /* possibly flush primitives referencing the current state... */
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_CHANGE_LAYERS,
                                    NULL);

  if (texture_handle)
    cogl_handle_ref (texture_handle);

  if (layer->texture)
    cogl_handle_unref (layer->texture);

  layer->texture = texture_handle;

  handle_automatic_blend_enable (material);

  layer->differences |= COGL_MATERIAL_LAYER_DIFFERENCE_TEXTURE;
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
				 int layer_index,
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
  _cogl_material_layer_pre_change_notify (
                                        layer,
                                        COGL_MATERIAL_LAYER_CHANGE_COMBINE);

  setup_texture_combine_state (rgb,
                               &layer->texture_combine_rgb_func,
                               layer->texture_combine_rgb_src,
                               layer->texture_combine_rgb_op);

  setup_texture_combine_state (a,
                               &layer->texture_combine_alpha_func,
                               layer->texture_combine_alpha_src,
                               layer->texture_combine_alpha_op);


  layer->differences |= COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE;
  return TRUE;
}

void
cogl_material_set_layer_combine_constant (CoglHandle handle,
				          int layer_index,
                                          CoglColor *constant_color)
{
  CoglMaterial      *material;
  CoglMaterialLayer *layer;
  GLfloat           *constant;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_layer_pre_change_notify (
                              layer,
                              COGL_MATERIAL_LAYER_CHANGE_COMBINE_CONSTANT);

  constant = layer->texture_combine_constant;
  constant[0] = cogl_color_get_red_float (constant_color);
  constant[1] = cogl_color_get_green_float (constant_color);
  constant[2] = cogl_color_get_blue_float (constant_color);
  constant[3] = cogl_color_get_alpha_float (constant_color);

  layer->differences |= COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE_CONSTANT;
}

void
cogl_material_set_layer_matrix (CoglHandle material_handle,
				int layer_index,
				CoglMatrix *matrix)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;

  static gboolean initialized_identity_matrix = FALSE;
  static CoglMatrix identity_matrix;

  g_return_if_fail (cogl_is_material (material_handle));

  material = _cogl_material_pointer_from_handle (material_handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  if (cogl_matrix_equal (matrix, &layer->matrix))
    return;

  /* possibly flush primitives referencing the current state... */
  _cogl_material_layer_pre_change_notify (
                              layer,
                              COGL_MATERIAL_LAYER_CHANGE_USER_MATRIX);
  layer->matrix = *matrix;

  if (G_UNLIKELY (!initialized_identity_matrix))
    cogl_matrix_init_identity (&identity_matrix);

  if (cogl_matrix_equal (matrix, &identity_matrix))
    layer->differences &= ~COGL_MATERIAL_LAYER_DIFFERENCE_USER_MATRIX;
  else
    layer->differences |= COGL_MATERIAL_LAYER_DIFFERENCE_USER_MATRIX;
}

static void
_cogl_material_layer_free (CoglMaterialLayer *layer)
{
  CoglTextureUnit *unit = _cogl_get_texture_unit (layer->unit_index);

  /* Since we're freeing the layer make sure the texture unit no
   * longer keeps a back reference to it */
  if (unit->layer == layer)
    unit->layer = NULL;

  if (layer->texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (layer->texture);
  g_slice_free (CoglMaterialLayer, layer);
}

void
cogl_material_remove_layer (CoglHandle material_handle,
			    int layer_index)
{
  CoglMaterial	     *material;
  CoglMaterialLayer  *layer;
  GList		     *l;
  GList		     *l2;
  gboolean            found = FALSE;
  int                 i;

  g_return_if_fail (cogl_is_material (material_handle));

  material = _cogl_material_pointer_from_handle (material_handle);

  for (l = material->layers, i = 0; l != NULL; l = l2, i++)
    {
      /* were going to be modifying the list and continuing to iterate
       * it so we get the pointer to the next link now... */
      l2 = l->next;

      layer = l->data;
      if (layer->index == layer_index)
	{
	  CoglHandle handle = (CoglHandle) layer;

          found = TRUE;

          /* possibly flush primitives referencing the current state... */
          _cogl_material_pre_change_notify (material,
                                            COGL_MATERIAL_CHANGE_LAYERS,
                                            NULL);

	  cogl_handle_unref (handle);
	  material->layers = g_list_delete_link (material->layers, l);
          material->n_layers--;

          /* We need to iterate through the rest of the layers
           * updating the texture unit that they reference. */
	  continue;
	}

      /* All layers following a removed layer need to have their
       * associated texture unit updated... */
      if (found)
        {
          _cogl_material_layer_pre_change_notify (
                                        layer,
                                        COGL_MATERIAL_LAYER_CHANGE_UNIT);
          layer->unit_index = i;
        }
    }

  handle_automatic_blend_enable (material);
}

/* XXX: This API is hopfully just a stop-gap solution. Ideally _cogl_enable
 * will be replaced. */
unsigned long
_cogl_material_get_cogl_enable_flags (CoglHandle material_handle)
{
  CoglMaterial	*material;
  unsigned long	 enable_flags = 0;

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

gboolean
_cogl_material_layer_has_user_matrix (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer =
    _cogl_material_layer_pointer_from_handle (layer_handle);
  return layer->differences & COGL_MATERIAL_LAYER_CHANGE_USER_MATRIX ?
    TRUE : FALSE;
}

static CoglHandle
_cogl_material_layer_copy (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer =
    _cogl_material_layer_pointer_from_handle (layer_handle);
  CoglMaterialLayer *layer_copy = g_slice_new (CoglMaterialLayer);

  memcpy (layer_copy, layer, sizeof (CoglMaterialLayer));

  if (layer_copy->texture != COGL_INVALID_HANDLE)
    cogl_handle_ref (layer_copy->texture);

  return _cogl_material_layer_handle_new (layer_copy);
}

static gboolean
is_mipmap_filter (CoglMaterialFilter filter)
{
  return (filter == COGL_MATERIAL_FILTER_NEAREST_MIPMAP_NEAREST
          || filter == COGL_MATERIAL_FILTER_LINEAR_MIPMAP_NEAREST
          || filter == COGL_MATERIAL_FILTER_NEAREST_MIPMAP_LINEAR
          || filter == COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR);
}

#ifndef HAVE_COGL_GLES
static int
get_max_texture_image_units (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  /* This function is called quite often so we cache the value to
     avoid too many GL calls */
  if (G_UNLIKELY (ctx->max_texture_image_units == -1))
    {
      ctx->max_texture_image_units = 1;
      GE (glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS,
                         &ctx->max_texture_image_units));
    }

  return ctx->max_texture_image_units;
}
#endif

static void
disable_texture_unit (int unit_index)
{
  CoglTextureUnit *unit;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  unit = &g_array_index (ctx->texture_units, CoglTextureUnit, unit_index);

#ifndef DISABLE_MATERIAL_CACHE
  if (unit->enabled)
#endif
    {
      set_active_texture_unit (unit_index);
      GE (glDisable (unit->enabled_gl_target));
      unit->enabled_gl_target = 0;
      unit->enabled = FALSE;

      /* XXX: This implies that a lot of unneeded work will happen
       * if a given layer somehow simply gets disabled and enabled
       * without changing. Currently the public CoglMaterial API
       * doesn't give a way to disable layers but one day we may
       * want to avoid doing this if that changes... */
      unit->layer = NULL;
    }
}

void
_cogl_material_layer_ensure_mipmaps (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer;

  layer = _cogl_material_layer_pointer_from_handle (layer_handle);

  if (layer->texture &&
      (is_mipmap_filter (layer->min_filter) ||
       is_mipmap_filter (layer->mag_filter)))
    _cogl_texture_ensure_mipmaps (layer->texture);
}

static unsigned int
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

void
_cogl_gl_use_program_wrapper (GLuint program)
{
#ifdef COGL_MATERIAL_BACKEND_GLSL
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_gl_program == program)
    return;

  if (program)
    {
      GLenum gl_error;

      while ((gl_error = glGetError ()) != GL_NO_ERROR)
        ;
      glUseProgram (program);
      if (glGetError () != GL_NO_ERROR)
        {
          GE (glUseProgram (0));
          ctx->current_gl_program = 0;
          return;
        }
    }
  else
    GE (glUseProgram (0));

  ctx->current_gl_program = program;
#endif
}

static void
disable_glsl (void)
{
#ifdef COGL_MATERIAL_BACKEND_GLSL
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_GLSL)
    _cogl_gl_use_program_wrapper (0);
#endif
}

static void
disable_arbfp (void)
{
#ifdef COGL_MATERIAL_BACKEND_ARBFP
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_ARBFP)
    GE (glDisable (GL_FRAGMENT_PROGRAM_ARB));
#endif
}

static void
use_program (CoglHandle program_handle, CoglMaterialProgramType type)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  switch (type)
    {
#ifdef COGL_MATERIAL_BACKEND_GLSL
    case COGL_MATERIAL_PROGRAM_TYPE_GLSL:
      {
        /* The GLES2 backend currently manages its own codegen for
         * fixed function API fallbacks and manages its own shader
         * state.  */
#ifndef HAVE_COGL_GLES2
        CoglProgram *program =
          _cogl_program_pointer_from_handle (program_handle);

        _cogl_gl_use_program_wrapper (program->gl_handle);
        disable_arbfp ();
#endif

        ctx->current_use_program_type = type;
        break;
      }
#else
    case COGL_MATERIAL_PROGRAM_TYPE_GLSL:
      g_warning ("Unexpected use of GLSL backend!");
      break;
#endif
#ifdef COGL_MATERIAL_BACKEND_ARBFP
    case COGL_MATERIAL_PROGRAM_TYPE_ARBFP:

      /* _cogl_gl_use_program_wrapper can be called by cogl-program.c
       * so we can't bailout without making sure we glUseProgram (0)
       * first. */
      disable_glsl ();

      if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_ARBFP)
        break;

      GE (glEnable (GL_FRAGMENT_PROGRAM_ARB));

      ctx->current_use_program_type = type;
      break;
#else
    case COGL_MATERIAL_PROGRAM_TYPE_ARBFP:
      g_warning ("Unexpected use of GLSL backend!");
      break;
#endif
#ifdef COGL_MATERIAL_BACKEND_FIXED
    case COGL_MATERIAL_PROGRAM_TYPE_FIXED:

      /* _cogl_gl_use_program_wrapper can be called by cogl-program.c
       * so we can't bailout without making sure we glUseProgram (0)
       * first. */
      disable_glsl ();

      if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_FIXED)
        break;

      disable_arbfp ();

      ctx->current_use_program_type = type;
#endif
    }
}

#ifdef COGL_MATERIAL_BACKEND_GLSL

static int
_cogl_material_backend_glsl_get_max_texture_units (void)
{
  return get_max_texture_image_units ();
}

static gboolean
_cogl_material_backend_glsl_start (CoglMaterial *material)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    return FALSE;

  /* FIXME: This will likely conflict with the GLES 2 backends use of
   * glUseProgram.
   */
  if (!(ctx->current_material_flags & COGL_MATERIAL_FLAG_DEFAULT_USER_SHADER
        && material->flags & COGL_MATERIAL_FLAG_DEFAULT_USER_SHADER))
    {
      CoglHandle program = material->user_program;
      if (program == COGL_INVALID_HANDLE)
        return FALSE; /* XXX: change me when we support code generation here */

      use_program (program, COGL_MATERIAL_PROGRAM_TYPE_GLSL);
      return TRUE;
    }

  /* TODO: also support code generation */

  return FALSE;
}

gboolean
_cogl_material_backend_glsl_add_layer (CoglMaterialLayer *layer)
{
  return TRUE;
}

gboolean
_cogl_material_backend_glsl_passthrough (CoglMaterial *material)
{
  return TRUE;
}

gboolean
_cogl_material_backend_glsl_end (CoglMaterial *material)
{
  return TRUE;
}

static const CoglMaterialBackend _cogl_material_glsl_backend =
{
  _cogl_material_backend_glsl_get_max_texture_units,
  _cogl_material_backend_glsl_start,
  _cogl_material_backend_glsl_add_layer,
  _cogl_material_backend_glsl_passthrough,
  _cogl_material_backend_glsl_end,
  NULL, /* material_state_change_notify */
  NULL, /* layer_state_change_notify */
  NULL, /* free_priv */
};

#endif /* COGL_MATERIAL_BACKEND_GLSL */

#ifdef COGL_MATERIAL_BACKEND_ARBFP

static int
_cogl_material_backend_arbfp_get_max_texture_units (void)
{
  return get_max_texture_image_units ();
}

static gboolean
_cogl_material_backend_arbfp_start (CoglMaterial *material)
{
  CoglMaterialBackendARBfpPrivate *priv;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!_cogl_features_available_private (COGL_FEATURE_PRIVATE_ARB_FP))
    return FALSE;

  /* TODO: support fog */
  if (ctx->fog_enabled)
    return FALSE;

  if (!material->backend_priv)
    material->backend_priv = g_slice_new0 (CoglMaterialBackendARBfpPrivate);
  priv = material->backend_priv;

  if (priv->gl_program == 0)
    {
      /* Se reuse a single grow-only GString for ARBfp code-gen */
      g_string_set_size (ctx->arbfp_source_buffer, 0);
      priv->source = ctx->arbfp_source_buffer;
      g_string_append (priv->source,
                       "!!ARBfp1.0\n"
                       "TEMP output;\n"
                       "TEMP tmp0, tmp1, tmp2, tmp3, tmp4;\n"
                       "PARAM half = {.5, .5, .5, .5};\n"
                       "PARAM one = {1, 1, 1, 1};\n"
                       "PARAM two = {2, 2, 2, 2};\n"
                       "PARAM minus_one = {-1, -1, -1, -1};\n");
      priv->sampled = g_new0 (gboolean, material->n_layers);
    }

  return TRUE;
}

/* Determines if we need to handle the RGB and A texture combining
 * separately or is the same function used for both channel masks and
 * with the same arguments...
 */
static gboolean
need_texture_combine_separate (CoglMaterialLayer *layer)
{
  int n_args;
  int i;

  if (layer->texture_combine_rgb_func != layer->texture_combine_alpha_func)
    return TRUE;

  n_args = get_n_args_for_combine_func (layer->texture_combine_rgb_func);

  for (i = 0; i < n_args; i++)
    {
      if (layer->texture_combine_rgb_src[i] !=
          layer->texture_combine_alpha_src[i])
        return TRUE;

      /*
       * We can allow some variation of the source operands without
       * needing a separation...
       *
       * "A = REPLACE (CONSTANT[A])" + either of the following...
       * "RGB = REPLACE (CONSTANT[RGB])"
       * "RGB = REPLACE (CONSTANT[A])"
       *
       * can be combined as:
       * "RGBA = REPLACE (CONSTANT)" or
       * "RGBA = REPLACE (CONSTANT[A])" or
       *
       * And "A = REPLACE (1-CONSTANT[A])" + either of the following...
       * "RGB = REPLACE (1-CONSTANT)" or
       * "RGB = REPLACE (1-CONSTANT[A])"
       *
       * can be combined as:
       * "RGBA = REPLACE (1-CONSTANT)" or
       * "RGBA = REPLACE (1-CONSTANT[A])"
       */
      switch (layer->texture_combine_alpha_op[i])
        {
        case GL_SRC_ALPHA:
          switch (layer->texture_combine_rgb_op[i])
            {
            case GL_SRC_COLOR:
            case GL_SRC_ALPHA:
              break;
            default:
              return FALSE;
            }
          break;
        case GL_ONE_MINUS_SRC_ALPHA:
          switch (layer->texture_combine_rgb_op[i])
            {
            case GL_ONE_MINUS_SRC_COLOR:
            case GL_ONE_MINUS_SRC_ALPHA:
              break;
            default:
              return FALSE;
            }
          break;
        default:
          return FALSE;	/* impossible */
        }
    }

   return FALSE;
}

static const char *
gl_target_to_arbfp_string (GLenum gl_target)
{
#ifndef HAVE_COGL_GLES2
  if (gl_target == GL_TEXTURE_1D)
    return "1D";
  else
#endif
    if (gl_target == GL_TEXTURE_2D)
    return "2D";
#ifdef ARB_texture_rectangle
  else if (gl_target == GL_TEXTURE_RECTANGLE_ARB)
    return "RECT";
#endif
  else
    return "2D";
}

static void
setup_texture_source (CoglMaterialBackendARBfpPrivate *priv,
                      int unit_index,
                      GLenum gl_target)
{
  if (!priv->sampled[unit_index])
    {
      g_string_append_printf (priv->source,
                              "TEMP texel%d;\n"
                              "TEX texel%d,fragment.texcoord[%d],"
                              "texture[%d],%s;\n",
                              unit_index,
                              unit_index,
                              unit_index,
                              unit_index,
                              gl_target_to_arbfp_string (gl_target));
      priv->sampled[unit_index] = TRUE;
    }
}

typedef enum _CoglMaterialBackendARBfpArgType
{
  COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE,
  COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT,
  COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE
} CoglMaterialBackendARBfpArgType;

typedef struct _CoglMaterialBackendARBfpArg
{
  const char *name;

  CoglMaterialBackendARBfpArgType type;

  /* for type = TEXTURE */
  int texture_unit;
  GLenum texture_target;

  /* for type = CONSTANT */
  int constant_id;

  const char *swizzle;

} CoglMaterialBackendARBfpArg;

static void
append_arg (GString *source, const CoglMaterialBackendARBfpArg *arg)
{
  switch (arg->type)
    {
    case COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE:
      g_string_append_printf (source, "texel%d%s",
                              arg->texture_unit, arg->swizzle);
      break;
    case COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT:
      g_string_append_printf (source, "constant%d%s",
                              arg->constant_id, arg->swizzle);
      break;
    case COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE:
      g_string_append_printf (source, "%s%s",
                              arg->name, arg->swizzle);
      break;
    }
}

/* Note: we are trying to avoid duplicating strings during codegen
 * which is why we have the slightly awkward
 * CoglMaterialBackendARBfpArg mechanism. */
static void
setup_arg (CoglMaterial *material,
           CoglMaterialLayer *layer,
           CoglBlendStringChannelMask mask,
           int arg_index,
           GLint src,
           GLint op,
           CoglMaterialBackendARBfpArg *arg)
{
  CoglMaterialBackendARBfpPrivate *priv = material->backend_priv;
  static const char *tmp_name[3] = { "tmp0", "tmp1", "tmp2" };
  GLenum gl_target;

  switch (src)
    {
    case GL_TEXTURE:
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE;
      arg->name = "texel%d";
      arg->texture_unit = layer->unit_index;
      cogl_texture_get_gl_texture (layer->texture, NULL, &gl_target);
      setup_texture_source (priv, arg->texture_unit, gl_target);
      break;
    case GL_CONSTANT:
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT;
      arg->name = "constant%d";
      arg->constant_id = priv->next_constant_id++;
      g_string_append_printf (priv->source,
                              "PARAM constant%d = "
                              "  {%f, %f, %f, %f};\n",
                              arg->constant_id,
                              layer->texture_combine_constant[0],
                              layer->texture_combine_constant[1],
                              layer->texture_combine_constant[2],
                              layer->texture_combine_constant[3]);
      break;
    case GL_PRIMARY_COLOR:
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = "fragment.color.primary";
      break;
    case GL_PREVIOUS:
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE;
      if (layer->unit_index == 0)
        arg->name = "fragment.color.primary";
      else
        arg->name = "output";
      break;
    default: /* GL_TEXTURE0..N */
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE;
      arg->name = "texture[%d]";
      arg->texture_unit = src - GL_TEXTURE0;
      cogl_texture_get_gl_texture (layer->texture, NULL, &gl_target);
      setup_texture_source (priv, arg->texture_unit, gl_target);
    }

  arg->swizzle = "";

  switch (op)
    {
    case GL_SRC_COLOR:
      break;
    case GL_ONE_MINUS_SRC_COLOR:
      g_string_append_printf (priv->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (priv->source, arg);
      g_string_append_printf (priv->source, ";\n");
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = tmp_name[arg_index];
      arg->swizzle = "";
      break;
    case GL_SRC_ALPHA:
      /* avoid a swizzle if we know RGB are going to be masked
       * in the end anyway */
      if (mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        arg->swizzle = ".a";
      break;
    case GL_ONE_MINUS_SRC_ALPHA:
      g_string_append_printf (priv->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (priv->source, arg);
      /* avoid a swizzle if we know RGB are going to be masked
       * in the end anyway */
      if (mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        g_string_append_printf (priv->source, ".a;\n");
      else
        g_string_append_printf (priv->source, ";\n");
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = tmp_name[arg_index];
      break;
    default:
      g_error ("Unknown texture combine operator %d", op);
      break;
    }
}

static gboolean
backend_arbfp_args_equal (CoglMaterialBackendARBfpArg *arg0,
                          CoglMaterialBackendARBfpArg *arg1)
{
  if (arg0->type != arg1->type)
    return FALSE;

  if (arg0->name != arg1->name &&
      strcmp (arg0->name, arg1->name) != 0)
    return FALSE;

  if (arg0->type == COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE &&
      arg0->texture_unit != arg1->texture_unit)
    return FALSE;
  /* Note we don't have to check the target; a texture unit can only
   * have one target enabled at a time. */

  if (arg0->type == COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT &&
      arg0->constant_id != arg0->constant_id)
    return FALSE;

  if (arg0->swizzle != arg1->swizzle &&
      strcmp (arg0->swizzle, arg1->swizzle) != 0)
    return FALSE;

  return TRUE;
}

static void
append_function (CoglMaterial *material,
                 CoglMaterialLayer *layer,
                 CoglBlendStringChannelMask mask,
                 GLint function,
                 CoglMaterialBackendARBfpArg *args,
                 int n_args)
{
  CoglMaterialBackendARBfpPrivate *priv = material->backend_priv;
  const char *mask_name;

  switch (mask)
    {
    case COGL_BLEND_STRING_CHANNEL_MASK_RGB:
      mask_name = ".rgb";
      break;
    case COGL_BLEND_STRING_CHANNEL_MASK_ALPHA:
      mask_name = ".a";
      break;
    case COGL_BLEND_STRING_CHANNEL_MASK_RGBA:
      mask_name = "";
      break;
    default:
      g_error ("Unknown channel mask %d", mask);
      mask_name = "";
    }

  switch (function)
    {
    case GL_ADD:
      g_string_append_printf (priv->source, "ADD_SAT output%s, ",
                              mask_name);
      break;
    case GL_MODULATE:
      /* Note: no need to saturate since we can assume operands
       * have values in the range [0,1] */
      g_string_append_printf (priv->source, "MUL output%s, ",
                              mask_name);
      break;
    case GL_REPLACE:
      /* Note: no need to saturate since we can assume operand
       * has a value in the range [0,1] */
      g_string_append_printf (priv->source, "MOV output%s, ",
                              mask_name);
      break;
    case GL_SUBTRACT:
      g_string_append_printf (priv->source, "SUB_SAT output%s, ",
                              mask_name);
      break;
    case GL_ADD_SIGNED:
      g_string_append_printf (priv->source, "ADD tmp3%s, ",
                              mask_name);
      append_arg (priv->source, &args[0]);
      g_string_append (priv->source, ", ");
      append_arg (priv->source, &args[1]);
      g_string_append (priv->source, ";\n");
      g_string_append_printf (priv->source, "SUB_SAT output%s, tmp3, half",
                              mask_name);
      n_args = 0;
      break;
    case GL_DOT3_RGB:
    /* These functions are the same except that GL_DOT3_RGB never
     * updates the alpha channel.
     *
     * NB: GL_DOT3_RGBA is a bit special because it effectively forces
     * an RGBA mask and we end up ignoring any separate alpha channel
     * function.
     */
    case GL_DOT3_RGBA:
      {
        const char *tmp4 = "tmp4";

        /* The maths for this was taken from Mesa;
         * apparently:
         *
         * tmp3 = 2*src0 - 1
         * tmp4 = 2*src1 - 1
         * output = DP3 (tmp3, tmp4)
         *
         * is the same as:
         *
         * output = 4 * DP3 (src0 - 0.5, src1 - 0.5)
         */

        g_string_append (priv->source, "MAD tmp3, two, ");
        append_arg (priv->source, &args[0]);
        g_string_append (priv->source, ", minus_one;\n");

        if (!backend_arbfp_args_equal (&args[0], &args[1]))
          {
            g_string_append (priv->source, "MAD tmp4, two, ");
            append_arg (priv->source, &args[1]);
            g_string_append (priv->source, ", minus_one;\n");
          }
        else
          tmp4 = "tmp3";

        g_string_append_printf (priv->source,
                                "DP3_SAT output%s, tmp3, %s",
                                mask_name, tmp4);
        n_args = 0;
      }
      break;
    case GL_INTERPOLATE:
      /* Note: no need to saturate since we can assume operands
       * have values in the range [0,1] */

      /* NB: GL_INTERPOLATE = arg0*arg2 + arg1*(1-arg2)
       * but LRP dst, a, b, c = b*a + c*(1-a) */
      g_string_append_printf (priv->source, "LRP output%s, ",
                              mask_name);
      append_arg (priv->source, &args[2]);
      g_string_append (priv->source, ", ");
      append_arg (priv->source, &args[0]);
      g_string_append (priv->source, ", ");
      append_arg (priv->source, &args[1]);
      n_args = 0;
      break;
    default:
      g_error ("Unknown texture combine function %d", function);
      g_string_append_printf (priv->source, "MUL_SAT output%s, ",
                              mask_name);
      n_args = 2;
      break;
    }

  if (n_args > 0)
    append_arg (priv->source, &args[0]);
  if (n_args > 1)
    {
      g_string_append (priv->source, ", ");
      append_arg (priv->source, &args[1]);
    }
  g_string_append (priv->source, ";\n");
}

static void
append_masked_combine (CoglMaterial *material,
                       CoglMaterialLayer *layer,
                       CoglBlendStringChannelMask mask,
                       GLint function,
                       GLint *src,
                       GLint *op)
{
  int i;
  int n_args;
  CoglMaterialBackendARBfpArg args[3];

  n_args = get_n_args_for_combine_func (function);

  for (i = 0; i < n_args; i++)
    {
      setup_arg (material,
                 layer,
                 mask,
                 i,
                 src[i],
                 op[i],
                 &args[i]);
    }

  append_function (material,
                   layer,
                   mask,
                   function,
                   args,
                   n_args);
}

static gboolean
_cogl_material_backend_arbfp_add_layer (CoglMaterialLayer *layer)
{
  CoglMaterial *material = layer->material;
  CoglMaterialBackendARBfpPrivate *priv = material->backend_priv;

  /* Notes...
   *
   * We are ignoring the issue of texture indirection limits until
   * someone complains (Ref Section 3.11.6 in the ARB_fragment_program
   * spec)
   *
   * There always five TEMPs named tmp0, tmp1 and tmp2, tmp3 and tmp4
   * available and these constants: 'one' = {1, 1, 1, 1}, 'half'
   * {.5, .5, .5, .5}, 'two' = {2, 2, 2, 2}, 'minus_one' = {-1, -1,
   * -1, -1}
   *
   * tmp0-2 are intended for dealing with some of the texture combine
   * operands (e.g. GL_ONE_MINUS_SRC_COLOR) tmp3/4 are for dealing
   * with the GL_ADD_SIGNED texture combine and the GL_DOT3_RGB[A]
   * functions.
   *
   * Each layer outputs to the TEMP called "output", and reads from
   * output if it needs to refer to GL_PREVIOUS. (we detect if we are
   * layer0 so we will read fragment.color for GL_PREVIOUS in that
   * case)
   *
   * We aim to do all the channels together if the same function is
   * used for RGB as for A.
   *
   * We aim to avoid string duplication / allocations during codegen.
   *
   * We are careful to only saturate when writing to output.
   */

  if (!priv->source)
    return TRUE;

  if (!need_texture_combine_separate (layer))
    {
      append_masked_combine (material,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGBA,
                             layer->texture_combine_rgb_func,
                             layer->texture_combine_rgb_src,
                             layer->texture_combine_rgb_op);
    }
  else if (layer->texture_combine_rgb_func == GL_DOT3_RGBA)
    {
      /* GL_DOT3_RGBA Is a bit weird as a GL_COMBINE_RGB function
       * since if you use it, it overrides your ALPHA function...
       */
      append_masked_combine (material,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGBA,
                             layer->texture_combine_rgb_func,
                             layer->texture_combine_rgb_src,
                             layer->texture_combine_rgb_op);
    }
  else
    {
      append_masked_combine (material,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGB,
                             layer->texture_combine_rgb_func,
                             layer->texture_combine_rgb_src,
                             layer->texture_combine_rgb_op);
      append_masked_combine (material,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_ALPHA,
                             layer->texture_combine_alpha_func,
                             layer->texture_combine_alpha_src,
                             layer->texture_combine_alpha_op);
    }

  return TRUE;
}

gboolean
_cogl_material_backend_arbfp_passthrough (CoglMaterial *material)
{
  CoglMaterialBackendARBfpPrivate *priv = material->backend_priv;

  if (!priv->source)
    return TRUE;

  g_string_append (priv->source, "MOV output, fragment.color.primary;\n");
  return TRUE;
}

static gboolean
_cogl_material_backend_arbfp_end (CoglMaterial *material)
{
  CoglMaterialBackendARBfpPrivate *priv = material->backend_priv;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (priv->source)
    {
      GLenum gl_error;

      g_string_append (priv->source, "MOV result.color,output;\n");
      g_string_append (priv->source, "END\n");

      if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_SHOW_SOURCE))
        g_message ("material program:\n%s", priv->source->str);

      GE (glGenPrograms (1, &priv->gl_program));

      GE (glBindProgram (GL_FRAGMENT_PROGRAM_ARB, priv->gl_program));

      while ((gl_error = glGetError ()) != GL_NO_ERROR)
        ;
      glProgramString (GL_FRAGMENT_PROGRAM_ARB,
                       GL_PROGRAM_FORMAT_ASCII_ARB,
                       priv->source->len,
                       priv->source->str);
      if (glGetError () != GL_NO_ERROR)
        {
          g_warning ("\n%s\n%s",
                     priv->source->str,
                     glGetString (GL_PROGRAM_ERROR_STRING_ARB));
        }

      priv->source = NULL;

      g_free (priv->sampled);
      priv->sampled = NULL;
    }
  else
    GE (glBindProgram (GL_FRAGMENT_PROGRAM_ARB, priv->gl_program));

  use_program (COGL_INVALID_HANDLE, COGL_MATERIAL_PROGRAM_TYPE_ARBFP);

  return TRUE;
}

static void
_cogl_material_backend_arbfp_material_change_notify (CoglMaterial *material,
                                                     unsigned long changes,
                                                     GLubyte *new_color)
{
  CoglMaterialBackendARBfpPrivate *priv = material->backend_priv;
  static const unsigned long fragment_op_changes =
    COGL_MATERIAL_CHANGE_LAYERS;
    /* TODO: COGL_MATERIAL_CHANGE_FOG */

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (priv &&
      priv->gl_program &&
      changes & fragment_op_changes)
    {
      GE (glDeletePrograms (1, &priv->gl_program));
      priv->gl_program = 0;
    }
}

static void
_cogl_material_backend_arbfp_layer_change_notify (CoglMaterialLayer *layer,
                                                  unsigned long changes)
{
  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
  return;
}

static void
_cogl_material_backend_arbfp_free_priv (CoglMaterial *material)
{
  CoglMaterialBackendARBfpPrivate *priv = material->backend_priv;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (priv)
    {
      glDeletePrograms (1, &priv->gl_program);
      if (priv->sampled)
        g_free (priv->sampled);
      g_slice_free (CoglMaterialBackendARBfpPrivate, material->backend_priv);
    }
}

static const CoglMaterialBackend _cogl_material_arbfp_backend =
{
  _cogl_material_backend_arbfp_get_max_texture_units,
  _cogl_material_backend_arbfp_start,
  _cogl_material_backend_arbfp_add_layer,
  _cogl_material_backend_arbfp_passthrough,
  _cogl_material_backend_arbfp_end,
  _cogl_material_backend_arbfp_material_change_notify,
  _cogl_material_backend_arbfp_layer_change_notify,
  _cogl_material_backend_arbfp_free_priv
};

#endif /* COGL_MATERIAL_BACKEND_ARBFP */

static int
_cogl_material_backend_fixed_get_max_texture_units (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  /* This function is called quite often so we cache the value to
     avoid too many GL calls */
  if (ctx->max_texture_units == -1)
    {
      ctx->max_texture_units = 1;
      GE (glGetIntegerv (GL_MAX_TEXTURE_UNITS,
                         &ctx->max_texture_units));
    }

  return ctx->max_texture_units;
}

static gboolean
_cogl_material_backend_fixed_start (CoglMaterial *material)
{
  use_program (COGL_INVALID_HANDLE, COGL_MATERIAL_PROGRAM_TYPE_FIXED);
  return TRUE;
}

static gboolean
_cogl_material_backend_fixed_add_layer (CoglMaterialLayer *layer)
{
  CoglTextureUnit *unit = _cogl_get_texture_unit (layer->unit_index);
  int n_rgb_func_args;
  int n_alpha_func_args;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* XXX: Beware that since we are changing the active texture unit we
   * must make sure we don't call into other Cogl components that may
   * temporarily bind texture objects to query/modify parameters until
   * we restore texture unit 1 as the active unit. For more details
   * about this see the end of _cogl_material_flush_gl_state
   */
  set_active_texture_unit (unit->index);

#ifndef DISABLE_MATERIAL_CACHE
  if (unit->layer_differences & COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE ||
      layer->differences & COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE)
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

  return TRUE;
}

static gboolean
_cogl_material_backend_fixed_end (CoglMaterial *material)
{
  /* There is a convention to always leave texture unit 1 active and
   * since we modify the active unit in
   * _cogl_material_backend_fixed_add_layer we need to restore it
   * here...
   *
   * (See the end of _cogl_material_flush_gl_state for more
   *  details) */
  set_active_texture_unit (1);
  return TRUE;
}

static const CoglMaterialBackend _cogl_material_fixed_backend =
{
  _cogl_material_backend_fixed_get_max_texture_units,
  _cogl_material_backend_fixed_start,
  _cogl_material_backend_fixed_add_layer,
  NULL,
  _cogl_material_backend_fixed_end,
  NULL, /* material_change_notify */
  NULL, /* layer_change_notify */
  NULL /* free_priv */
};

/* Here we resolve what low level GL texture we are *actually* going
 * to use. This can either be a layer0 override texture, it can be a
 * fallback texture or we can query the CoglTexture for the GL
 * texture.
 */
static void
_cogl_material_layer_get_texture_info (CoglMaterialLayer *layer,
                                       GLuint layer0_override_texture,
                                       gboolean fallback,
                                       CoglHandle *texture,
                                       GLuint *gl_texture,
                                       GLuint *gl_target)
{
  gboolean layer0_overridden = layer0_override_texture ? TRUE : FALSE;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  *texture = layer->texture;
  if (G_LIKELY (*texture != COGL_INVALID_HANDLE))
    cogl_texture_get_gl_texture (*texture, gl_texture, gl_target);
  else
    {
      fallback = TRUE;
      *gl_target = GL_TEXTURE_2D;
    }

  if (layer0_overridden && layer->unit_index == 0)
    {
      /* We assume that layer0 overrides are only used for sliced
       * textures where the GL texture is actually a sub component
       * of the layer->texture... */
      *texture = layer->texture;
      *gl_texture = layer0_override_texture;
    }
  else if (fallback)
    {
      if (*gl_target == GL_TEXTURE_2D)
        *texture = ctx->default_gl_texture_2d_tex;
#ifdef HAVE_COGL_GL
      else if (*gl_target == GL_TEXTURE_RECTANGLE_ARB)
        *texture = ctx->default_gl_texture_rect_tex;
#endif
      else
        {
          g_warning ("We don't have a default texture we can use to fill "
                     "in for an invalid material layer, since it was "
                     "using an unsupported texture target ");
          /* might get away with this... */
          *texture = ctx->default_gl_texture_2d_tex;
        }
      cogl_texture_get_gl_texture (*texture, gl_texture, NULL);
    }
}

#ifndef HAVE_COGL_GLES

static gboolean
blend_factor_uses_constant (GLenum blend_factor)
{
  return (blend_factor == GL_CONSTANT_COLOR ||
          blend_factor == GL_ONE_MINUS_CONSTANT_COLOR ||
          blend_factor == GL_CONSTANT_ALPHA ||
          blend_factor == GL_ONE_MINUS_CONSTANT_ALPHA);
}

#endif

static void
_cogl_material_flush_color_blend_alpha_state (CoglMaterial *material,
                                              gboolean skip_gl_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!skip_gl_color)
    {
      if (!(ctx->current_material_flags & COGL_MATERIAL_FLAG_DEFAULT_COLOR) ||
          !(material->flags & COGL_MATERIAL_FLAG_DEFAULT_COLOR) ||
          /* Assume if we were previously told to skip the color, then
           * the current color needs updating... */
          ctx->current_material_skip_gl_color)
        {
          GE (glColor4ub (material->unlit[0],
                          material->unlit[1],
                          material->unlit[2],
                          material->unlit[3]));
        }
    }

  /* XXX:
   * Currently we only don't update state when the flags indicate that the
   * current material uses the defaults, and the new material also uses the
   * defaults, but we could do deeper comparisons of state.
   */

  if (!(ctx->current_material_flags &
        COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL) ||
      !(material->flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL))
    {
      /* FIXME - we only need to set these if lighting is enabled... */
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, material->ambient));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, material->diffuse));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, material->specular));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, material->emission));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS,
                        &material->shininess));
    }

  if (!(ctx->current_material_flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND) ||
      !(material->flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND))
    {
#if defined (HAVE_COGL_GLES2)
      gboolean have_blend_equation_seperate = TRUE;
      gboolean have_blend_func_separate = TRUE;
#elif defined (HAVE_COGL_GL)
      gboolean have_blend_equation_seperate = FALSE;
      gboolean have_blend_func_separate = FALSE;
      if (ctx->drv.pf_glBlendEquationSeparate) /* Only GL 2.0 + */
        have_blend_equation_seperate = TRUE;
      if (ctx->drv.pf_glBlendFuncSeparate) /* Only GL 1.4 + */
        have_blend_func_separate = TRUE;
#endif

#ifndef HAVE_COGL_GLES /* GLES 1 only has glBlendFunc */
      if (have_blend_equation_seperate &&
          material->blend_equation_rgb != material->blend_equation_alpha)
        GE (glBlendEquationSeparate (material->blend_equation_rgb,
                                     material->blend_equation_alpha));
      else
        GE (glBlendEquation (material->blend_equation_rgb));

      if (blend_factor_uses_constant (material->blend_src_factor_rgb) ||
          blend_factor_uses_constant (material->blend_src_factor_alpha) ||
          blend_factor_uses_constant (material->blend_dst_factor_rgb) ||
          blend_factor_uses_constant (material->blend_dst_factor_alpha))
        GE (glBlendColor (material->blend_constant[0],
                          material->blend_constant[1],
                          material->blend_constant[2],
                          material->blend_constant[3]));

      if (have_blend_func_separate &&
          (material->blend_src_factor_rgb != material->blend_src_factor_alpha ||
           (material->blend_src_factor_rgb !=
            material->blend_src_factor_alpha)))
        GE (glBlendFuncSeparate (material->blend_src_factor_rgb,
                                 material->blend_dst_factor_rgb,
                                 material->blend_src_factor_alpha,
                                 material->blend_dst_factor_alpha));
      else
#endif
        GE (glBlendFunc (material->blend_src_factor_rgb,
                         material->blend_dst_factor_rgb));
    }

  if (!(ctx->current_material_flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC) ||
      !(material->flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC))
    {
      /* NB: Currently the Cogl defines are compatible with the GL ones: */
      GE (glAlphaFunc (material->alpha_func, material->alpha_func_reference));
    }
}

static int
get_max_activateable_texture_units (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  if (G_UNLIKELY (ctx->max_activateable_texture_units == -1))
    {
#ifdef HAVE_COGL_GL
      GLint max_tex_coords;
      GLint max_combined_tex_units;
      GE (glGetIntegerv (GL_MAX_TEXTURE_COORDS, &max_tex_coords));
      GE (glGetIntegerv (GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                         &max_combined_tex_units));
      ctx->max_activateable_texture_units =
        MAX (max_tex_coords - 1, max_combined_tex_units);
#else
      GE (glGetIntegerv (GL_MAX_TEXTURE_UNITS,
                         &ctx->max_activateable_texture_units));
#endif
    }

  return ctx->max_activateable_texture_units;
}

/*
 * _cogl_material_flush_common_gl_state:
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
 *    The intention of this is for any primitives that supports sliced textures.
 *    The code will can iterate each of the slices and re-flush the material
 *    forcing the GL texture of each slice in turn.
 *
 * @wrap_mode_overrides: overrides the wrap modes set on each
 *    layer. This is used to implement the automatic wrap mode.
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
_cogl_material_flush_common_gl_state (CoglMaterial *material,
                                      gboolean      skip_gl_color,
                                      guint32       fallback_mask,
                                      guint32       disable_mask,
                                      GLuint        layer0_override_texture,
                                      const CoglMaterialWrapModeOverrides *
                                                    wrap_mode_overrides)
{
  GList *l;
  int    i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_material_flush_color_blend_alpha_state (material, skip_gl_color);

  for (l = material->layers, i = 0; l != NULL; l = l->next, i++)
    {
      CoglMaterialLayer *layer = l->data;
      CoglTextureUnit   *unit;
      gboolean           fallback;
      CoglHandle         texture;
      GLuint             gl_texture;
      GLenum             gl_target;

      unit = _cogl_get_texture_unit (layer->unit_index);

      /* There may not be enough texture units so we can bail out if
       * that's the case...
       */
      if (G_UNLIKELY (unit->index >= get_max_activateable_texture_units ()))
        {
          static gboolean shown_warning = FALSE;

          if (!shown_warning)
            {
              g_warning ("Your hardware does not have enough texture units"
                         "to handle this many texture layers");
              shown_warning = TRUE;
            }
          break;
        }

      /* Bail out as soon as we hit a bit set in the disable mask */
      if (G_UNLIKELY (disable_mask & (1<<unit->index)))
        break;

      fallback = (fallback_mask & (1<<i)) ? TRUE : FALSE;

      /* Switch units first so we don't disturb the previous unit if
       * something needs to bind the texture temporarily */
      set_active_texture_unit (unit->index);

      _cogl_material_layer_get_texture_info (layer,
                                             layer0_override_texture,
                                             fallback,
                                             &texture,
                                             &gl_texture,
                                             &gl_target);

      /* NB: Due to fallbacks texture may not == layer->texture */
      unit->texture = texture;
      unit->layer0_overridden = layer0_override_texture ? TRUE : FALSE;
      unit->fallback = fallback;

      /* NB: There are several Cogl components and some code in
       * Clutter that will temporarily bind arbitrary GL textures to
       * query and modify texture object parameters. If you look at
       * the end of _cogl_material_flush_gl_state() you can see we
       * make sure that such code always binds to texture unit 1 by
       * always leaving texture unit 1 active. This means we can't
       * rely on the unit->gl_texture state if unit->index == 1.
       * Because texture unit 1 is a bit special we actually defer any
       * necessary glBindTexture for it until the end of
       * _cogl_material_flush_gl_state().
       *
       * NB: we get notified whenever glDeleteTextures is used (see
       * _cogl_delete_gl_texture()) where we invalidate
       * unit->gl_texture references to deleted textures so it's safe
       * to compare unit->gl_texture with gl_texture.  (Without the
       * hook it would be possible to delete a GL texture and create a
       * new one with the same name and comparing unit->gl_texture and
       * gl_texture wouldn't detect that.)
       *
       * NB: for foreign textures we don't know how the deletion of
       * the GL texture objects correspond to the deletion of the
       * CoglTextures so if there was previously a foreign texture
       * associated with the texture unit then we can't assume that we
       * aren't seeing a recycled texture name so we have to bind.
       */
#ifndef DISABLE_MATERIAL_CACHE
      if (unit->gl_texture != gl_texture || unit->is_foreign)
        {
          if (unit->index != 1)
            GE (glBindTexture (gl_target, gl_texture));
          unit->gl_texture = gl_texture;
        }
#else
      GE (glBindTexture (gl_target, gl_texture));
#endif
      unit->is_foreign = _cogl_texture_is_foreign (texture);

      /* Disable the previous target if it was different and it's
       * still enabled */
      if (unit->enabled
#ifndef DISABLE_MATERIAL_CACHE
          && unit->enabled_gl_target != gl_target
#endif
         )
        GE (glDisable (unit->enabled_gl_target));

      if (!G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_TEXTURING)
#ifndef DISABLE_MATERIAL_CACHE
          && !(unit->enabled && unit->enabled_gl_target == gl_target)
#endif
         )
        {
          GE (glEnable (gl_target));
          unit->enabled = TRUE;
          unit->enabled_gl_target = gl_target;
        }

      if (unit->layer_differences & COGL_MATERIAL_LAYER_DIFFERENCE_USER_MATRIX ||
          layer->differences & COGL_MATERIAL_LAYER_DIFFERENCE_USER_MATRIX)
        {
          if (layer->differences & COGL_MATERIAL_LAYER_DIFFERENCE_USER_MATRIX)
            _cogl_matrix_stack_set (unit->matrix_stack, &layer->matrix);
          else
            _cogl_matrix_stack_load_identity (unit->matrix_stack);

          _cogl_matrix_stack_flush_to_gl (unit->matrix_stack,
                                          COGL_MATRIX_TEXTURE);
        }
    }

  /* Disable additional texture units that may have previously been in use.. */
  for (; i < ctx->texture_units->len; i++)
    disable_texture_unit (i);

  /* There is a convention to always leave texture unit 1 active..
   * (See the end of _cogl_material_flush_gl_state for more
   *  details) */
  set_active_texture_unit (1);
}

/* Re-assert the layer's wrap modes on the given CoglTexture.
 *
 * Note: we don't simply forward the wrap modes to layer->texture
 * since the actual texture being used may have been overridden.
 */
static void
_cogl_material_layer_forward_wrap_modes (
                       CoglMaterialLayer *layer,
                       const CoglMaterialWrapModeOverrides *wrap_mode_overrides,
                       CoglHandle texture)
{
  GLenum wrap_mode_s, wrap_mode_t, wrap_mode_r;
  int unit_index = layer->unit_index;

  /* Update the wrap mode on the texture object. The texture backend
     should cache the value so that it will be a no-op if the object
     already has the same wrap mode set. The backend is best placed to
     do this because it knows how many of the coordinates will
     actually be used (ie, a 1D texture only cares about the 's'
     coordinate but a 3D texture would use all three). GL uses the
     wrap mode as part of the texture object state but we are
     pretending it's part of the per-layer environment state. This
     will break if the application tries to use different modes in
     different layers using the same texture. */

  if (wrap_mode_overrides && wrap_mode_overrides->values[unit_index].s)
    wrap_mode_s = (wrap_mode_overrides->values[unit_index].s ==
                   COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT ?
                   GL_REPEAT :
                   wrap_mode_overrides->values[unit_index].s ==
                   COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE ?
                   GL_CLAMP_TO_EDGE :
                   GL_CLAMP_TO_BORDER);
  else if (layer->wrap_mode_s == COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
    wrap_mode_s = GL_CLAMP_TO_EDGE;
  else
    wrap_mode_s = layer->wrap_mode_s;

  if (wrap_mode_overrides && wrap_mode_overrides->values[unit_index].t)
    wrap_mode_t = (wrap_mode_overrides->values[unit_index].t ==
                   COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT ?
                   GL_REPEAT :
                   wrap_mode_overrides->values[unit_index].t ==
                   COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE ?
                   GL_CLAMP_TO_EDGE :
                   GL_CLAMP_TO_BORDER);
  else if (layer->wrap_mode_t == COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
    wrap_mode_t = GL_CLAMP_TO_EDGE;
  else
    wrap_mode_t = layer->wrap_mode_t;

  if (wrap_mode_overrides && wrap_mode_overrides->values[unit_index].r)
    wrap_mode_r = (wrap_mode_overrides->values[unit_index].r ==
                   COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT ?
                   GL_REPEAT :
                   wrap_mode_overrides->values[unit_index].r ==
                   COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE ?
                   GL_CLAMP_TO_EDGE :
                   GL_CLAMP_TO_BORDER);
  else if (layer->wrap_mode_r == COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
    wrap_mode_r = GL_CLAMP_TO_EDGE;
  else
    wrap_mode_r = layer->wrap_mode_r;

  _cogl_texture_set_wrap_mode_parameters (texture,
                                          wrap_mode_s,
                                          wrap_mode_t,
                                          wrap_mode_r);
}

/* OpenGL associates the min/mag filters and repeat modes with the
 * texture object not the texture unit so we always have to re-assert
 * the filter and repeat modes whenever we use a texture since it may
 * be referenced by multiple materials with different modes.
 *
 * XXX: GL_ARB_sampler_objects fixes this in OpenGL so we should
 * eventually look at using this extension when available.
 */
static void
foreach_texture_unit_update_filter_and_wrap_modes (
                      const CoglMaterialWrapModeOverrides *wrap_mode_overrides)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (unit->enabled)
        {
          /* NB: we can't just look at unit->layer->texture because
           * _cogl_material_flush_gl_state may have chosen to flush a
           * different texture due to fallbacks. */
          _cogl_texture_set_filters (unit->texture,
                                     unit->layer->min_filter,
                                     unit->layer->mag_filter);

          _cogl_material_layer_forward_wrap_modes (unit->layer,
                                                   wrap_mode_overrides,
                                                   unit->texture);
        }
    }
}

void
_cogl_material_flush_gl_state (CoglHandle handle,
                               CoglMaterialFlushOptions *options)
{
  CoglMaterial                        *material;
  guint32                              fallback_layers = 0;
  guint32                              disable_layers = 0;
  GLuint                               layer0_override_texture = 0;
  gboolean                             skip_gl_color = FALSE;
  const CoglMaterialWrapModeOverrides *wrap_mode_overrides = NULL;
  int                                  i;
  CoglTextureUnit                     *unit1;
  GList                               *tmp;

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
      if (options->flags & COGL_MATERIAL_FLUSH_WRAP_MODE_OVERRIDES)
        wrap_mode_overrides = &options->wrap_mode_overrides;
    }

  /* If the material we are flushing and the override options are the
   * same then try to bail out as quickly as possible.
   *
   * XXX: the more overrides we add the slower "quickly" will get; I
   * think we need to move towards cheap copy-on-write materials so
   * that exceptional fallbacks/overrides can be implemented simply by
   * copying a material and modifying it before flushing.
   */
  if (ctx->current_material == material &&
      ctx->current_material_fallback_layers == fallback_layers &&
      ctx->current_material_disable_layers == disable_layers &&
      ctx->current_material_layer0_override == layer0_override_texture &&
      ctx->current_material_skip_gl_color == skip_gl_color)
    goto done;

  /* First flush everything that's the same regardless of which
   * material backend is being used...
   *
   * 1) top level state:
   *  glColor (or skip if a vertex attribute is being used for color)
   *  blend state
   *  alpha test state (except for GLES 2.0)
   *
   * 2) then foreach layer:
   *  determine gl_target/gl_texture
   *  bind texture
   *  enable/disable target
   *  flush user matrix
   *
   *  Note: After _cogl_material_flush_common_gl_state you can expect
   *  all state of the layers corresponding texture unit to be
   *  updated.
   */
  _cogl_material_flush_common_gl_state (material,
                                        skip_gl_color,
                                        fallback_layers,
                                        disable_layers,
                                        layer0_override_texture,
                                        wrap_mode_overrides);

  /* Now flush the fragment processing state according to the current
   * fragment processing backend.
   *
   * Note: Some of the backends may not support the current material
   * configuration and in that case it will report an error and we
   * will fallback to a different backend.
   *
   * NB: if material->backend != COGL_MATERIAL_BACKEND_UNDEFINED then
   * we have previously managed to successfully flush this material
   * with the given backend so we will simply use that to avoid
   * fallback code paths.
   */

  if (material->backend == COGL_MATERIAL_BACKEND_UNDEFINED)
    _cogl_material_set_backend (material, COGL_MATERIAL_BACKEND_DEFAULT);

  for (i = material->backend;
       i < G_N_ELEMENTS (backends);
       i++, _cogl_material_set_backend (material, i))
    {
      const GList *l;
      const CoglMaterialBackend *backend = backends[i];
      gboolean added_layer = FALSE;
      gboolean error_adding_layer = FALSE;

      /* E.g. For backends generating code they can setup their
       * scratch buffers here... */
      if (G_UNLIKELY (!backend->start (material)))
        continue;

      for (l = cogl_material_get_layers (material); l; l = l->next)
        {
          CoglMaterialLayer *layer = l->data;
          CoglTextureUnit *unit = _cogl_get_texture_unit (layer->unit_index);

          /* NB: We don't support the random disabling of texture
           * units, so as soon as we hit a disabled unit we know all
           * subsequent units are also disabled */
          if (!unit->enabled)
            break;

          if (G_UNLIKELY (layer->unit_index >=
                          backend->get_max_texture_units ()))
            {
              int j;
              for (j = layer->unit_index; j < ctx->texture_units->len; j++)
                disable_texture_unit (j);
              /* TODO: although this isn't considered an error that
               * warrants falling back to a different backend we
               * should print a warning here. */
              break;
            }

          /* Either generate per layer code snippets or setup the
           * fixed function glTexEnv for each layer... */
          if (G_LIKELY (backend->add_layer (layer)))
            added_layer = TRUE;
          else
            {
              error_adding_layer = TRUE;
              break;
            }
        }

      if (G_UNLIKELY (error_adding_layer))
        continue;

      if (!added_layer &&
          backend->passthrough &&
          G_UNLIKELY (!backend->passthrough (material)))
        continue;

      /* For backends generating code they may compile and link their
       * programs here, update any uniforms and tell OpenGL to use
       * that program.
       */
      if (G_UNLIKELY (!backend->end (material)))
        continue;

      break;
    }

  for (tmp = material->layers; tmp != NULL; tmp = tmp->next)
    {
      CoglMaterialLayer *layer = tmp->data;
      CoglTextureUnit   *unit = _cogl_get_texture_unit (layer->unit_index);

      unit->layer = layer;
      unit->layer_differences = layer->differences;
    }

  /* NB: _cogl_material_pre_change_notify and _cogl_material_free will
   * invalidate ctx->current_material (set it to COGL_INVALID_HANDLE)
   * if the material is changed/freed.
   */
  ctx->current_material = handle;
  ctx->current_material_flags = material->flags;
  ctx->current_material_fallback_layers = fallback_layers;
  ctx->current_material_disable_layers = disable_layers;
  ctx->current_material_layer0_override = layer0_override_texture;
  ctx->current_material_skip_gl_color = skip_gl_color;

done: /* well, almost... */

  /* Handle the fact that OpenGL associates texture filter and wrap
   * modes with the texture objects not the texture units... */
  foreach_texture_unit_update_filter_and_wrap_modes (wrap_mode_overrides);

  /* If this material has more than one layer then we always need
   * to make sure we rebind the texture for unit 1.
   *
   * NB: various components of Cogl may temporarily bind arbitrary
   * textures to the current texture unit so they can query and modify
   * texture object parameters. cogl-material.c will always leave
   * texture unit 1 active so we can ignore these temporary binds
   * unless multitexturing is being used.
   */
  unit1 = _cogl_get_texture_unit (1);
  if (unit1->enabled && unit1->dirty_gl_texture)
    {
      set_active_texture_unit (1);
      GE (glBindTexture (unit1->enabled_gl_target, unit1->gl_texture));
      unit1->dirty_gl_texture = FALSE;
    }

  /* Since there are several places where Cogl will temporarily bind a
   * GL texture so that it can query or modify texture objects we want
   * to make sure we know which texture unit state is being changed by
   * such code.
   *
   * We choose to always end up with texture unit 1 active so that in
   * the common case where multitexturing isn't used we can simply
   * ignore the state of this texture unit. Notably we didn't use a
   * large texture unit (.e.g. (GL_MAX_TEXTURE_UNITS - 1) in case the
   * driver doesn't have a sparse data structure for texture units.
   */
  set_active_texture_unit (1);
}

static gboolean
_cogl_material_texture_equal (CoglHandle texture0, CoglHandle texture1)
{
  GLenum gl_handle0, gl_handle1, gl_target0, gl_target1;

  /* If the texture handles are the same then the textures are
     definitely equal */
  if (texture0 == texture1)
    return TRUE;

  /* If neither texture is sliced then they could still be the same if
     the are referring to the same GL texture */
  if (cogl_texture_is_sliced (texture0) ||
      cogl_texture_is_sliced (texture1))
    return FALSE;

  cogl_texture_get_gl_texture (texture0, &gl_handle0, &gl_target0);
  cogl_texture_get_gl_texture (texture1, &gl_handle1, &gl_target1);

  return gl_handle0 == gl_handle1 && gl_target0 == gl_target1;
}

static gboolean
_cogl_material_layer_equal (CoglMaterialLayer *material0_layer,
                            CoglHandle         material0_layer_texture,
                            CoglMaterialLayer *material1_layer,
                            CoglHandle         material1_layer_texture)
{
  if (!_cogl_material_texture_equal (material0_layer_texture,
                                     material1_layer_texture))
    return FALSE;

  if ((material0_layer->differences &
       COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE) !=
      (material1_layer->differences &
       COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE))
    return FALSE;

#if 0 /* TODO */
  if (!_deep_are_layer_combines_equal ())
    return FALSE;
#else
  if (!(material0_layer->differences & COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE))
    return FALSE;
#endif

  if (material0_layer->mag_filter != material1_layer->mag_filter)
    return FALSE;
  if (material0_layer->min_filter != material1_layer->min_filter)
    return FALSE;

  if (material0_layer->wrap_mode_s != material1_layer->wrap_mode_s ||
      material0_layer->wrap_mode_t != material1_layer->wrap_mode_t ||
      material0_layer->wrap_mode_r != material1_layer->wrap_mode_r)
    return FALSE;

  return TRUE;
}

/* This is used by the Cogl journal to compare materials so that it
 * can split up geometry that needs different OpenGL state.
 *
 * It is acceptable to have false negatives - although they will result
 * in redundant OpenGL calls that try and update the state.
 *
 * False positives aren't allowed.
 */
gboolean
_cogl_material_equal (CoglHandle material0_handle,
                      CoglMaterialFlushOptions *material0_flush_options,
                      CoglHandle material1_handle,
                      CoglMaterialFlushOptions *material1_flush_options)
{
  CoglMaterial          *material0;
  CoglMaterial          *material1;
  CoglMaterialFlushFlag  flush_flags0 = material0_flush_options->flags;
  CoglMaterialFlushFlag  flush_flags1 = material1_flush_options->flags;
  guint32                fallback_layers0;
  guint32                fallback_layers1;
  guint32                disable_layers0;
  guint32                disable_layers1;
  GList                 *l0, *l1;
  int                    i;

  /* Compare the flush options first; if they are equivalent then we
   * can potentially return quickly if the material handles then match. */


  /* The skip color option is used when the color of the material is being
   * submitted in a vertex array so cogl_material_flush_gl_state doesn't
   * need to call glColor.
   * - A skip gl color material following a non skip color material doesn't
   *   need a state change since putting a color in a vertex array (as done
   *   for skip color materials) would simply take precedence over one
   *   previously specified by glColor (as done for non skip color materials)
   * - A non skip color material following a skip color material also doesn't
   *   need a state change for the same reason.
   * - The problem is that a non skip color, followed by a skip color, followed
   *   by a non skip color does require a state change. Since we don't have
   *   enough contextual information here we currently return FALSE whenever
   *   the skip color option changes. */
  if ((flush_flags0 & COGL_MATERIAL_FLUSH_SKIP_GL_COLOR) !=
      (flush_flags1 & COGL_MATERIAL_FLUSH_SKIP_GL_COLOR))
    return FALSE;

  fallback_layers0 = flush_flags0 & COGL_MATERIAL_FLUSH_FALLBACK_MASK ?
    material0_flush_options->fallback_layers : 0;
  fallback_layers1 = flush_flags1 & COGL_MATERIAL_FLUSH_FALLBACK_MASK ?
    material1_flush_options->fallback_layers : 0;
  if (fallback_layers0 != fallback_layers1)
    return FALSE;

  disable_layers0 = flush_flags0 & COGL_MATERIAL_FLUSH_DISABLE_MASK ?
    material0_flush_options->disable_layers : 0;
  disable_layers1 = flush_flags1 & COGL_MATERIAL_FLUSH_DISABLE_MASK ?
    material1_flush_options->disable_layers : 0;
  if (disable_layers0 != disable_layers1)
    return FALSE;

  /* NB: Some unlikely false negatives are possible here. */
  if ((flush_flags0 & COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE) !=
      (flush_flags1 & COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE))
    return FALSE;

  if ((flush_flags0 & COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE) &&
      material0_flush_options->layer0_override_texture !=
      material1_flush_options->layer0_override_texture)
    return FALSE;

  /* If one has wrap mode overrides and the other doesn't then the
     materials are different */
  if (((flush_flags0 ^ flush_flags1) & COGL_MATERIAL_FLUSH_WRAP_MODE_OVERRIDES))
    return FALSE;
  /* If they both have overrides then we need to compare them */
  if ((flush_flags0 & COGL_MATERIAL_FLUSH_WRAP_MODE_OVERRIDES) &&
      memcmp (&material0_flush_options->wrap_mode_overrides,
              &material1_flush_options->wrap_mode_overrides,
              sizeof (CoglMaterialWrapModeOverrides)))
    return FALSE;

  /* Since we know the flush options match at this point, if the material
   * handles match then we know they are equivalent. */
  if (material0_handle == material1_handle)
    return TRUE;

  /* Now we need to look in more detail... */

  material0 = _cogl_material_pointer_from_handle (material0_handle);
  material1 = _cogl_material_pointer_from_handle (material1_handle);

  if (!(material0_flush_options->flags & COGL_MATERIAL_FLUSH_SKIP_GL_COLOR) &&
      !memcmp (material0->unlit, material1->unlit, sizeof (material0->unlit)))
    return FALSE;

  /* First we simply try and find a difference according to default flags
   * for each material component to avoid deeper comparison. */

  if ((material0->flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL) !=
      (material1->flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL))
    return FALSE;

  if ((material0->flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC) !=
      (material1->flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC))
    return FALSE;

  /* Potentially blending could be "enabled" but the blend mode
   * could be equivalent to being disabled, but we accept those false
   * negatives for now. */
  if ((material0->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND) !=
      (material1->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND))
    return FALSE;

  if ((material0->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND) &&
      (material0->flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND) !=
      (material1->flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND))
    return FALSE;

  /* If we still haven't found a difference then do a deeper comparison..
   *
   * Actually we don't currently do this; we simply assume anything
   * non default is different and accept the false negatives for now.
   */

#if 0 /* TODO */
  if (!_deep_are_gl_materials_equal ())
    return FALSE;
#else
  /* Just assume that all non default materials are different */
  if (!(material0->flags & COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL))
    return FALSE;
#endif

#if 0 /* TODO */
  if (!_deep_are_alpha_funcs_equal ())
    return FALSE;
#else
  /* Just assume that all non default alpha funcs are different */
  if (!(material0->flags & COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC))
    return FALSE;
#endif

  if (material0->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND)
    {
#if 0 /* TODO */
      if (!_deep_is_blend_equal ())
        return FALSE;
#else
      if (!(material0->flags & COGL_MATERIAL_FLAG_DEFAULT_BLEND))
        return FALSE;
#endif
    }


  /* Finally compare each of the material layers ... */

  l0 = material0->layers;
  l1 = material1->layers;
  i = 0;

  /* NB: At this point we know if COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE is being
   * used then both materials are overriding with the same texture. */
  if (flush_flags0 & COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE &&
      l0 && l1)
    {
      /* We still need to check if the combine modes etc are equal, but we
       * simply pass COGL_INVALID_HANDLE for both texture handles so they will
       * be considered equal */
      if (!_cogl_material_layer_equal (l0->data, COGL_INVALID_HANDLE,
                                       l1->data, COGL_INVALID_HANDLE))
        return FALSE;

      l0 = l0->next;
      l1 = l1->next;
      i++;
    }

  while (l0 && l1)
    {
      CoglMaterialLayer *m0_layer;
      CoglMaterialLayer *m1_layer;

      if ((l0 == NULL && l1 != NULL) ||
          (l1 == NULL && l0 != NULL))
        return FALSE;

      /* NB: At this point we know that the fallback and disable masks for
       * both materials are equal. */
      if (disable_layers0 & (1<<i))
        goto next_layer;

      m0_layer = l0->data;
      m1_layer = l1->data;

      /* NB: The use of a fallback texture doesn't imply that the combine
       * modes etc are the same.
       */
      if ((disable_layers0 & (1<<i)) || (fallback_layers0 & (1<<i)))
        {
          /* As with layer0 overrides, we simply pass COGL_INVALID_HANDLEs for
           * both texture handles here so they will be considered equal. */
          if (!_cogl_material_layer_equal (m0_layer, COGL_INVALID_HANDLE,
                                           m1_layer, COGL_INVALID_HANDLE))
            return FALSE;
        }
      else
        {
          if (!_cogl_material_layer_equal (m0_layer, m0_layer->texture,
                                           m1_layer, m1_layer->texture))
            return FALSE;
        }

next_layer:
      l0 = l0->next;
      l1 = l1->next;
      i++;
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
                                 int                layer_index,
                                 CoglMaterialFilter min_filter,
                                 CoglMaterialFilter mag_filter)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  /* possibly flush primitives referencing the current state... */
  _cogl_material_layer_pre_change_notify (
                                      layer,
                                      COGL_MATERIAL_LAYER_CHANGE_FILTERS);

  layer->min_filter = min_filter;
  layer->mag_filter = mag_filter;

  /* Note we don't have a layer->difference flag for the min/mag
   * filters since in GL terms this state is owned by the texture
   * object so they are dealt with slightly differently. */
}

void
cogl_material_set_layer_wrap_mode_s (CoglHandle handle,
                                     int layer_index,
                                     CoglMaterialWrapMode mode)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  if (layer->wrap_mode_s != mode)
    {
      /* possibly flush primitives referencing the current state... */
      _cogl_material_pre_change_notify (material, FALSE, NULL);

      layer->wrap_mode_s = mode;
    }
}

void
cogl_material_set_layer_wrap_mode_t (CoglHandle           handle,
                                     int                  layer_index,
                                     CoglMaterialWrapMode mode)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  if (layer->wrap_mode_t != mode)
    {
      /* possibly flush primitives referencing the current state... */
      _cogl_material_pre_change_notify (material, FALSE, NULL);

      layer->wrap_mode_t = mode;
    }
}

/* TODO: this should be made public once we add support for 3D
   textures in Cogl */
void
_cogl_material_set_layer_wrap_mode_r (CoglHandle           handle,
                                      int                  layer_index,
                                      CoglMaterialWrapMode mode)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, TRUE);

  if (layer->wrap_mode_r != mode)
    {
      /* possibly flush primitives referencing the current state... */
      _cogl_material_pre_change_notify (material, FALSE, NULL);

      layer->wrap_mode_r = mode;
    }
}

void
cogl_material_set_layer_wrap_mode (CoglHandle           material,
                                   int                  layer_index,
                                   CoglMaterialWrapMode mode)
{
  cogl_material_set_layer_wrap_mode_s (material, layer_index, mode);
  cogl_material_set_layer_wrap_mode_t (material, layer_index, mode);
  _cogl_material_set_layer_wrap_mode_r (material, layer_index, mode);
}

CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_s (CoglHandle handle)
{
  g_return_val_if_fail (cogl_is_material_layer (handle), FALSE);

  return _cogl_material_layer_pointer_from_handle (handle)->wrap_mode_s;
}

CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_t (CoglHandle handle)
{
  g_return_val_if_fail (cogl_is_material_layer (handle), FALSE);

  return _cogl_material_layer_pointer_from_handle (handle)->wrap_mode_t;
}

/* TODO: this should be made public once we add support for 3D
   textures in Cogl */
CoglMaterialWrapMode
_cogl_material_layer_get_wrap_mode_r (CoglHandle handle)
{
  g_return_val_if_fail (cogl_is_material_layer (handle), FALSE);

  return _cogl_material_layer_pointer_from_handle (handle)->wrap_mode_r;
}

void
_cogl_material_apply_legacy_state (CoglHandle handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_program)
    {
      /* It was a mistake that we ever copied the OpenGL style API for
       * making a program current (cogl_program_use) on the context.
       * Until cogl_program_use is removed we will transparently set
       * the program on the material because the cogl-material code is
       * in the best position to juggle the corresponding GL state. */
      _cogl_material_set_user_program (handle,
                                       ctx->current_program);
    }
}

