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

#ifndef __COGL_MATERIAL_PRIVATE_H
#define __COGL_MATERIAL_PRIVATE_H

#include "cogl.h"

#include "cogl-material.h"
#include "cogl-matrix.h"
#include "cogl-handle.h"
#include "cogl-profile.h"

#include <glib.h>

#if defined (HAVE_COGL_GL)

/* NB: material->backend is currently a 3bit unsigned int bitfield */
#define COGL_MATERIAL_BACKEND_GLSL       0
#define COGL_MATERIAL_BACKEND_GLSL_MASK  (1L<<0)
#define COGL_MATERIAL_BACKEND_ARBFP      1
#define COGL_MATERIAL_BACKEND_ARBFP_MASK (1L<<1)
#define COGL_MATERIAL_BACKEND_FIXED      2
#define COGL_MATERIAL_BACKEND_FIXED_MASK (1L<<2)

#define COGL_MATERIAL_N_BACKENDS         3

#elif defined (HAVE_COGL_GLES2)

#define COGL_MATERIAL_BACKEND_GLSL       0
#define COGL_MATERIAL_BACKEND_GLSL_MASK  (1L<<0)
#define COGL_MATERIAL_BACKEND_FIXED      1
#define COGL_MATERIAL_BACKEND_FIXED_MASK (1L<<1)

#define COGL_MATERIAL_N_BACKENDS         2

#else /* HAVE_COGL_GLES */

#define COGL_MATERIAL_BACKEND_FIXED      0
#define COGL_MATERIAL_BACKEND_FIXED_MASK (1L<<0)

#define COGL_MATERIAL_N_BACKENDS         1

#endif

#define COGL_MATERIAL_BACKEND_DEFAULT    0
#define COGL_MATERIAL_BACKEND_UNDEFINED  3

typedef enum
{
  COGL_MATERIAL_LAYER_STATE_UNIT             = 1L<<0,
  COGL_MATERIAL_LAYER_STATE_TEXTURE          = 1L<<1,
  COGL_MATERIAL_LAYER_STATE_FILTERS          = 1L<<2,
  COGL_MATERIAL_LAYER_STATE_WRAP_MODES       = 1L<<3,

  COGL_MATERIAL_LAYER_STATE_COMBINE          = 1L<<4,
  COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT = 1L<<5,
  COGL_MATERIAL_LAYER_STATE_USER_MATRIX      = 1L<<6,

  COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS = 1L<<7,

  /* COGL_MATERIAL_LAYER_STATE_TEXTURE_INTERN   = 1L<<8, */

  COGL_MATERIAL_LAYER_STATE_ALL_SPARSE =
    COGL_MATERIAL_LAYER_STATE_UNIT |
    COGL_MATERIAL_LAYER_STATE_TEXTURE |
    COGL_MATERIAL_LAYER_STATE_FILTERS |
    COGL_MATERIAL_LAYER_STATE_WRAP_MODES |
    COGL_MATERIAL_LAYER_STATE_COMBINE |
    COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT |
    COGL_MATERIAL_LAYER_STATE_USER_MATRIX |
    COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS,

  COGL_MATERIAL_LAYER_STATE_NEEDS_BIG_STATE =
    COGL_MATERIAL_LAYER_STATE_COMBINE |
    COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT |
    COGL_MATERIAL_LAYER_STATE_USER_MATRIX |
    COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS,

} CoglMaterialLayerState;

typedef struct
{
  /* The texture combine state determines how the color of individual
   * texture fragments are calculated. */
  GLint texture_combine_rgb_func;
  GLint texture_combine_rgb_src[3];
  GLint texture_combine_rgb_op[3];

  GLint texture_combine_alpha_func;
  GLint texture_combine_alpha_src[3];
  GLint texture_combine_alpha_op[3];

  float texture_combine_constant[4];

  /* The texture matrix dscribes how to transform texture coordinates */
  CoglMatrix matrix;

  gboolean point_sprite_coords;

} CoglMaterialLayerBigState;

/* Materials and layers represent their state in a tree structure where
 * some of the state relating to a given material or layer may actually
 * be owned by one if is ancestors in the tree. We have a common data
 * type to track the tree heirachy so we can share code... */
typedef struct _CoglMaterialNode CoglMaterialNode;
struct _CoglMaterialNode
{
  /* the parent in terms of class hierarchy, so anything inheriting
   * from CoglMaterialNode also inherits from CoglObject. */
  CoglObject _parent;

  /* The parent material/layer */
  CoglMaterialNode *parent;

  /* TRUE if the node took a strong reference on its parent. Weak
   * materials for instance don't take a reference on their parent. */
  gboolean has_parent_reference;

  /* As an optimization for creating leaf node materials/layers (the
   * most common) we don't require any list node allocations to link
   * to a single descendant. */
  CoglMaterialNode *first_child;

  /* Determines if node->first_child and node->children are
   * initialized pointers. */
  gboolean has_children;

  /* Materials and layers are sparse structures defined as a diff
   * against their parent and may have multiple children which depend
   * on them to define the values of properties which they don't
   * change. */
  GList *children;
};

#define COGL_MATERIAL_NODE(X) ((CoglMaterialNode *)(X))

typedef void (*CoglMaterialNodeUnparentVFunc) (CoglMaterialNode *node);

typedef gboolean (*CoglMaterialNodeChildCallback) (CoglMaterialNode *child,
                                                   void *user_data);

void
_cogl_material_node_foreach_child (CoglMaterialNode *node,
                                   CoglMaterialNodeChildCallback callback,
                                   void *user_data);

struct _CoglMaterialLayer
{
  /* XXX: Please think twice about adding members that *have* be
   * initialized during a _cogl_material_layer_copy. We are aiming
   * to have copies be as cheap as possible and copies may be
   * done by the primitives APIs which means they may happen
   * in performance critical code paths.
   *
   * XXX: If you are extending the state we track please consider if
   * the state is expected to vary frequently across many materials or
   * if the state can be shared among many derived materials instead.
   * This will determine if the state should be added directly to this
   * structure which will increase the memory overhead for *all*
   * layers or if instead it can go under ->big_state.
   */

  /* Layers represent their state in a tree structure where some of
   * the state relating to a given material or layer may actually be
   * owned by one if is ancestors in the tree. We have a common data
   * type to track the tree heirachy so we can share code... */
  CoglMaterialNode _parent;

  /* Some layers have a material owner, which is to say that the layer
   * is referenced in that materials->layer_differences list.  A layer
   * doesn't always have an owner and may simply be an ancestor for
   * other layers that keeps track of some shared state. */
  CoglMaterial      *owner;

  /* The lowest index is blended first then others on top */
  int	             index;

  /* Different material backends (GLSL/ARBfp/Fixed Function) may
   * want to associate private data with a layer...
   *
   * NB: we have per backend pointers because a layer may be
   * associated with multiple materials with different backends.
   */
  void              *backend_priv[COGL_MATERIAL_N_BACKENDS];

  /* A mask of which state groups are different in this layer
   * in comparison to its parent. */
  unsigned long             differences;

  /* Common differences
   *
   * As a basic way to reduce memory usage we divide the layer
   * state into two groups; the minimal state modified in 90% of
   * all layers and the rest, so that the second group can
   * be allocated dynamically when required.
   */

  /* Each layer is directly associated with a single texture unit */
  int                        unit_index;

  /* The texture for this layer, or COGL_INVALID_HANDLE for an empty
   * layer */
  CoglHandle                 texture;
  gboolean                   texture_overridden;
  /* If ->texture_overridden == TRUE then the texture is instead
   * defined by these... */
  GLuint                     slice_gl_texture;
  GLenum                     slice_gl_target;

  CoglMaterialFilter         mag_filter;
  CoglMaterialFilter         min_filter;

  CoglMaterialWrapMode       wrap_mode_s;
  CoglMaterialWrapMode       wrap_mode_t;
  CoglMaterialWrapMode       wrap_mode_p;

  /* Infrequent differences aren't currently tracked in
   * a separate, dynamically allocated structure as they are
   * for materials... */
  CoglMaterialLayerBigState *big_state;

  /* bitfields */

  /* Determines if layer->big_state is valid */
  unsigned int          has_big_state:1;

};

/* Used in material->differences masks and for notifying material
 * state changes... */
typedef enum _CoglMaterialState
{
  COGL_MATERIAL_STATE_COLOR             = 1L<<0,
  COGL_MATERIAL_STATE_BLEND_ENABLE      = 1L<<1,
  COGL_MATERIAL_STATE_LAYERS            = 1L<<2,

  COGL_MATERIAL_STATE_LIGHTING          = 1L<<3,
  COGL_MATERIAL_STATE_ALPHA_FUNC        = 1L<<4,
  COGL_MATERIAL_STATE_BLEND             = 1L<<5,
  COGL_MATERIAL_STATE_USER_SHADER       = 1L<<6,
  COGL_MATERIAL_STATE_DEPTH             = 1L<<7,
  COGL_MATERIAL_STATE_FOG               = 1L<<8,
  COGL_MATERIAL_STATE_POINT_SIZE        = 1L<<9,

  COGL_MATERIAL_STATE_REAL_BLEND_ENABLE = 1L<<10,

  COGL_MATERIAL_STATE_ALL_SPARSE =
    COGL_MATERIAL_STATE_COLOR |
    COGL_MATERIAL_STATE_BLEND_ENABLE |
    COGL_MATERIAL_STATE_LAYERS |
    COGL_MATERIAL_STATE_LIGHTING |
    COGL_MATERIAL_STATE_ALPHA_FUNC |
    COGL_MATERIAL_STATE_BLEND |
    COGL_MATERIAL_STATE_USER_SHADER |
    COGL_MATERIAL_STATE_DEPTH |
    COGL_MATERIAL_STATE_FOG |
    COGL_MATERIAL_STATE_POINT_SIZE,

  COGL_MATERIAL_STATE_AFFECTS_BLENDING =
    COGL_MATERIAL_STATE_COLOR |
    COGL_MATERIAL_STATE_BLEND_ENABLE |
    COGL_MATERIAL_STATE_LAYERS |
    COGL_MATERIAL_STATE_LIGHTING |
    COGL_MATERIAL_STATE_BLEND |
    COGL_MATERIAL_STATE_USER_SHADER,

  COGL_MATERIAL_STATE_NEEDS_BIG_STATE =
    COGL_MATERIAL_STATE_LIGHTING |
    COGL_MATERIAL_STATE_ALPHA_FUNC |
    COGL_MATERIAL_STATE_BLEND |
    COGL_MATERIAL_STATE_USER_SHADER |
    COGL_MATERIAL_STATE_DEPTH |
    COGL_MATERIAL_STATE_FOG |
    COGL_MATERIAL_STATE_POINT_SIZE

} CoglMaterialState;

typedef enum
{
  COGL_MATERIAL_LIGHTING_STATE_PROPERTY_AMBIENT = 1,
  COGL_MATERIAL_LIGHTING_STATE_PROPERTY_DIFFUSE,
  COGL_MATERIAL_LIGHTING_STATE_PROPERTY_SPECULAR,
  COGL_MATERIAL_LIGHTING_STATE_PROPERTY_EMISSION,
  COGL_MATERIAL_LIGHTING_STATE_PROPERTY_SHININESS
} CoglMaterialLightingStateProperty;

typedef struct
{
  /* Standard OpenGL lighting model attributes */
  float ambient[4];
  float diffuse[4];
  float specular[4];
  float emission[4];
  float shininess;
} CoglMaterialLightingState;

typedef struct
{
  /* Determines what fragments are discarded based on their alpha */
  CoglMaterialAlphaFunc alpha_func;
  GLfloat		alpha_func_reference;
} CoglMaterialAlphaFuncState;

typedef enum _CoglMaterialBlendEnable
{
  /* XXX: we want to detect users mistakenly using TRUE or FALSE
   * so start the enum at 2. */
  COGL_MATERIAL_BLEND_ENABLE_ENABLED = 2,
  COGL_MATERIAL_BLEND_ENABLE_DISABLED,
  COGL_MATERIAL_BLEND_ENABLE_AUTOMATIC
} CoglMaterialBlendEnable;

typedef struct
{
  /* Determines how this material is blended with other primitives */
#ifndef HAVE_COGL_GLES
  GLenum    blend_equation_rgb;
  GLenum    blend_equation_alpha;
  GLint     blend_src_factor_alpha;
  GLint     blend_dst_factor_alpha;
  CoglColor blend_constant;
#endif
  GLint     blend_src_factor_rgb;
  GLint     blend_dst_factor_rgb;
} CoglMaterialBlendState;

typedef struct
{
  gboolean              depth_test_enabled;
  CoglDepthTestFunction depth_test_function;
  gboolean              depth_writing_enabled;
  float                 depth_range_near;
  float                 depth_range_far;
} CoglMaterialDepthState;

typedef struct
{
  gboolean        enabled;
  CoglColor       color;
  CoglFogMode     mode;
  float           density;
  float           z_near;
  float           z_far;
} CoglMaterialFogState;

typedef struct
{
  CoglMaterialLightingState lighting_state;
  CoglMaterialAlphaFuncState alpha_state;
  CoglMaterialBlendState blend_state;
  CoglHandle user_program;
  CoglMaterialDepthState depth_state;
  CoglMaterialFogState fog_state;
  float point_size;
} CoglMaterialBigState;

typedef enum
{
  COGL_MATERIAL_FLAG_DIRTY_LAYERS_CACHE     = 1L<<0,
  COGL_MATERIAL_FLAG_DIRTY_GET_LAYERS_LIST  = 1L<<1
} CoglMaterialFlag;

typedef struct
{
  CoglMaterial *owner;
  CoglMaterialLayer *layer;
} CoglMaterialLayerCacheEntry;

/*
 * CoglMaterialDestroyCallback
 * @material: The #CoglMaterial that has been destroyed
 * @user_data: The private data associated with the callback
 *
 * Notifies when a weak material has been destroyed because one
 * of its ancestors has been freed or modified.
 */
typedef void (*CoglMaterialDestroyCallback)(CoglMaterial *material,
                                            void *user_data);

struct _CoglMaterial
{
  /* XXX: Please think twice about adding members that *have* be
   * initialized during a cogl_material_copy. We are aiming to have
   * copies be as cheap as possible and copies may be done by the
   * primitives APIs which means they may happen in performance
   * critical code paths.
   *
   * XXX: If you are extending the state we track please consider if
   * the state is expected to vary frequently across many materials or
   * if the state can be shared among many derived materials instead.
   * This will determine if the state should be added directly to this
   * structure which will increase the memory overhead for *all*
   * materials or if instead it can go under ->big_state.
   */

  /* Layers represent their state in a tree structure where some of
   * the state relating to a given material or layer may actually be
   * owned by one if is ancestors in the tree. We have a common data
   * type to track the tree heirachy so we can share code... */
  CoglMaterialNode _parent;

  /* We need to track if a material is referenced in the journal
   * because we can't allow modification to these materials without
   * flushing the journal first */
  unsigned long    journal_ref_count;

  /* When weak materials are destroyed the user is notified via this
   * callback */
  CoglMaterialDestroyCallback destroy_callback;

  /* When notifying that a weak material has been destroyed this
   * private data is passed to the above callback */
  void *destroy_data;

  /* A mask of which sparse state groups are different in this
   * material in comparison to its parent. */
  unsigned long    differences;

  /* The fragment processing backends can associate private data with a
   * material. */
  void		  *backend_privs[COGL_MATERIAL_N_BACKENDS];

  /* Whenever a material is modified we increment the age. There's no
   * guarantee that it won't wrap but it can nevertheless be a
   * convenient mechanism to determine when a material has been
   * changed to you can invalidate some some associated cache that
   * depends on the old state. */
  unsigned long    age;

  /* This is the primary color of the material.
   *
   * This is a sparse property, ref COGL_MATERIAL_STATE_COLOR */
  CoglColor        color;

  /* A material may be made up with multiple layers used to combine
   * textures together.
   *
   * This is sparse state, ref COGL_MATERIAL_STATE_LAYERS */
  GList	          *layer_differences;
  unsigned int     n_layers;

  /* As a basic way to reduce memory usage we divide the material
   * state into two groups; the minimal state modified in 90% of
   * all materials and the rest, so that the second group can
   * be allocated dynamically when required... */
  CoglMaterialBigState *big_state;

  /* For debugging purposes it's possible to associate a static const
   * string with a material which can be an aid when trying to trace
   * where the material originates from */
  const char      *static_breadcrumb;

  /* Cached state... */

  /* A cached, complete list of the layers this material depends
   * on sorted by layer->unit_index. */
  CoglMaterialLayer   **layers_cache;
  /* To avoid a separate ->layers_cache allocation for common
   * materials with only a few layers... */
  CoglMaterialLayer    *short_layers_cache[3];

  /* The deprecated cogl_material_get_layers() API returns a
   * const GList of layers, which we track here... */
  GList                *deprecated_get_layers_list;

  /* XXX: consider adding an authorities cache to speed up sparse
   * property value lookups:
   * CoglMaterial *authorities_cache[COGL_MATERIAL_N_SPARSE_PROPERTIES];
   * and corresponding authorities_cache_dirty:1 bitfield
   */

  /* bitfields */

  /* A material can have private data associated with it for multiple
   * fragment processing backends. Although only one backend is
   * associated with a material the backends may want to cache private
   * state with the ancestors of other materials and those ancestors
   * could currently be associated with different backends.
   *
   * Each set bit indicates if the correspondong ->backend_privs[]
   * entry is valid.
   */
  unsigned int          backend_priv_set_mask:COGL_MATERIAL_N_BACKENDS;

  /* Weak materials don't count as dependants on their parents which
   * means that the parent material can be modified without
   * considering how the modifications may affect the weak material.
   */
  unsigned int          is_weak:1;

  /* Determines if material->big_state is valid */
  unsigned int          has_big_state:1;

  /* By default blending is enabled automatically depending on the
   * unlit color, the lighting colors or the texture format. The user
   * can override this to explicitly enable or disable blending.
   *
   * This is a sparse property */
  unsigned int          blend_enable:3;

  /* There are many factors that can determine if we need to enable
   * blending, this holds our final decision */
  unsigned int          real_blend_enable:1;

  unsigned int          layers_cache_dirty:1;
  unsigned int          deprecated_get_layers_list_dirty:1;

  /* For debugging purposes it's possible to associate a static const
   * string with a material which can be an aid when trying to trace
   * where the material originates from */
  unsigned int          has_static_breadcrumb:1;

  /* There are multiple fragment processing backends for CoglMaterial,
   * glsl, arbfp and fixed. This identifies the backend being used for
   * the material and any private state the backend has associated
   * with the material. */
  unsigned int          backend:3;
};

typedef struct _CoglMaterialBackend
{
  int (*get_max_texture_units) (void);

  gboolean (*start) (CoglMaterial *material,
                     int n_layers,
                     unsigned long materials_difference);
  gboolean (*add_layer) (CoglMaterial *material,
                         CoglMaterialLayer *layer,
                         unsigned long layers_difference);
  gboolean (*passthrough) (CoglMaterial *material);
  gboolean (*end) (CoglMaterial *material,
                   unsigned long materials_difference);

  void (*material_pre_change_notify) (CoglMaterial *material,
                                      CoglMaterialState change,
                                      const CoglColor *new_color);
  void (*material_set_parent_notify) (CoglMaterial *material);
  void (*layer_pre_change_notify) (CoglMaterial *owner,
                                   CoglMaterialLayer *layer,
                                   CoglMaterialLayerState change);

  void (*free_priv) (CoglMaterial *material);
  void (*free_layer_priv) (CoglMaterialLayer *layer);
} CoglMaterialBackend;

typedef enum
{
  COGL_MATERIAL_PROGRAM_TYPE_GLSL = 1,
  COGL_MATERIAL_PROGRAM_TYPE_ARBFP,
  COGL_MATERIAL_PROGRAM_TYPE_FIXED
} CoglMaterialProgramType;

extern const CoglMaterialBackend *
_cogl_material_backends[COGL_MATERIAL_N_BACKENDS];

void
_cogl_material_init_default_material (void);

void
_cogl_material_init_default_layers (void);

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

gboolean
_cogl_material_get_real_blend_enabled (CoglMaterial *material);

gboolean
_cogl_material_layer_has_user_matrix (CoglMaterialLayer *layer);

/*
 * Calls the pre_paint method on the layer texture if there is
 * one. This will determine whether mipmaps are needed based on the
 * filter settings.
 */
void
_cogl_material_layer_pre_paint (CoglMaterialLayer *layerr);

/*
 * Calls the pre_paint method on the layer texture if there is
 * one. This will determine whether mipmaps are needed based on the
 * filter settings.
 */
void
_cogl_material_pre_paint_for_layer (CoglMaterial *material,
                                    int layer_id);

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

/* This isn't defined in the GLES headers */
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif

/* GL_ALWAYS is just used here as a value that is known not to clash
 * with any valid GL wrap modes.
 *
 * XXX: keep the values in sync with the CoglMaterialWrapMode enum
 * so no conversion is actually needed.
 */
typedef enum _CoglMaterialWrapModeInternal
{
  COGL_MATERIAL_WRAP_MODE_INTERNAL_REPEAT = GL_REPEAT,
  COGL_MATERIAL_WRAP_MODE_INTERNAL_CLAMP_TO_EDGE = GL_CLAMP_TO_EDGE,
  COGL_MATERIAL_WRAP_MODE_INTERNAL_CLAMP_TO_BORDER = GL_CLAMP_TO_BORDER,
  COGL_MATERIAL_WRAP_MODE_INTERNAL_AUTOMATIC = GL_ALWAYS
} CoglMaterialWrapModeInternal;

typedef enum _CoglMaterialWrapModeOverride
{
  COGL_MATERIAL_WRAP_MODE_OVERRIDE_NONE = 0,
  COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT =
    COGL_MATERIAL_WRAP_MODE_INTERNAL_REPEAT,
  COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE =
    COGL_MATERIAL_WRAP_MODE_INTERNAL_CLAMP_TO_EDGE,
  COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_BORDER =
    COGL_MATERIAL_WRAP_MODE_INTERNAL_CLAMP_TO_BORDER,
} CoglMaterialWrapModeOverride;

/* There can't be more than 32 layers because we need to fit a bitmask
   of the layers into a guint32 */
#define COGL_MATERIAL_MAX_LAYERS 32

typedef struct _CoglMaterialWrapModeOverrides
{
  struct
  {
    CoglMaterialWrapModeOverride s;
    CoglMaterialWrapModeOverride t;
    CoglMaterialWrapModeOverride p;
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


int
_cogl_get_max_texture_image_units (void);


void
_cogl_use_program (GLuint gl_program, CoglMaterialProgramType type);

unsigned int
_cogl_get_n_args_for_combine_func (GLint func);

/*
 * _cogl_material_weak_copy:
 * @material: A #CoglMaterial object
 * @callback: A callback to notify when your weak material is destroyed
 * @user_data: Private data to pass to your given callback.
 *
 * Returns a weak copy of the given source @material. Unlike a normal
 * copy no internal reference is taken on the source @material and you
 * can expect that later modifications of the source material (or in
 * fact any other material) can result in the weak material being
 * destroyed.
 *
 * To understand this better its good to know a bit about the internal
 * design of #CoglMaterial...
 *
 * Internally #CoglMaterial<!-- -->s are represented as a graph of
 * property diff's, where each node is a diff of properties that gets
 * applied on top of its parent. Copying a material creates an empty
 * diff and a child->parent relationship between the empty diff and
 * the source @material, parent.
 *
 * Because of this internal graph design a single #CoglMaterial may
 * indirectly depend on a chain of ancestors to fully define all of
 * its properties. Because a node depends on its ancestors it normally
 * owns a reference to its parent to stop it from being freed. Also if
 * you try to modify a material with children we internally use a
 * copy-on-write mechanism to ensure that you don't indirectly change
 * the properties those children.
 *
 * Weak materials avoid the use of copy-on-write to preserve the
 * integrity of weak dependants and instead weak dependants are
 * simply destroyed allowing the parent to be modified directly. Also
 * because weak materials don't own a reference to their parent they
 * won't stop the source @material from being freed when the user
 * releases their reference on it.
 *
 * Because weak materials don't own a reference on their parent they
 * are the recommended mechanism for creating derived materials that you
 * want to cache as a private property of the original material
 * because they won't result in a circular dependency.
 *
 * An example use case:
 *
 * Consider for example you are implementing a custom primitive that is
 * not compatible with certain source materials. To handle this you
 * implement a validation stage that given an arbitrary material as
 * input will create a derived material that is suitable for drawing
 * your primitive.
 *
 * Because you don't want to have to repeat this validation every time
 * the same incompatible material is given as input you want to cache
 * the result as a private property of the original material. If the
 * derived material were created using cogl_material_copy that would
 * create a circular dependency so the original material can never be
 * freed.
 *
 * If you instead create a weak copy you won't stop the original material
 * from being freed if it's no longer needed, and you will instead simply
 * be notified that your weak material has been destroyed.
 *
 * This is the recommended coding pattern for validating an input
 * material and caching a derived result:
 * |[
 * static CoglUserDataKey _cogl_my_cache_key;
 *
 * typedef struct {
 *   CoglMaterial *validated_source;
 * } MyValidatedMaterialCache;
 *
 * static void
 * destroy_cache_cb (CoglObject *object, void *user_data)
 * {
 *   g_slice_free (MyValidatedMaterialCache, user_data);
 * }
 *
 * static void
 * invalidate_cache_cb (CoglMaterial *destroyed, void *user_data)
 * {
 *   MyValidatedMaterialCache *cache = user_data;
 *   cogl_object_unref (cache->validated_source);
 *   cache->validated_source = NULL;
 * }
 *
 * static CoglMaterial *
 * get_validated_material (CoglMaterial *source)
 * {
 *   MyValidatedMaterialCache *cache =
 *     cogl_object_get_user_data (COGL_OBJECT (source),
 *                                &_cogl_my_cache_key);
 *   if (G_UNLIKELY (cache == NULL))
 *     {
 *       cache = g_slice_new (MyValidatedMaterialCache);
 *       cogl_object_set_user_data (COGL_OBJECT (source),
 *                                  &_cogl_my_cache_key,
 *                                  cache, destroy_cache_cb);
 *       cache->validated_source = source;
 *     }
 *
 *   if (G_UNLIKELY (cache->validated_source == NULL))
 *     {
 *       cache->validated_source = source;
 *
 *       /&nbsp;* Start validating source... *&nbsp;/
 *
 *       /&nbsp;* If you find you need to change something... *&nbsp;/
 *       if (cache->validated_source == source)
 *         cache->validated_source =
 *           cogl_material_weak_copy (source,
 *                                    invalidate_cache_cb,
 *                                    cache);
 *
 *       /&nbsp;* Modify cache->validated_source *&nbsp;/
 *     }
 *
 *    return cache->validated_source;
 * }
 * ]|
 */
CoglMaterial *
_cogl_material_weak_copy (CoglMaterial *material,
                          CoglMaterialDestroyCallback callback,
                          void *user_data);

void
_cogl_material_set_backend (CoglMaterial *material, int backend);

CoglMaterial *
_cogl_material_get_parent (CoglMaterial *material);

void
_cogl_material_get_colorubv (CoglMaterial *material,
                             guint8       *color);

unsigned long
_cogl_material_compare_differences (CoglMaterial *material0,
                                    CoglMaterial *material1);

gboolean
_cogl_material_equal (CoglMaterial *material0,
                      CoglMaterial *material1,
                      gboolean skip_gl_color);

CoglMaterial *
_cogl_material_journal_ref (CoglMaterial *material);

void
_cogl_material_journal_unref (CoglMaterial *material);

void
_cogl_material_layer_get_wrap_modes (CoglMaterialLayer *layer,
                                     CoglMaterialWrapModeInternal *wrap_mode_s,
                                     CoglMaterialWrapModeInternal *wrap_mode_t,
                                     CoglMaterialWrapModeInternal *wrap_mode_r);

void
_cogl_material_layer_get_filters (CoglMaterialLayer *layer,
                                  CoglMaterialFilter *min_filter,
                                  CoglMaterialFilter *mag_filter);

void
_cogl_material_get_layer_filters (CoglMaterial *material,
                                  int layer_index,
                                  CoglMaterialFilter *min_filter,
                                  CoglMaterialFilter *mag_filter);

CoglMaterialFilter
_cogl_material_get_layer_min_filter (CoglMaterial *material,
                                     int layer_index);

CoglMaterialFilter
_cogl_material_get_layer_mag_filter (CoglMaterial *material,
                                     int layer_index);

void
_cogl_material_texture_storage_change_notify (CoglHandle texture);

void
_cogl_material_apply_legacy_state (CoglMaterial *material);

void
_cogl_material_apply_overrides (CoglMaterial *material,
                                CoglMaterialFlushOptions *options);

CoglMaterialBlendEnable
_cogl_material_get_blend_enabled (CoglMaterial *material);

void
_cogl_material_set_blend_enabled (CoglMaterial *material,
                                  CoglMaterialBlendEnable enable);

void
_cogl_material_set_static_breadcrumb (CoglMaterial *material,
                                      const char *breadcrumb);

unsigned long
_cogl_material_get_age (CoglMaterial *material);

CoglMaterial *
_cogl_material_get_authority (CoglMaterial *material,
                              unsigned long difference);

unsigned long
_cogl_material_layer_compare_differences (CoglMaterialLayer *layer0,
                                          CoglMaterialLayer *layer1);

CoglMaterialLayer *
_cogl_material_layer_get_authority (CoglMaterialLayer *layer,
                                    unsigned long difference);

CoglHandle
_cogl_material_layer_get_texture (CoglMaterialLayer *layer);

CoglHandle
_cogl_material_get_layer_texture (CoglMaterial *material,
                                  int layer_index);

typedef gboolean (*CoglMaterialInternalLayerCallback) (CoglMaterialLayer *layer,
                                                       void *user_data);

void
_cogl_material_foreach_layer_internal (CoglMaterial *material,
                                       CoglMaterialInternalLayerCallback callback,
                                       void *user_data);

int
_cogl_material_layer_get_unit_index (CoglMaterialLayer *layer);

void
_cogl_material_get_layer_combine_constant (CoglMaterial *material,
                                           int layer_index,
                                           float *constant);

void
_cogl_material_prune_to_n_layers (CoglMaterial *material, int n);

#endif /* __COGL_MATERIAL_PRIVATE_H */

