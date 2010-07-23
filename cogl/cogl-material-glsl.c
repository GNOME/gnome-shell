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

#include "cogl-material-private.h"

#ifdef COGL_MATERIAL_BACKEND_GLSL

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#ifndef HAVE_COGL_GLES
#include "cogl-program.h"
#endif

#include <glib.h>

/*
 * GL/GLES compatability defines for material thingies:
 */

#ifdef HAVE_COGL_GLES2
#include "../gles/cogl-gles2-wrapper.h"
#endif

static int
_cogl_material_backend_glsl_get_max_texture_units (void)
{
  return _cogl_get_max_texture_image_units ();
}

static gboolean
_cogl_material_backend_glsl_start (CoglMaterial *material,
                                   int n_layers,
                                   unsigned long materials_difference)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    return FALSE;

  if (materials_difference & COGL_MATERIAL_STATE_USER_SHADER)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material,
                                      COGL_MATERIAL_STATE_USER_SHADER);
      CoglHandle program = authority->big_state->user_program;

      if (program == COGL_INVALID_HANDLE)
        return FALSE; /* XXX: change me when we support code generation here */

      _cogl_use_program (program, COGL_MATERIAL_PROGRAM_TYPE_GLSL);
      return TRUE;
    }

  /* TODO: also support code generation */

  return FALSE;
}

gboolean
_cogl_material_backend_glsl_add_layer (CoglMaterial *material,
                                       CoglMaterialLayer *layer,
                                       unsigned long layers_difference)
{
  return TRUE;
}

gboolean
_cogl_material_backend_glsl_passthrough (CoglMaterial *material)
{
  return TRUE;
}

gboolean
_cogl_material_backend_glsl_end (CoglMaterial *material,
                                 unsigned long materials_difference)
{
  return TRUE;
}

const CoglMaterialBackend _cogl_material_glsl_backend =
{
  _cogl_material_backend_glsl_get_max_texture_units,
  _cogl_material_backend_glsl_start,
  _cogl_material_backend_glsl_add_layer,
  _cogl_material_backend_glsl_passthrough,
  _cogl_material_backend_glsl_end,
  NULL, /* material_state_change_notify */
  NULL, /* material_set_parent_notify */
  NULL, /* layer_state_change_notify */
  NULL, /* free_priv */
};

#endif /* COGL_MATERIAL_BACKEND_GLSL */

