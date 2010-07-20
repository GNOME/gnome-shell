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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
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

#include "cogl-material-opengl-private.h"
#include "cogl-material-private.h"
#include "cogl-context.h"
#include "cogl-texture-private.h"
#ifndef HAVE_COGL_GLES
#include "cogl-program.h"
#endif

#ifdef COGL_MATERIAL_BACKEND_GLSL
#include "cogl-material-glsl-private.h"
#endif
#ifdef COGL_MATERIAL_BACKEND_ARBFP
#include "cogl-material-arbfp-private.h"
#endif
#ifdef COGL_MATERIAL_BACKEND_FIXED
#include "cogl-material-fixed-private.h"
#endif

#include <glib.h>
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

/* These aren't defined in the GLES headers */
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_COORD_REPLACE
#define GL_COORD_REPLACE 0x8862
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif


static void
texture_unit_init (CoglTextureUnit *unit, int index_)
{
  unit->index = index_;
  unit->enabled = FALSE;
  unit->current_gl_target = 0;
  unit->gl_texture = 0;
  unit->is_foreign = FALSE;
  unit->dirty_gl_texture = FALSE;
  unit->matrix_stack = _cogl_matrix_stack_new ();

  unit->layer = NULL;
  unit->layer_changes_since_flush = 0;
  unit->texture_storage_changed = FALSE;
}

static void
texture_unit_free (CoglTextureUnit *unit)
{
  if (unit->layer)
    cogl_object_unref (unit->layer);
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

void
_cogl_set_active_texture_unit (int unit_index)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->active_texture_unit != unit_index)
    {
      GE (glActiveTexture (GL_TEXTURE0 + unit_index));
      ctx->active_texture_unit = unit_index;
    }
}

void
_cogl_disable_texture_unit (int unit_index)
{
  CoglTextureUnit *unit;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  unit = &g_array_index (ctx->texture_units, CoglTextureUnit, unit_index);

  if (unit->enabled)
    {
      _cogl_set_active_texture_unit (unit_index);
      GE (glDisable (unit->current_gl_target));
      unit->enabled = FALSE;
    }
}

/* Note: _cogl_bind_gl_texture_transient conceptually has slightly
 * different semantics to OpenGL's glBindTexture because Cogl never
 * cares about tracking multiple textures bound to different targets
 * on the same texture unit.
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

  /* We choose to always make texture unit 1 active for transient
   * binds so that in the common case where multitexturing isn't used
   * we can simply ignore the state of this texture unit. Notably we
   * didn't use a large texture unit (.e.g. (GL_MAX_TEXTURE_UNITS - 1)
   * in case the driver doesn't have a sparse data structure for
   * texture units.
   */
  _cogl_set_active_texture_unit (1);
  unit = _cogl_get_texture_unit (1);

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

  GE (glDeleteTextures (1, &gl_texture));
}

/* Whenever the underlying GL texture storage of a CoglTexture is
 * changed (e.g. due to migration out of a texture atlas) then we are
 * notified. This lets us ensure that we reflush that texture's state
 * if it reused again with the same texture unit.
 */
void
_cogl_material_texture_storage_change_notify (CoglHandle texture)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (unit->layer &&
          unit->layer->texture == texture)
        unit->texture_storage_changed = TRUE;

      /* NB: the texture may be bound to multiple texture units so
       * we continue to check the rest */
    }
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
disable_arbfp (void)
{
#ifdef COGL_MATERIAL_BACKEND_ARBFP
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_ARBFP)
    GE (glDisable (GL_FRAGMENT_PROGRAM_ARB));
#endif
}

void
_cogl_use_program (CoglHandle program_handle, CoglMaterialProgramType type)
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
      _cogl_gl_use_program_wrapper (0);

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
      _cogl_gl_use_program_wrapper (0);

      if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_FIXED)
        break;

      disable_arbfp ();

      ctx->current_use_program_type = type;
#endif
    }
}

#if defined (COGL_MATERIAL_BACKEND_GLSL) || \
    defined (COGL_MATERIAL_BACKEND_ARBFP)
int
_cogl_get_max_texture_image_units (void)
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
_cogl_material_layer_get_texture_info (CoglMaterialLayer *layer,
                                       CoglHandle *texture,
                                       GLuint *gl_texture,
                                       GLuint *gl_target)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  *texture = layer->texture;
  if (G_UNLIKELY (*texture == COGL_INVALID_HANDLE))
    *texture = ctx->default_gl_texture_2d_tex;
  if (layer->texture_overridden)
    {
      *gl_texture = layer->slice_gl_texture;
      *gl_target = layer->slice_gl_target;
    }
  else
    cogl_texture_get_gl_texture (*texture, gl_texture, gl_target);
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
flush_depth_state (CoglMaterialDepthState *depth_state)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->depth_test_function_cache != depth_state->depth_test_function)
    {
      GE (glDepthFunc (depth_state->depth_test_function));
      ctx->depth_test_function_cache = depth_state->depth_test_function;
    }

  if (ctx->depth_writing_enabled_cache != depth_state->depth_writing_enabled)
    {
      GE (glDepthMask (depth_state->depth_writing_enabled ?
                       GL_TRUE : GL_FALSE));
      ctx->depth_writing_enabled_cache = depth_state->depth_writing_enabled;
    }

#ifndef COGL_HAS_GLES
  if (ctx->depth_range_near_cache != depth_state->depth_range_near ||
      ctx->depth_range_far_cache != depth_state->depth_range_far)
    {
#ifdef COGL_HAS_GLES2
      GE (glDepthRangef (depth_state->depth_range_near,
                         depth_state->depth_range_far));
#else
      GE (glDepthRange (depth_state->depth_range_near,
                        depth_state->depth_range_far));
#endif
      ctx->depth_range_near_cache = depth_state->depth_range_near;
      ctx->depth_range_far_cache = depth_state->depth_range_far;
    }
#endif /* COGL_HAS_GLES */
}

static void
_cogl_material_flush_color_blend_alpha_depth_state (
                                            CoglMaterial *material,
                                            unsigned long materials_difference,
                                            gboolean      skip_gl_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!skip_gl_color)
    {
      if ((materials_difference & COGL_MATERIAL_STATE_COLOR) ||
          /* Assume if we were previously told to skip the color, then
           * the current color needs updating... */
          ctx->current_material_skip_gl_color)
        {
          CoglMaterial *authority =
            _cogl_material_get_authority (material, COGL_MATERIAL_STATE_COLOR);
          GE (glColor4ub (cogl_color_get_red_byte (&authority->color),
                          cogl_color_get_green_byte (&authority->color),
                          cogl_color_get_blue_byte (&authority->color),
                          cogl_color_get_alpha_byte (&authority->color)));
        }
    }

  if (materials_difference & COGL_MATERIAL_STATE_LIGHTING)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LIGHTING);
      CoglMaterialLightingState *lighting_state =
        &authority->big_state->lighting_state;

      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, lighting_state->ambient));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, lighting_state->diffuse));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, lighting_state->specular));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, lighting_state->emission));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS,
                        &lighting_state->shininess));
    }

  if (materials_difference & COGL_MATERIAL_STATE_BLEND)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_BLEND);
      CoglMaterialBlendState *blend_state =
        &authority->big_state->blend_state;

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
      if (blend_factor_uses_constant (blend_state->blend_src_factor_rgb) ||
          blend_factor_uses_constant (blend_state->blend_src_factor_alpha) ||
          blend_factor_uses_constant (blend_state->blend_dst_factor_rgb) ||
          blend_factor_uses_constant (blend_state->blend_dst_factor_alpha))
        {
          float red =
            cogl_color_get_red_float (&blend_state->blend_constant);
          float green =
            cogl_color_get_green_float (&blend_state->blend_constant);
          float blue =
            cogl_color_get_blue_float (&blend_state->blend_constant);
          float alpha =
            cogl_color_get_alpha_float (&blend_state->blend_constant);


          GE (glBlendColor (red, green, blue, alpha));
        }

      if (have_blend_equation_seperate &&
          blend_state->blend_equation_rgb != blend_state->blend_equation_alpha)
        GE (glBlendEquationSeparate (blend_state->blend_equation_rgb,
                                     blend_state->blend_equation_alpha));
      else
        GE (glBlendEquation (blend_state->blend_equation_rgb));

      if (have_blend_func_separate &&
          (blend_state->blend_src_factor_rgb != blend_state->blend_src_factor_alpha ||
           (blend_state->blend_src_factor_rgb !=
            blend_state->blend_src_factor_alpha)))
        GE (glBlendFuncSeparate (blend_state->blend_src_factor_rgb,
                                 blend_state->blend_dst_factor_rgb,
                                 blend_state->blend_src_factor_alpha,
                                 blend_state->blend_dst_factor_alpha));
      else
#endif
        GE (glBlendFunc (blend_state->blend_src_factor_rgb,
                         blend_state->blend_dst_factor_rgb));
    }

  if (materials_difference & COGL_MATERIAL_STATE_ALPHA_FUNC)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_ALPHA_FUNC);
      CoglMaterialAlphaFuncState *alpha_state =
        &authority->big_state->alpha_state;

      /* NB: Currently the Cogl defines are compatible with the GL ones: */
      GE (glAlphaFunc (alpha_state->alpha_func,
                       alpha_state->alpha_func_reference));
    }

  if (materials_difference & COGL_MATERIAL_STATE_DEPTH)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_DEPTH);
      CoglMaterialDepthState *depth_state = &authority->big_state->depth_state;

      if (depth_state->depth_test_enabled)
        {
          if (ctx->depth_test_enabled_cache != TRUE)
            {
              GE (glEnable (GL_DEPTH_TEST));
              ctx->depth_test_enabled_cache = depth_state->depth_test_enabled;
            }
          flush_depth_state (depth_state);
        }
      else if (ctx->depth_test_enabled_cache != FALSE)
        {
          GE (glDisable (GL_DEPTH_TEST));
          ctx->depth_test_enabled_cache = depth_state->depth_test_enabled;
        }
    }

  if (materials_difference & COGL_MATERIAL_STATE_POINT_SIZE)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_POINT_SIZE);

      if (ctx->point_size_cache != authority->big_state->point_size)
        {
          GE( glPointSize (authority->big_state->point_size) );
          ctx->point_size_cache = authority->big_state->point_size;
        }
    }

  if (material->real_blend_enable != ctx->gl_blend_enable_cache)
    {
      if (material->real_blend_enable)
        GE (glEnable (GL_BLEND));
      else
        GE (glDisable (GL_BLEND));
      /* XXX: we shouldn't update any other blend state if blending
       * is disabled! */
      ctx->gl_blend_enable_cache = material->real_blend_enable;
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

typedef struct
{
  int i;
  unsigned long *layer_differences;
} CoglMaterialFlushLayerState;

static gboolean
flush_layers_common_gl_state_cb (CoglMaterialLayer *layer, void *user_data)
{
  CoglMaterialFlushLayerState *flush_state = user_data;
  int                          unit_index = flush_state->i;
  CoglTextureUnit             *unit = _cogl_get_texture_unit (unit_index);
  unsigned long                layers_difference =
    flush_state->layer_differences[unit_index];

  /* There may not be enough texture units so we can bail out if
   * that's the case...
   */
  if (G_UNLIKELY (unit_index >= get_max_activateable_texture_units ()))
    {
      static gboolean shown_warning = FALSE;

      if (!shown_warning)
        {
          g_warning ("Your hardware does not have enough texture units"
                     "to handle this many texture layers");
          shown_warning = TRUE;
        }
      return FALSE;
    }

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_TEXTURE)
    {
      CoglMaterialLayer *authority =
        _cogl_material_layer_get_authority (layer,
                                            COGL_MATERIAL_LAYER_STATE_TEXTURE);
      CoglHandle texture = NULL;
      GLuint     gl_texture;
      GLenum     gl_target;

      _cogl_material_layer_get_texture_info (authority,
                                             &texture,
                                             &gl_texture,
                                             &gl_target);

      _cogl_set_active_texture_unit (unit_index);

      /* NB: There are several Cogl components and some code in
       * Clutter that will temporarily bind arbitrary GL textures to
       * query and modify texture object parameters. If you look at
       * _cogl_bind_gl_texture_transient() you can see we make sure
       * that such code always binds to texture unit 1 which means we
       * can't rely on the unit->gl_texture state if unit->index == 1.
       *
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
      if (unit->gl_texture != gl_texture || unit->is_foreign)
        {
          if (unit_index != 1)
            GE (glBindTexture (gl_target, gl_texture));
          unit->gl_texture = gl_texture;
        }

      unit->is_foreign = _cogl_texture_is_foreign (texture);

      /* Disable the previous target if it was different and it's
       * still enabled */
      if (unit->enabled && unit->current_gl_target != gl_target)
        GE (glDisable (unit->current_gl_target));

      if (!G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_TEXTURING) &&
          (!unit->enabled || unit->current_gl_target != gl_target))
        {
          GE (glEnable (gl_target));
          unit->enabled = TRUE;
          unit->current_gl_target = gl_target;
        }

      /* The texture_storage_changed boolean indicates if the
       * CoglTexture's underlying GL texture storage has changed since
       * it was flushed to the texture unit. We've just flushed the
       * latest state so we can reset this. */
      unit->texture_storage_changed = FALSE;
    }
  else
    {
      /* Even though there may be no difference between the last flushed
       * texture state and the current layers texture state it may be that the
       * texture unit has been disabled for some time so we need to assert that
       * it's enabled now.
       */
      if (!G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_TEXTURING) &&
          !unit->enabled)
        {
          GE (glEnable (unit->current_gl_target));
          unit->enabled = TRUE;
        }
    }

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_USER_MATRIX)
    {
      CoglMaterialLayerState state = COGL_MATERIAL_LAYER_STATE_USER_MATRIX;
      CoglMaterialLayer *authority =
        _cogl_material_layer_get_authority (layer, state);

      _cogl_matrix_stack_set (unit->matrix_stack,
                              &authority->big_state->matrix);

      _cogl_matrix_stack_flush_to_gl (unit->matrix_stack, COGL_MATRIX_TEXTURE);
    }

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS)
    {
      CoglMaterialState change = COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS;
      CoglMaterialLayer *authority =
        _cogl_material_layer_get_authority (layer, change);
      CoglMaterialLayerBigState *big_state = authority->big_state;

      _cogl_set_active_texture_unit (unit_index);

      GE (glTexEnvi (GL_POINT_SPRITE, GL_COORD_REPLACE,
                     big_state->point_sprite_coords));
    }

  cogl_handle_ref (layer);
  if (unit->layer != COGL_INVALID_HANDLE)
    cogl_handle_unref (unit->layer);

  unit->layer = layer;
  unit->layer_changes_since_flush = 0;

  flush_state->i++;

  return TRUE;
}

static void
_cogl_material_flush_common_gl_state (CoglMaterial  *material,
                                      unsigned long  materials_difference,
                                      unsigned long *layer_differences,
                                      gboolean       skip_gl_color)
{
  CoglMaterialFlushLayerState state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_material_flush_color_blend_alpha_depth_state (material,
                                                      materials_difference,
                                                      skip_gl_color);

  state.i = 0;
  state.layer_differences = layer_differences;
  _cogl_material_foreach_layer (material,
                                flush_layers_common_gl_state_cb,
                                &state);

  /* Disable additional texture units that may have previously been in use.. */
  for (; state.i < ctx->texture_units->len; state.i++)
    _cogl_disable_texture_unit (state.i);
}

/* Re-assert the layer's wrap modes on the given CoglTexture.
 *
 * Note: we don't simply forward the wrap modes to layer->texture
 * since the actual texture being used may have been overridden.
 */
static void
_cogl_material_layer_forward_wrap_modes (CoglMaterialLayer *layer,
                                         CoglHandle texture)
{
  CoglMaterialWrapModeInternal wrap_mode_s, wrap_mode_t, wrap_mode_p;
  GLenum gl_wrap_mode_s, gl_wrap_mode_t, gl_wrap_mode_p;

  if (texture == COGL_INVALID_HANDLE)
    return;

  _cogl_material_layer_get_wrap_modes (layer,
                                       &wrap_mode_s,
                                       &wrap_mode_t,
                                       &wrap_mode_p);

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

  if (wrap_mode_s == COGL_MATERIAL_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_s = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_s = wrap_mode_s;

  if (wrap_mode_t == COGL_MATERIAL_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_t = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_t = wrap_mode_t;

  if (wrap_mode_p == COGL_MATERIAL_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_p = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_p = wrap_mode_p;

  _cogl_texture_set_wrap_mode_parameters (texture,
                                          gl_wrap_mode_s,
                                          gl_wrap_mode_t,
                                          gl_wrap_mode_p);
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
foreach_texture_unit_update_filter_and_wrap_modes (void)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (!unit->enabled)
        break;

      if (unit->layer)
        {
          CoglHandle texture = _cogl_material_layer_get_texture (unit->layer);
          CoglMaterialFilter min;
          CoglMaterialFilter mag;

          _cogl_material_layer_get_filters (unit->layer, &min, &mag);
          _cogl_texture_set_filters (texture, min, mag);

          _cogl_material_layer_forward_wrap_modes (unit->layer, texture);
        }
    }
}

typedef struct
{
  int i;
  unsigned long *layer_differences;
} CoglMaterialCompareLayersState;

static gboolean
compare_layer_differences_cb (CoglMaterialLayer *layer, void *user_data)
{
  CoglMaterialCompareLayersState *state = user_data;
  CoglTextureUnit *unit = _cogl_get_texture_unit (state->i);

  if (unit->layer == layer)
    state->layer_differences[state->i] = unit->layer_changes_since_flush;
  else if (unit->layer)
    {
      state->layer_differences[state->i] = unit->layer_changes_since_flush;
      state->layer_differences[state->i] |=
        _cogl_material_layer_compare_differences (layer, unit->layer);
    }
  else
    state->layer_differences[state->i] = COGL_MATERIAL_LAYER_STATE_ALL_SPARSE;

  /* XXX: There is always a possibility that a CoglTexture's
   * underlying GL texture storage has been changed since it was last
   * bound to a texture unit which is why we have a callback into
   * _cogl_material_texture_storage_change_notify whenever a textures
   * underlying GL texture storage changes which will set the
   * unit->texture_intern_changed flag. If we see that's been set here
   * then we force an update of the texture state...
   */
  if (unit->texture_storage_changed)
    state->layer_differences[state->i] |= COGL_MATERIAL_LAYER_STATE_TEXTURE;

  state->i++;

  return TRUE;
}

typedef struct
{
  const CoglMaterialBackend *backend;
  CoglMaterial *material;
  unsigned long *layer_differences;
  gboolean error_adding_layer;
  gboolean added_layer;
} CoglMaterialBackendAddLayerState;


static gboolean
backend_add_layer_cb (CoglMaterialLayer *layer,
                      void *user_data)
{
  CoglMaterialBackendAddLayerState *state = user_data;
  const CoglMaterialBackend *backend = state->backend;
  CoglMaterial *material = state->material;
  int unit_index = _cogl_material_layer_get_unit_index (layer);
  CoglTextureUnit *unit = _cogl_get_texture_unit (unit_index);

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* NB: We don't support the random disabling of texture
   * units, so as soon as we hit a disabled unit we know all
   * subsequent units are also disabled */
  if (!unit->enabled)
    return FALSE;

  if (G_UNLIKELY (unit_index >= backend->get_max_texture_units ()))
    {
      int j;
      for (j = unit_index; j < ctx->texture_units->len; j++)
        _cogl_disable_texture_unit (j);
      /* TODO: although this isn't considered an error that
       * warrants falling back to a different backend we
       * should print a warning here. */
      return FALSE;
    }

  /* Either generate per layer code snippets or setup the
   * fixed function glTexEnv for each layer... */
  if (G_LIKELY (backend->add_layer (material,
                                    layer,
                                    state->layer_differences[unit_index])))
    state->added_layer = TRUE;
  else
    {
      state->error_adding_layer = TRUE;
      return FALSE;
    }

  return TRUE;
}

/*
 * _cogl_material_flush_gl_state:
 *
 * Details of override options:
 * ->fallback_mask: is a bitmask of the material layers that need to be
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
 * ->disable_mask: is a bitmask of the material layers that will simply have
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
 * ->layer0_override_texture: forcibly tells us to bind this GL texture name for
 *    layer 0 instead of plucking the gl_texture from the CoglTexture of layer
 *    0.
 *
 *    The intention of this is for any primitives that supports sliced textures.
 *    The code will can iterate each of the slices and re-flush the material
 *    forcing the GL texture of each slice in turn.
 *
 * ->wrap_mode_overrides: overrides the wrap modes set on each
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
void
_cogl_material_flush_gl_state (CoglMaterial *material,
                               gboolean skip_gl_color)
{
  unsigned long    materials_difference;
  int              n_layers;
  unsigned long   *layer_differences = NULL;
  int              i;
  CoglTextureUnit *unit1;

  COGL_STATIC_TIMER (material_flush_timer,
                     "Mainloop", /* parent */
                     "Material Flush",
                     "The time spent flushing material state",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, material_flush_timer);

  if (ctx->current_material == material)
    materials_difference = ctx->current_material_changes_since_flush;
  else if (ctx->current_material)
    {
      materials_difference = ctx->current_material_changes_since_flush;
      materials_difference |=
        _cogl_material_compare_differences (ctx->current_material,
                                            material);
    }
  else
    materials_difference = COGL_MATERIAL_STATE_ALL_SPARSE;

  /* Get a layer_differences mask for each layer to be flushed */
  n_layers = cogl_material_get_n_layers (material);
  if (n_layers)
    {
      CoglMaterialCompareLayersState state;
      layer_differences = g_alloca (sizeof (unsigned long *) * n_layers);
      memset (layer_differences, 0, sizeof (layer_differences));
      state.i = 0;
      state.layer_differences = layer_differences;
      _cogl_material_foreach_layer (material,
                                    compare_layer_differences_cb,
                                    &state);
    }

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
                                        materials_difference,
                                        layer_differences,
                                        skip_gl_color);

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
       i < G_N_ELEMENTS (_cogl_material_backends);
       i++, _cogl_material_set_backend (material, i))
    {
      const CoglMaterialBackend *backend = _cogl_material_backends[i];
      CoglMaterialBackendAddLayerState state;

      /* E.g. For backends generating code they can setup their
       * scratch buffers here... */
      if (G_UNLIKELY (!backend->start (material,
                                       n_layers,
                                       materials_difference)))
        continue;

      state.backend = backend;
      state.material = material;
      state.layer_differences = layer_differences;
      state.error_adding_layer = FALSE;
      state.added_layer = FALSE;
      _cogl_material_foreach_layer (material,
                                    backend_add_layer_cb,
                                    &state);

      if (G_UNLIKELY (state.error_adding_layer))
        continue;

      if (!state.added_layer &&
          backend->passthrough &&
          G_UNLIKELY (!backend->passthrough (material)))
        continue;

      /* For backends generating code they may compile and link their
       * programs here, update any uniforms and tell OpenGL to use
       * that program.
       */
      if (G_UNLIKELY (!backend->end (material, materials_difference)))
        continue;

      break;
    }

  /* FIXME: This reference is actually resulting in lots of
   * copy-on-write reparenting because one-shot materials end up
   * living for longer than necessary and so any later modification of
   * the parent will cause a copy-on-write.
   *
   * XXX: The issue should largely go away when we switch to using
   * weak materials for overrides.
   */
  cogl_object_ref (material);
  if (ctx->current_material != NULL)
    cogl_object_unref (ctx->current_material);
  ctx->current_material = material;
  ctx->current_material_changes_since_flush = 0;
  ctx->current_material_skip_gl_color = skip_gl_color;

  /* Handle the fact that OpenGL associates texture filter and wrap
   * modes with the texture objects not the texture units... */
  foreach_texture_unit_update_filter_and_wrap_modes ();

  /* If this material has more than one layer then we always need
   * to make sure we rebind the texture for unit 1.
   *
   * NB: various components of Cogl may temporarily bind arbitrary
   * textures to texture unit 1 so they can query and modify texture
   * object parameters. cogl-material.c (See
   * _cogl_bind_gl_texture_transient)
   */
  unit1 = _cogl_get_texture_unit (1);
  if (unit1->enabled && unit1->dirty_gl_texture)
    {
      _cogl_set_active_texture_unit (1);
      GE (glBindTexture (unit1->current_gl_target, unit1->gl_texture));
      unit1->dirty_gl_texture = FALSE;
    }

  COGL_TIMER_STOP (_cogl_uprof_context, material_flush_timer);
}

