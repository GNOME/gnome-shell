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


