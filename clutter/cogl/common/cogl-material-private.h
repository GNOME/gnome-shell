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

#ifndef __COGL_MATERIAL_PRIVATE_H
#define __COGL_MATERIAL_PRIVATE_H

#include "cogl-material.h"
#include "cogl-matrix.h"
#include "cogl-handle.h"

#include <glib.h>

typedef struct _CoglMaterial	      CoglMaterial;
typedef struct _CoglMaterialLayer     CoglMaterialLayer;

/* XXX: I don't think gtk-doc supports having private enums so these aren't
 * bundled in with CoglMaterialLayerFlags */
typedef enum _CoglMaterialLayerPrivFlags
{
  /* Ref: CoglMaterialLayerFlags
  COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX  = 1L<<0
  */
  COGL_MATERIAL_LAYER_FLAG_DIRTY            = 1L<<1,
  COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE  = 1L<<2
} CoglMaterialLayerPrivFlags;

/* For tracking the state of a layer that's been flushed to OpenGL */
typedef struct _CoglLayerInfo
{
  CoglHandle  handle;
  gulong      flags;
  GLenum      gl_target;
  GLuint      gl_texture;
  gboolean    fallback;
  gboolean    disabled;
  gboolean    layer0_overridden;
} CoglLayerInfo;

struct _CoglMaterialLayer
{
  CoglHandleObject _parent;
  guint	       index;	/*!< lowest index is blended first then others
			     on top */
  gulong       flags;
  CoglHandle   texture;	/*!< The texture for this layer, or COGL_INVALID_HANDLE
			     for an empty layer */

  /* Determines how the color of individual texture fragments
   * are calculated. */
  GLint texture_combine_rgb_func;
  GLint texture_combine_rgb_src[3];
  GLint texture_combine_rgb_op[3];

  GLint texture_combine_alpha_func;
  GLint texture_combine_alpha_src[3];
  GLint texture_combine_alpha_op[3];

  GLfloat texture_combine_constant[4];

  /* TODO: Support purely GLSL based material layers */

  CoglMatrix matrix;
};

typedef enum _CoglMaterialFlags
{
  COGL_MATERIAL_FLAG_ENABLE_BLEND	    = 1L<<0,
  COGL_MATERIAL_FLAG_SHOWN_SAMPLER_WARNING  = 1L<<1,
  COGL_MATERIAL_FLAG_DEFAULT_COLOR          = 1L<<2,
  COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL    = 1L<<3,
  COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC     = 1L<<4,
  COGL_MATERIAL_FLAG_DEFAULT_BLEND_FUNC     = 1L<<5
} CoglMaterialFlags;

struct _CoglMaterial
{
  CoglHandleObject _parent;

  gulong    flags;

  /* If no lighting is enabled; this is the basic material color */
  GLfloat   unlit[4];

  /* Standard OpenGL lighting model attributes */
  GLfloat   ambient[4];
  GLfloat   diffuse[4];
  GLfloat   specular[4];
  GLfloat   emission[4];
  GLfloat   shininess;

  /* Determines what fragments are discarded based on their alpha */
  CoglMaterialAlphaFunc alpha_func;
  GLfloat		alpha_func_reference;

  /* Determines how this material is blended with other primitives */
#ifndef HAVE_COGL_GLES
  GLenum blend_equation_rgb;
  GLenum blend_equation_alpha;
  GLint blend_src_factor_alpha;
  GLint blend_dst_factor_alpha;
  GLfloat blend_constant[4];
#endif
  GLint blend_src_factor_rgb;
  GLint blend_dst_factor_rgb;

  GList	   *layers;
};

/*
 * SECTION:cogl-material-internals
 * @short_description: Functions for creating custom primitives that make use
 *                     of Cogl materials for filling.
 *
 * Normally you shouldn't need to use this API directly, but if you need to
 * developing a custom/specialised primitive - probably using raw OpenGL - then
 * this API aims to expose enough of the material internals to support being
 * able to fill your geometry according to a given Cogl material.
 */


/*
 * cogl_material_get_cogl_enable_flags:
 * @material: A CoglMaterial object
 *
 * This determines what flags need to be passed to cogl_enable before this
 * material can be used. Normally you shouldn't need to use this function
 * directly since Cogl will do this internally, but if you are developing
 * custom primitives directly with OpenGL you may want to use this.
 *
 * Note: This API is hopfully just a stop-gap solution. Ideally cogl_enable
 * will be replaced.
 */
/* TODO: find a nicer solution! */
gulong _cogl_material_get_cogl_enable_flags (CoglHandle handle);

/*
 * CoglMaterialLayerFlags:
 * @COGL_MATERIAL_LAYER_FLAG_USER_MATRIX: Means the user has supplied a
 *                                        custom texture matrix.
 */
typedef enum _CoglMaterialLayerFlags
{
  COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX	= 1L<<0
} CoglMaterialLayerFlags;
/* XXX: NB: if you add flags here you will need to update
 * CoglMaterialLayerPrivFlags!!! */

/*
 * cogl_material_layer_get_flags:
 * @layer_handle: A CoglMaterialLayer layer handle
 *
 * This lets you get a number of flag attributes about the layer.  Normally
 * you shouldn't need to use this function directly since Cogl will do this
 * internally, but if you are developing custom primitives directly with
 * OpenGL you may need this.
 */
gulong _cogl_material_layer_get_flags (CoglHandle layer_handle);

/*
 * CoglMaterialFlushOption:
 * @COGL_MATERIAL_FLUSH_FALLBACK_MASK: Follow this by a guin32 mask
 *      of the layers that can't be supported with the user supplied texture
 *      and need to be replaced with fallback textures. (1 = fallback, and the
 *      least significant bit = layer 0)
 * @COGL_MATERIAL_FLUSH_DISABLE_MASK: Follow this by a guint32 mask
 *      of the layers that you want to completly disable texturing for
 *      (1 = fallback, and the least significant bit = layer 0)
 * @COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE: Follow this by a GLuint OpenGL texture
 *      name to override the texture used for layer 0 of the material. This is
 *      intended for dealing with sliced textures where you will need to point
 *      to each of the texture slices in turn when drawing your geometry.
 *      Passing a value of 0 is the same as not passing the option at all.
 */
typedef enum _CoglMaterialFlushOption
{
  COGL_MATERIAL_FLUSH_FALLBACK_MASK = 1,
  COGL_MATERIAL_FLUSH_DISABLE_MASK,
  COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE,
} CoglMaterialFlushOption;

/*
 * cogl_material_flush_gl_state:
 * @material: A CoglMaterial object
 * @...: A NULL terminated list of (CoglMaterialFlushOption, data) pairs
 *
 * This function commits the state of the specified CoglMaterial - including
 * the texture state for all the layers - to the OpenGL[ES] driver.
 *
 * Since 1.0
 */
void _cogl_material_flush_gl_state (CoglHandle material,
                                    ...) G_GNUC_NULL_TERMINATED;


#endif /* __COGL_MATERIAL_PRIVATE_H */

