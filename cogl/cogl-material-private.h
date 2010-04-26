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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_MATERIAL_PRIVATE_H
#define __COGL_MATERIAL_PRIVATE_H

#include "cogl-material.h"
#include "cogl-matrix.h"
#include "cogl-matrix-stack.h"
#include "cogl-handle.h"

#include <glib.h>

typedef struct _CoglMaterial	      CoglMaterial;
typedef struct _CoglMaterialLayer     CoglMaterialLayer;


/*
 * cogl-material.c owns the GPU's texture unit state so we have some
 * private structures for describing the current state of a texture
 * unit that we track in a per context array (ctx->texture_units) that
 * grows according to the largest texture unit used so far...
 *
 * Roughly speaking the members in this structure are of two kinds:
 * either they are a low level reflection of the state we send to
 * OpenGL or they are for high level meta data assoicated with the
 * texture unit when flushing CoglMaterialLayers that is typically
 * used to optimize subsequent re-flushing of the same layer.
 *
 * The low level members are at the top, and the high level members
 * start with the .layer member.
 */
typedef struct _CoglTextureUnit
{
  /* The base 0 texture unit index which can be used with
   * glActiveTexture () */
  int                index;

  /* Whether or not the corresponding gl_target has been glEnabled */
  gboolean           enabled;

  /* The GL target currently glEnabled or 0 if .enabled == FALSE */
  GLenum             enabled_gl_target;

  /* The raw GL texture object name for which we called glBindTexture when
   * we flushed the last layer. (NB: The CoglTexture associated
   * with a layer may represent more than one GL texture) */
  GLuint             gl_texture;

  /* A matrix stack giving us the means to associate a texture
   * transform matrix with the texture unit. */
  CoglMatrixStack   *matrix_stack;

  /*
   * Higher level layer state associated with the unit...
   */

  /* The CoglMaterialLayer whos state was flushed to update this
   * texture unit last.
   *
   * This will be set to NULL if the layer is modified or freed which
   * means when we come to flush a layer; if this pointer is still
   * valid and == to the layer being flushed we don't need to update
   * any texture unit state. */
  CoglMaterialLayer *layer;

  /* To help minimize the state changes required we track the
   * difference flags associated with the layer whos state was last
   * flushed to update this texture unit.
   *
   * Note: we track this explicitly because .layer may get invalidated
   * if that layer is modified or deleted. Even if the layer is
   * invalidated though these flags can be used to optimize the state
   * flush of the next layer
   */
  unsigned long      layer_differences;

  /* The options that may have affected how the layer state updated
   * this texture unit. */
  gboolean           fallback;
  gboolean           layer0_overridden;

  /* When flushing a layers state, fallback options may mean that a
   * different CoglTexture is used than layer->texture.
   *
   * Once a layers state has been flushed we have to keep track of
   * changes to that layer so if we are asked to re-flush the same
   * layer later we will know what work is required. This also means
   * we need to keep track of changes to the CoglTexture of that layer
   * so we need to explicitly keep a reference to the final texture
   * chosen.
   */
  CoglHandle         texture;

} CoglTextureUnit;

CoglTextureUnit *
_cogl_get_texture_unit (int index_);

void
_cogl_destroy_texture_units (void);

typedef enum _CoglMaterialEqualFlags
{
  /* Return FALSE if any component of either material isn't set to its
   * default value. (Note: if the materials have corresponding flush
   * options indicating that e.g. the material color won't be flushed then
   * this will not assert a default color value.) */
  COGL_MATERIAL_EQUAL_FLAGS_ASSERT_ALL_DEFAULTS   = 1L<<0,

} CoglMaterialEqualFlags;

typedef enum _CoglMaterialLayerDifferenceFlags
{
  COGL_MATERIAL_LAYER_DIFFERENCE_TEXTURE          = 1L<<0,
  COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE          = 1L<<1,
  COGL_MATERIAL_LAYER_DIFFERENCE_COMBINE_CONSTANT = 1L<<2,
  COGL_MATERIAL_LAYER_DIFFERENCE_USER_MATRIX      = 1L<<3,
  COGL_MATERIAL_LAYER_DIFFERENCE_FILTERS          = 1L<<4
} CoglMaterialLayerDifferenceFlags;

typedef enum _CoglMaterialLayerChangeFlags
{
  COGL_MATERIAL_LAYER_CHANGE_TEXTURE          = 1L<<0,
  COGL_MATERIAL_LAYER_CHANGE_COMBINE          = 1L<<1,
  COGL_MATERIAL_LAYER_CHANGE_COMBINE_CONSTANT = 1L<<2,
  COGL_MATERIAL_LAYER_CHANGE_USER_MATRIX      = 1L<<3,
  COGL_MATERIAL_LAYER_CHANGE_FILTERS          = 1L<<4,

  COGL_MATERIAL_LAYER_CHANGE_TEXTURE_INTERN   = 1L<<5,
  COGL_MATERIAL_LAYER_CHANGE_UNIT             = 1L<<6
} CoglMaterialLayerChangeFlags;

struct _CoglMaterialLayer
{
  CoglHandleObject _parent;

  /* Parent material */
  CoglMaterial    *material;

  unsigned int	   index;   /*!< lowest index is blended first then others on
                              top */

  int              unit_index;

  unsigned long    differences;

  CoglHandle       texture; /*!< The texture for this layer, or
                              COGL_INVALID_HANDLE for an empty layer */

  CoglMaterialFilter mag_filter;
  CoglMaterialFilter min_filter;

  CoglMaterialWrapMode wrap_mode_s;
  CoglMaterialWrapMode wrap_mode_t;
  CoglMaterialWrapMode wrap_mode_r;

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

  /* Different material backends (GLSL/ARBfp/Fixed Function) may
   * want to associate private data with a layer... */
  void *backend_priv;
};

typedef enum _CoglMaterialFlags
{
  COGL_MATERIAL_FLAG_DEFAULT_COLOR          = 1L<<1,
  COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL    = 1L<<2,
  COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC     = 1L<<3,
  COGL_MATERIAL_FLAG_ENABLE_BLEND	    = 1L<<4,
  COGL_MATERIAL_FLAG_DEFAULT_BLEND          = 1L<<5,
  COGL_MATERIAL_FLAG_DEFAULT_USER_SHADER    = 1L<<6,
  COGL_MATERIAL_FLAG_DEFAULT_LAYERS         = 1L<<7
} CoglMaterialFlags;

/* This defines the initialization state for
 * ctx->current_material_flags which should result in the first
 * material flush explicitly initializing everything
 */
#define COGL_MATERIAL_FLAGS_INIT \
  COGL_MATERIAL_FLAG_DEFAULT_USER_SHADER

typedef enum _CoglMaterialChangeFlag
{
  COGL_MATERIAL_CHANGE_COLOR          = 1L<<1,
  COGL_MATERIAL_CHANGE_GL_MATERIAL    = 1L<<2,
  COGL_MATERIAL_CHANGE_ALPHA_FUNC     = 1L<<3,
  COGL_MATERIAL_CHANGE_ENABLE_BLEND   = 1L<<4,
  COGL_MATERIAL_CHANGE_BLEND          = 1L<<5,
  COGL_MATERIAL_CHANGE_USER_SHADER    = 1L<<6,
  COGL_MATERIAL_CHANGE_LAYERS         = 1L<<7
} CoglMaterialChangeFlag;

struct _CoglMaterial
{
  CoglHandleObject _parent;
  unsigned long    journal_ref_count;

  int              backend;

  unsigned long    flags;

  /* If no lighting is enabled; this is the basic material color */
  GLubyte   unlit[4];

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

  CoglHandle user_program;

  GList	       *layers;
  unsigned int  n_layers;

  void *backend_priv;
};

typedef struct _CoglMaterialBackend
{
  int (*get_max_texture_units) (void);

  gboolean (*start) (CoglMaterial *material);
  gboolean (*add_layer) (CoglMaterialLayer *layer);
  gboolean (*passthrough) (CoglMaterial *material);
  gboolean (*end) (CoglMaterial *material);

  void (*material_change_notify) (CoglMaterial *material,
                                  unsigned long changes,
                                  GLubyte *new_color);
  void (*layer_change_notify) (CoglMaterialLayer *layer,
                               unsigned long changes);

  void (*free_priv) (CoglMaterial *material);
} CoglMaterialBackend;

typedef enum
{
  COGL_MATERIAL_PROGRAM_TYPE_GLSL = 1,
  COGL_MATERIAL_PROGRAM_TYPE_ARBFP,
  COGL_MATERIAL_PROGRAM_TYPE_FIXED
} CoglMaterialProgramType;

/*
 * SECTION:cogl-material-internals
 * @short_description: Functions for creating custom primitives that make use
 *    of Cogl materials for filling.
 *
 * Normally you shouldn't need to use this API directly, but if you need to
 * developing a custom/specialised primitive - probably using raw OpenGL - then
 * this API aims to expose enough of the material internals to support being
 * able to fill your geometry according to a given Cogl material.
 */

/*
 * _cogl_material_init_default_material:
 *
 * This initializes the first material owned by the Cogl context. All
 * subsequently instantiated materials created via the cogl_material_new()
 * API will initially be a copy of this material.
 */
void
_cogl_material_init_default_material (void);

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
unsigned long
_cogl_material_get_cogl_enable_flags (CoglHandle handle);

gboolean
_cogl_material_layer_has_user_matrix (CoglHandle layer_handle);

/*
 * Ensures the mipmaps are available for the texture in the layer if
 * the filter settings would require it
 */
void
_cogl_material_layer_ensure_mipmaps (CoglHandle layer_handler);

/*
 * CoglMaterialFlushFlag:
 * @COGL_MATERIAL_FLUSH_FALLBACK_MASK: The fallback_layers member is set to
 *      a guint32 mask of the layers that can't be supported with the user
 *      supplied texture and need to be replaced with fallback textures. (1 =
 *      fallback, and the least significant bit = layer 0)
 * @COGL_MATERIAL_FLUSH_DISABLE_MASK: The disable_layers member is set to
 *      a guint32 mask of the layers that you want to completly disable
 *      texturing for (1 = fallback, and the least significant bit = layer 0)
 * @COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE: The layer0_override_texture member is
 *      set to a GLuint OpenGL texture name to override the texture used for
 *      layer 0 of the material. This is intended for dealing with sliced
 *      textures where you will need to point to each of the texture slices in
 *      turn when drawing your geometry.  Passing a value of 0 is the same as
 *      not passing the option at all.
 * @COGL_MATERIAL_FLUSH_SKIP_GL_COLOR: When flushing the GL state for the
 *      material don't call glColor.
 * @COGL_MATERIAL_FLUSH_WRAP_MODE_OVERRIDES: Specifies that a bitmask
 *      of overrides for the wrap modes for some or all layers is
 *      given.
 */
typedef enum _CoglMaterialFlushFlag
{
  COGL_MATERIAL_FLUSH_FALLBACK_MASK       = 1L<<0,
  COGL_MATERIAL_FLUSH_DISABLE_MASK        = 1L<<1,
  COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE     = 1L<<2,
  COGL_MATERIAL_FLUSH_SKIP_GL_COLOR       = 1L<<3,
  COGL_MATERIAL_FLUSH_WRAP_MODE_OVERRIDES = 1L<<4
} CoglMaterialFlushFlag;

/* These constants are used to fill in wrap_mode_overrides */
#define COGL_MATERIAL_WRAP_MODE_OVERRIDE_NONE            0 /* no override */
#define COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT          1
#define COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE   2
#define COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_BORDER 3

/* There can't be more than 32 layers because we need to fit a bitmask
   of the layers into a guint32 */
#define COGL_MATERIAL_MAX_LAYERS 32

typedef struct _CoglMaterialWrapModeOverrides
{
  struct
  {
    unsigned long s : 2;
    unsigned long t : 2;
    unsigned long r : 2;
  } values[COGL_MATERIAL_MAX_LAYERS];
} CoglMaterialWrapModeOverrides;

/*
 * CoglMaterialFlushOptions:
 *
 */
typedef struct _CoglMaterialFlushOptions
{
  CoglMaterialFlushFlag         flags;

  guint32                       fallback_layers;
  guint32                       disable_layers;
  GLuint                        layer0_override_texture;
  CoglMaterialWrapModeOverrides wrap_mode_overrides;
} CoglMaterialFlushOptions;

void
_cogl_material_get_colorubv (CoglHandle  handle,
                             guint8     *color);

void
_cogl_material_flush_gl_state (CoglHandle material,
                               CoglMaterialFlushOptions *options);

gboolean
_cogl_material_equal (CoglHandle material0_handle,
                      CoglMaterialFlushOptions *material0_flush_options,
                      CoglHandle material1_handle,
                      CoglMaterialFlushOptions *material1_flush_options);

CoglHandle
_cogl_material_journal_ref (CoglHandle material_handle);

void
_cogl_material_journal_unref (CoglHandle material_handle);

/* TODO: These should be made public once we add support for 3D
   textures in Cogl */
void
_cogl_material_set_layer_wrap_mode_r (CoglHandle material,
                                      int layer_index,
                                      CoglMaterialWrapMode mode);

CoglMaterialWrapMode
_cogl_material_layer_get_wrap_mode_r (CoglHandle layer);

void
_cogl_material_set_user_program (CoglHandle handle,
                                 CoglHandle program);

void
_cogl_material_apply_legacy_state (CoglHandle handle);

void
_cogl_gl_use_program_wrapper (GLuint program);

#endif /* __COGL_MATERIAL_PRIVATE_H */

