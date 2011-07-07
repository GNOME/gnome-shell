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

#ifndef __COGL_PIPELINE_PRIVATE_H
#define __COGL_PIPELINE_PRIVATE_H

#include "cogl.h"

#include "cogl-pipeline.h"
#include "cogl-matrix.h"
#include "cogl-object-private.h"
#include "cogl-profile.h"
#include "cogl-queue.h"

#include <glib.h>

typedef struct _CoglPipelineLayer     CoglPipelineLayer;
#define COGL_PIPELINE_LAYER(OBJECT) ((CoglPipelineLayer *)OBJECT)

#ifdef HAVE_COGL_GL

#define COGL_PIPELINE_FRAGEND_ARBFP 0
#define COGL_PIPELINE_FRAGEND_FIXED 1
#define COGL_PIPELINE_FRAGEND_GLSL  2
#define COGL_PIPELINE_N_FRAGENDS    3

#else /* HAVE_COGL_GL */

#ifdef HAVE_COGL_GLES2

#define COGL_PIPELINE_FRAGEND_GLSL 0
#ifdef HAVE_COGL_GLES
#define COGL_PIPELINE_FRAGEND_FIXED 1
#define COGL_PIPELINE_N_FRAGENDS    2
#else
#define COGL_PIPELINE_N_FRAGENDS    1
#endif

#else /* HAVE_COGL_GLES2 */

#ifdef HAVE_COGL_GLES
#define COGL_PIPELINE_FRAGEND_FIXED 0
#define COGL_PIPELINE_N_FRAGENDS    1
#else
#error No drivers defined
#endif

#endif /* HAVE_COGL_GLES2 */

#endif /* HAVE_COGL_GL */

#ifdef COGL_PIPELINE_FRAGEND_ARBFP
#define COGL_PIPELINE_FRAGEND_ARBFP_MASK \
  (1 << COGL_PIPELINE_FRAGEND_ARBFP)
#endif
#ifdef COGL_PIPELINE_FRAGEND_FIXED
#define COGL_PIPELINE_FRAGEND_FIXED_MASK \
  (1 << COGL_PIPELINE_FRAGEND_FIXED)
#endif
#ifdef COGL_PIPELINE_FRAGEND_GLSL
#define COGL_PIPELINE_FRAGEND_GLSL_MASK \
  (1 << COGL_PIPELINE_FRAGEND_GLSL)
#endif

#define COGL_PIPELINE_FRAGEND_DEFAULT    0
#define COGL_PIPELINE_FRAGEND_UNDEFINED  3

#ifdef HAVE_COGL_GL

#define COGL_PIPELINE_VERTEND_FIXED 0
#define COGL_PIPELINE_VERTEND_GLSL  1
#define COGL_PIPELINE_N_VERTENDS    2

#else /* HAVE_COGL_GL */

#ifdef HAVE_COGL_GLES2

#define COGL_PIPELINE_VERTEND_GLSL  0
#ifdef HAVE_COGL_GLES
#define COGL_PIPELINE_VERTEND_FIXED 1
#define COGL_PIPELINE_N_VERTENDS    2
#else
#define COGL_PIPELINE_N_VERTENDS    1
#endif

#else /* HAVE_COGL_GLES2 */

#ifdef HAVE_COGL_GLES
#define COGL_PIPELINE_VERTEND_FIXED 0
#define COGL_PIPELINE_N_VERTENDS    1
#else
#error No drivers defined
#endif /* HAVE_COGL_GLES */

#endif /* HAVE_COGL_GLES2 */

#endif /* HAVE_COGL_GL */

#ifdef COGL_PIPELINE_VERTEND_FIXED
#define COGL_PIPELINE_VERTEND_FIXED_MASK \
  (1 << COGL_PIPELINE_VERTEND_FIXED)
#endif
#ifdef COGL_PIPELINE_VERTEND_GLSL
#define COGL_PIPELINE_VERTEND_GLSL_MASK \
  (1 << COGL_PIPELINE_VERTEND_GLSL)
#endif

#define COGL_PIPELINE_VERTEND_DEFAULT    0
#define COGL_PIPELINE_VERTEND_UNDEFINED  3

#define COGL_PIPELINE_VERTEND_DEFAULT    0
#define COGL_PIPELINE_VERTEND_UNDEFINED  3

/* If we have either of the GLSL backends then we also need a GLSL
   progend to combine the shaders generated into a single
   program. Currently there is only one progend but if we ever add
   other languages they would likely need their own progend too. The
   progends are different from the other backends because there can be
   more than one in use for each pipeline. All of the progends are
   invoked whenever a pipeline is flushed. */
#ifdef COGL_PIPELINE_FRAGEND_GLSL
#define COGL_PIPELINE_PROGEND_GLSL       0
#define COGL_PIPELINE_N_PROGENDS         1
#else
#define COGL_PIPELINE_N_PROGENDS         0
#endif

/* XXX: should I rename these as
 * COGL_PIPELINE_LAYER_STATE_INDEX_XYZ... ?
 */
typedef enum
{
  /* sparse state */
  COGL_PIPELINE_LAYER_STATE_UNIT_INDEX,
  COGL_PIPELINE_LAYER_STATE_TEXTURE_TARGET_INDEX,
  COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX,
  COGL_PIPELINE_LAYER_STATE_FILTERS_INDEX,
  COGL_PIPELINE_LAYER_STATE_WRAP_MODES_INDEX,
  COGL_PIPELINE_LAYER_STATE_COMBINE_INDEX,
  COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT_INDEX,
  COGL_PIPELINE_LAYER_STATE_USER_MATRIX_INDEX,
  COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX,

  /* note: layers don't currently have any non-sparse state */

  COGL_PIPELINE_LAYER_STATE_SPARSE_COUNT,
  COGL_PIPELINE_LAYER_STATE_COUNT = COGL_PIPELINE_LAYER_STATE_SPARSE_COUNT
} CoglPipelineLayerStateIndex;

/* XXX: If you add or remove state groups here you may need to update
 * some of the state masks following this enum too!
 *
 * FIXME: perhaps it would be better to rename this enum to
 * CoglPipelineLayerStateGroup to better convey the fact that a single
 * enum here can map to multiple properties.
 */
typedef enum
{
  COGL_PIPELINE_LAYER_STATE_UNIT =
    1L<<COGL_PIPELINE_LAYER_STATE_UNIT_INDEX,
  COGL_PIPELINE_LAYER_STATE_TEXTURE_TARGET =
    1L<<COGL_PIPELINE_LAYER_STATE_TEXTURE_TARGET_INDEX,
  COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA =
    1L<<COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX,
  COGL_PIPELINE_LAYER_STATE_FILTERS =
    1L<<COGL_PIPELINE_LAYER_STATE_FILTERS_INDEX,
  COGL_PIPELINE_LAYER_STATE_WRAP_MODES =
    1L<<COGL_PIPELINE_LAYER_STATE_WRAP_MODES_INDEX,

  COGL_PIPELINE_LAYER_STATE_COMBINE =
    1L<<COGL_PIPELINE_LAYER_STATE_COMBINE_INDEX,
  COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT =
    1L<<COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT_INDEX,
  COGL_PIPELINE_LAYER_STATE_USER_MATRIX =
    1L<<COGL_PIPELINE_LAYER_STATE_USER_MATRIX_INDEX,

  COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS =
    1L<<COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX,

  /* COGL_PIPELINE_LAYER_STATE_TEXTURE_INTERN   = 1L<<8, */

} CoglPipelineLayerState;

/*
 * Various special masks that tag state-groups in different ways...
 */

#define COGL_PIPELINE_LAYER_STATE_ALL \
  ((1L<<COGL_PIPELINE_LAYER_STATE_COUNT) - 1)

#define COGL_PIPELINE_LAYER_STATE_ALL_SPARSE \
  COGL_PIPELINE_LAYER_STATE_ALL

#define COGL_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE \
  (COGL_PIPELINE_LAYER_STATE_COMBINE | \
   COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT | \
   COGL_PIPELINE_LAYER_STATE_USER_MATRIX | \
   COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS)

#define COGL_PIPELINE_LAYER_STATE_MULTI_PROPERTY \
  (COGL_PIPELINE_LAYER_STATE_FILTERS | \
   COGL_PIPELINE_LAYER_STATE_WRAP_MODES | \
   COGL_PIPELINE_LAYER_STATE_COMBINE)

#define COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN 0


typedef enum
{
  /* These are the same values as GL */
  COGL_PIPELINE_COMBINE_FUNC_ADD         = 0x0104,
  COGL_PIPELINE_COMBINE_FUNC_ADD_SIGNED  = 0x8574,
  COGL_PIPELINE_COMBINE_FUNC_SUBTRACT    = 0x84E7,
  COGL_PIPELINE_COMBINE_FUNC_INTERPOLATE = 0x8575,
  COGL_PIPELINE_COMBINE_FUNC_REPLACE     = 0x1E01,
  COGL_PIPELINE_COMBINE_FUNC_MODULATE    = 0x2100,
  COGL_PIPELINE_COMBINE_FUNC_DOT3_RGB    = 0x86AE,
  COGL_PIPELINE_COMBINE_FUNC_DOT3_RGBA   = 0x86AF
} CoglPipelineCombineFunc;

typedef enum
{
  /* These are the same values as GL */
  COGL_PIPELINE_COMBINE_SOURCE_TEXTURE       = 0x1702,
  COGL_PIPELINE_COMBINE_SOURCE_CONSTANT      = 0x8576,
  COGL_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR = 0x8577,
  COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS      = 0x8578,
  COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0      = 0x84C0
} CoglPipelineCombineSource;

typedef enum
{
  /* These are the same values as GL */
  COGL_PIPELINE_COMBINE_OP_SRC_COLOR           = 0x0300,
  COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_COLOR = 0x0301,
  COGL_PIPELINE_COMBINE_OP_SRC_ALPHA           = 0x0302,
  COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_ALPHA = 0x0303
} CoglPipelineCombineOp;

typedef struct
{
  /* The texture combine state determines how the color of individual
   * texture fragments are calculated. */
  CoglPipelineCombineFunc texture_combine_rgb_func;
  CoglPipelineCombineSource texture_combine_rgb_src[3];
  CoglPipelineCombineOp texture_combine_rgb_op[3];

  CoglPipelineCombineFunc texture_combine_alpha_func;
  CoglPipelineCombineSource texture_combine_alpha_src[3];
  CoglPipelineCombineOp texture_combine_alpha_op[3];

  float texture_combine_constant[4];

  /* The texture matrix dscribes how to transform texture coordinates */
  CoglMatrix matrix;

  gboolean point_sprite_coords;

} CoglPipelineLayerBigState;

typedef struct _CoglPipelineNode CoglPipelineNode;

COGL_LIST_HEAD (CoglPipelineNodeList, CoglPipelineNode);

/* Materials and layers represent their state in a tree structure where
 * some of the state relating to a given pipeline or layer may actually
 * be owned by one if is ancestors in the tree. We have a common data
 * type to track the tree heirachy so we can share code... */
struct _CoglPipelineNode
{
  /* the parent in terms of class hierarchy, so anything inheriting
   * from CoglPipelineNode also inherits from CoglObject. */
  CoglObject _parent;

  /* The parent pipeline/layer */
  CoglPipelineNode *parent;

  /* The list entry here contains pointers to the node's siblings */
  COGL_LIST_ENTRY (CoglPipelineNode) list_node;

  /* List of children */
  CoglPipelineNodeList children;

  /* TRUE if the node took a strong reference on its parent. Weak
   * pipelines for instance don't take a reference on their parent. */
  gboolean has_parent_reference;
};

#define COGL_PIPELINE_NODE(X) ((CoglPipelineNode *)(X))

typedef void (*CoglPipelineNodeUnparentVFunc) (CoglPipelineNode *node);

typedef gboolean (*CoglPipelineNodeChildCallback) (CoglPipelineNode *child,
                                                   void *user_data);

void
_cogl_pipeline_node_foreach_child (CoglPipelineNode *node,
                                   CoglPipelineNodeChildCallback callback,
                                   void *user_data);

/* This isn't defined in the GLES headers */
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif

/* GL_ALWAYS is just used here as a value that is known not to clash
 * with any valid GL wrap modes.
 *
 * XXX: keep the values in sync with the CoglPipelineWrapMode enum
 * so no conversion is actually needed.
 */
typedef enum _CoglPipelineWrapModeInternal
{
  COGL_PIPELINE_WRAP_MODE_INTERNAL_REPEAT = GL_REPEAT,
  COGL_PIPELINE_WRAP_MODE_INTERNAL_CLAMP_TO_EDGE = GL_CLAMP_TO_EDGE,
  COGL_PIPELINE_WRAP_MODE_INTERNAL_CLAMP_TO_BORDER = GL_CLAMP_TO_BORDER,
  COGL_PIPELINE_WRAP_MODE_INTERNAL_AUTOMATIC = GL_ALWAYS
} CoglPipelineWrapModeInternal;

struct _CoglPipelineLayer
{
  /* XXX: Please think twice about adding members that *have* be
   * initialized during a _cogl_pipeline_layer_copy. We are aiming
   * to have copies be as cheap as possible and copies may be
   * done by the primitives APIs which means they may happen
   * in performance critical code paths.
   *
   * XXX: If you are extending the state we track please consider if
   * the state is expected to vary frequently across many pipelines or
   * if the state can be shared among many derived pipelines instead.
   * This will determine if the state should be added directly to this
   * structure which will increase the memory overhead for *all*
   * layers or if instead it can go under ->big_state.
   */

  /* Layers represent their state in a tree structure where some of
   * the state relating to a given pipeline or layer may actually be
   * owned by one if is ancestors in the tree. We have a common data
   * type to track the tree heirachy so we can share code... */
  CoglPipelineNode _parent;

  /* Some layers have a pipeline owner, which is to say that the layer
   * is referenced in that pipelines->layer_differences list.  A layer
   * doesn't always have an owner and may simply be an ancestor for
   * other layers that keeps track of some shared state. */
  CoglPipeline      *owner;

  /* The lowest index is blended first then others on top */
  int	             index;

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
  GLenum                     target;

  CoglPipelineFilter         mag_filter;
  CoglPipelineFilter         min_filter;

  CoglPipelineWrapModeInternal wrap_mode_s;
  CoglPipelineWrapModeInternal wrap_mode_t;
  CoglPipelineWrapModeInternal wrap_mode_p;

  /* Infrequent differences aren't currently tracked in
   * a separate, dynamically allocated structure as they are
   * for pipelines... */
  CoglPipelineLayerBigState *big_state;

  /* bitfields */

  /* Determines if layer->big_state is valid */
  unsigned int          has_big_state:1;

};

/* XXX: should I rename these as
 * COGL_PIPELINE_STATE_INDEX_XYZ... ?
 */
typedef enum
{
  /* sparse state */
  COGL_PIPELINE_STATE_COLOR_INDEX,
  COGL_PIPELINE_STATE_BLEND_ENABLE_INDEX,
  COGL_PIPELINE_STATE_LAYERS_INDEX,
  COGL_PIPELINE_STATE_LIGHTING_INDEX,
  COGL_PIPELINE_STATE_ALPHA_FUNC_INDEX,
  COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE_INDEX,
  COGL_PIPELINE_STATE_BLEND_INDEX,
  COGL_PIPELINE_STATE_USER_SHADER_INDEX,
  COGL_PIPELINE_STATE_DEPTH_INDEX,
  COGL_PIPELINE_STATE_FOG_INDEX,
  COGL_PIPELINE_STATE_POINT_SIZE_INDEX,

  /* non-sparse */
  COGL_PIPELINE_STATE_REAL_BLEND_ENABLE_INDEX,

  COGL_PIPELINE_STATE_COUNT,
  COGL_PIPELINE_STATE_SPARSE_COUNT = COGL_PIPELINE_STATE_COUNT - 1,
} CoglPipelineStateIndex;

/* Used in pipeline->differences masks and for notifying pipeline
 * state changes.
 *
 * XXX: If you add or remove state groups here you may need to update
 * some of the state masks following this enum too!
 *
 * FIXME: perhaps it would be better to rename this enum to
 * CoglPipelineStateGroup to better convey the fact that a single enum
 * here can map to multiple properties.
 */
typedef enum _CoglPipelineState
{
  COGL_PIPELINE_STATE_COLOR =
    1L<<COGL_PIPELINE_STATE_COLOR_INDEX,
  COGL_PIPELINE_STATE_BLEND_ENABLE =
    1L<<COGL_PIPELINE_STATE_BLEND_ENABLE_INDEX,
  COGL_PIPELINE_STATE_LAYERS =
    1L<<COGL_PIPELINE_STATE_LAYERS_INDEX,

  COGL_PIPELINE_STATE_LIGHTING =
    1L<<COGL_PIPELINE_STATE_LIGHTING_INDEX,
  COGL_PIPELINE_STATE_ALPHA_FUNC =
    1L<<COGL_PIPELINE_STATE_ALPHA_FUNC_INDEX,
  COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE =
    1L<<COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE_INDEX,
  COGL_PIPELINE_STATE_BLEND =
    1L<<COGL_PIPELINE_STATE_BLEND_INDEX,
  COGL_PIPELINE_STATE_USER_SHADER =
    1L<<COGL_PIPELINE_STATE_USER_SHADER_INDEX,
  COGL_PIPELINE_STATE_DEPTH =
    1L<<COGL_PIPELINE_STATE_DEPTH_INDEX,
  COGL_PIPELINE_STATE_FOG =
    1L<<COGL_PIPELINE_STATE_FOG_INDEX,
  COGL_PIPELINE_STATE_POINT_SIZE =
    1L<<COGL_PIPELINE_STATE_POINT_SIZE_INDEX,

  COGL_PIPELINE_STATE_REAL_BLEND_ENABLE =
    1L<<COGL_PIPELINE_STATE_REAL_BLEND_ENABLE_INDEX,

} CoglPipelineState;

/*
 * Various special masks that tag state-groups in different ways...
 */

#define COGL_PIPELINE_STATE_ALL \
  ((1L<<COGL_PIPELINE_STATE_COUNT) - 1)

#define COGL_PIPELINE_STATE_ALL_SPARSE \
  (COGL_PIPELINE_STATE_ALL \
   & ~COGL_PIPELINE_STATE_REAL_BLEND_ENABLE)

#define COGL_PIPELINE_STATE_AFFECTS_BLENDING \
  (COGL_PIPELINE_STATE_COLOR | \
   COGL_PIPELINE_STATE_BLEND_ENABLE | \
   COGL_PIPELINE_STATE_LAYERS | \
   COGL_PIPELINE_STATE_LIGHTING | \
   COGL_PIPELINE_STATE_BLEND | \
   COGL_PIPELINE_STATE_USER_SHADER)

#define COGL_PIPELINE_STATE_NEEDS_BIG_STATE \
  (COGL_PIPELINE_STATE_LIGHTING | \
   COGL_PIPELINE_STATE_ALPHA_FUNC | \
   COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE | \
   COGL_PIPELINE_STATE_BLEND | \
   COGL_PIPELINE_STATE_USER_SHADER | \
   COGL_PIPELINE_STATE_DEPTH | \
   COGL_PIPELINE_STATE_FOG | \
   COGL_PIPELINE_STATE_POINT_SIZE)

#define COGL_PIPELINE_STATE_MULTI_PROPERTY \
  (COGL_PIPELINE_STATE_LAYERS | \
   COGL_PIPELINE_STATE_LIGHTING | \
   COGL_PIPELINE_STATE_BLEND | \
   COGL_PIPELINE_STATE_DEPTH | \
   COGL_PIPELINE_STATE_FOG)

#define COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN \
  (COGL_PIPELINE_STATE_LAYERS | \
   COGL_PIPELINE_STATE_USER_SHADER)

typedef enum
{
  COGL_PIPELINE_LIGHTING_STATE_PROPERTY_AMBIENT = 1,
  COGL_PIPELINE_LIGHTING_STATE_PROPERTY_DIFFUSE,
  COGL_PIPELINE_LIGHTING_STATE_PROPERTY_SPECULAR,
  COGL_PIPELINE_LIGHTING_STATE_PROPERTY_EMISSION,
  COGL_PIPELINE_LIGHTING_STATE_PROPERTY_SHININESS
} CoglPipelineLightingStateProperty;

typedef struct
{
  /* Standard OpenGL lighting model attributes */
  float ambient[4];
  float diffuse[4];
  float specular[4];
  float emission[4];
  float shininess;
} CoglPipelineLightingState;

typedef struct
{
  /* Determines what fragments are discarded based on their alpha */
  CoglPipelineAlphaFunc alpha_func;
  float		        alpha_func_reference;
} CoglPipelineAlphaFuncState;

typedef enum _CoglPipelineBlendEnable
{
  /* XXX: we want to detect users mistakenly using TRUE or FALSE
   * so start the enum at 2. */
  COGL_PIPELINE_BLEND_ENABLE_ENABLED = 2,
  COGL_PIPELINE_BLEND_ENABLE_DISABLED,
  COGL_PIPELINE_BLEND_ENABLE_AUTOMATIC
} CoglPipelineBlendEnable;

typedef struct
{
  /* Determines how this pipeline is blended with other primitives */
#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)
  GLenum    blend_equation_rgb;
  GLenum    blend_equation_alpha;
  GLint     blend_src_factor_alpha;
  GLint     blend_dst_factor_alpha;
  CoglColor blend_constant;
#endif
  GLint     blend_src_factor_rgb;
  GLint     blend_dst_factor_rgb;
} CoglPipelineBlendState;

typedef struct
{
  gboolean        enabled;
  CoglColor       color;
  CoglFogMode     mode;
  float           density;
  float           z_near;
  float           z_far;
} CoglPipelineFogState;

typedef struct
{
  CoglPipelineLightingState lighting_state;
  CoglPipelineAlphaFuncState alpha_state;
  CoglPipelineBlendState blend_state;
  CoglHandle user_program;
  CoglDepthState depth_state;
  CoglPipelineFogState fog_state;
  float point_size;
} CoglPipelineBigState;

typedef enum
{
  COGL_PIPELINE_FLAG_DIRTY_LAYERS_CACHE     = 1L<<0,
  COGL_PIPELINE_FLAG_DIRTY_GET_LAYERS_LIST  = 1L<<1
} CoglPipelineFlag;

typedef struct
{
  CoglPipeline *owner;
  CoglPipelineLayer *layer;
} CoglPipelineLayerCacheEntry;

/*
 * CoglPipelineDestroyCallback
 * @pipeline: The #CoglPipeline that has been destroyed
 * @user_data: The private data associated with the callback
 *
 * Notifies when a weak pipeline has been destroyed because one
 * of its ancestors has been freed or modified.
 */
typedef void (*CoglPipelineDestroyCallback)(CoglPipeline *pipeline,
                                            void *user_data);

struct _CoglPipeline
{
  /* XXX: Please think twice about adding members that *have* be
   * initialized during a cogl_pipeline_copy. We are aiming to have
   * copies be as cheap as possible and copies may be done by the
   * primitives APIs which means they may happen in performance
   * critical code paths.
   *
   * XXX: If you are extending the state we track please consider if
   * the state is expected to vary frequently across many pipelines or
   * if the state can be shared among many derived pipelines instead.
   * This will determine if the state should be added directly to this
   * structure which will increase the memory overhead for *all*
   * pipelines or if instead it can go under ->big_state.
   */

  /* Layers represent their state in a tree structure where some of
   * the state relating to a given pipeline or layer may actually be
   * owned by one if is ancestors in the tree. We have a common data
   * type to track the tree heirachy so we can share code... */
  CoglPipelineNode _parent;

  /* We need to track if a pipeline is referenced in the journal
   * because we can't allow modification to these pipelines without
   * flushing the journal first */
  unsigned long    journal_ref_count;

  /* When weak pipelines are destroyed the user is notified via this
   * callback */
  CoglPipelineDestroyCallback destroy_callback;

  /* When notifying that a weak pipeline has been destroyed this
   * private data is passed to the above callback */
  void *destroy_data;

  /* A mask of which sparse state groups are different in this
   * pipeline in comparison to its parent. */
  unsigned long    differences;

  /* The fragment processing backends can associate private data with a
   * pipeline. */
  void		  *fragend_privs[COGL_PIPELINE_N_FRAGENDS];

  /* Whenever a pipeline is modified we increment the age. There's no
   * guarantee that it won't wrap but it can nevertheless be a
   * convenient mechanism to determine when a pipeline has been
   * changed to you can invalidate some some associated cache that
   * depends on the old state. */
  unsigned long    age;

  /* This is the primary color of the pipeline.
   *
   * This is a sparse property, ref COGL_PIPELINE_STATE_COLOR */
  CoglColor        color;

  /* A pipeline may be made up with multiple layers used to combine
   * textures together.
   *
   * This is sparse state, ref COGL_PIPELINE_STATE_LAYERS */
  GList	          *layer_differences;
  unsigned int     n_layers;

  /* As a basic way to reduce memory usage we divide the pipeline
   * state into two groups; the minimal state modified in 90% of
   * all pipelines and the rest, so that the second group can
   * be allocated dynamically when required... */
  CoglPipelineBigState *big_state;

  /* For debugging purposes it's possible to associate a static const
   * string with a pipeline which can be an aid when trying to trace
   * where the pipeline originates from */
  const char      *static_breadcrumb;

  /* Cached state... */

  /* A cached, complete list of the layers this pipeline depends
   * on sorted by layer->unit_index. */
  CoglPipelineLayer   **layers_cache;
  /* To avoid a separate ->layers_cache allocation for common
   * pipelines with only a few layers... */
  CoglPipelineLayer    *short_layers_cache[3];

  /* The deprecated cogl_pipeline_get_layers() API returns a
   * const GList of layers, which we track here... */
  GList                *deprecated_get_layers_list;

  /* XXX: consider adding an authorities cache to speed up sparse
   * property value lookups:
   * CoglPipeline *authorities_cache[COGL_PIPELINE_N_SPARSE_PROPERTIES];
   * and corresponding authorities_cache_dirty:1 bitfield
   */

  /* bitfields */

  /* A pipeline can have private data associated with it for multiple
   * fragment processing backends. Although only one backend is
   * associated with a pipeline the backends may want to cache private
   * state with the ancestors of other pipelines and those ancestors
   * could currently be associated with different backends.
   *
   * Each set bit indicates if the corresponding ->fragend_privs[]
   * entry is valid.
   */
  unsigned int          fragend_priv_set_mask:COGL_PIPELINE_N_FRAGENDS;

  /* Weak pipelines don't count as dependants on their parents which
   * means that the parent pipeline can be modified without
   * considering how the modifications may affect the weak pipeline.
   */
  unsigned int          is_weak:1;

  /* Determines if pipeline->big_state is valid */
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
   * string with a pipeline which can be an aid when trying to trace
   * where the pipeline originates from */
  unsigned int          has_static_breadcrumb:1;

  /* There are multiple fragment processing backends for CoglPipeline,
   * glsl, arbfp and fixed. This identifies the backend being used for
   * the pipeline and any private state the backend has associated
   * with the pipeline. */
  unsigned int          fragend:3;
  unsigned int          vertend:3;
};

typedef struct _CoglPipelineFragend
{
  gboolean (*start) (CoglPipeline *pipeline,
                     int n_layers,
                     unsigned long pipelines_difference,
                     int n_tex_coord_attribs);
  gboolean (*add_layer) (CoglPipeline *pipeline,
                         CoglPipelineLayer *layer,
                         unsigned long layers_difference);
  gboolean (*passthrough) (CoglPipeline *pipeline);
  gboolean (*end) (CoglPipeline *pipeline,
                   unsigned long pipelines_difference);

  void (*pipeline_pre_change_notify) (CoglPipeline *pipeline,
                                      CoglPipelineState change,
                                      const CoglColor *new_color);
  void (*pipeline_set_parent_notify) (CoglPipeline *pipeline);
  void (*layer_pre_change_notify) (CoglPipeline *owner,
                                   CoglPipelineLayer *layer,
                                   CoglPipelineLayerState change);

  void (*free_priv) (CoglPipeline *pipeline);
} CoglPipelineFragend;

typedef struct _CoglPipelineVertend
{
  gboolean (*start) (CoglPipeline *pipeline,
                     int n_layers,
                     unsigned long pipelines_difference);
  gboolean (*add_layer) (CoglPipeline *pipeline,
                         CoglPipelineLayer *layer,
                         unsigned long layers_difference);
  gboolean (*end) (CoglPipeline *pipeline,
                   unsigned long pipelines_difference);

  void (*pipeline_pre_change_notify) (CoglPipeline *pipeline,
                                      CoglPipelineState change,
                                      const CoglColor *new_color);
  void (*layer_pre_change_notify) (CoglPipeline *owner,
                                   CoglPipelineLayer *layer,
                                   CoglPipelineLayerState change);
} CoglPipelineVertend;

typedef struct
{
  void (*end) (CoglPipeline *pipeline,
               unsigned long pipelines_difference,
               int n_tex_coord_attribs);
  void (*pipeline_pre_change_notify) (CoglPipeline *pipeline,
                                      CoglPipelineState change,
                                      const CoglColor *new_color);
  void (*layer_pre_change_notify) (CoglPipeline *owner,
                                   CoglPipelineLayer *layer,
                                   CoglPipelineLayerState change);
  /* This is called after all of the other functions whenever the
     pipeline is flushed, even if the pipeline hasn't changed since
     the last flush */
  void (* pre_paint) (CoglPipeline *pipeline);
} CoglPipelineProgend;

typedef enum
{
  COGL_PIPELINE_PROGRAM_TYPE_GLSL = 1,
  COGL_PIPELINE_PROGRAM_TYPE_ARBFP,
  COGL_PIPELINE_PROGRAM_TYPE_FIXED
} CoglPipelineProgramType;

extern const CoglPipelineFragend *
_cogl_pipeline_fragends[COGL_PIPELINE_N_FRAGENDS];
extern const CoglPipelineVertend *
_cogl_pipeline_vertends[COGL_PIPELINE_N_VERTENDS];
extern const CoglPipelineProgend *
_cogl_pipeline_progends[];

void
_cogl_pipeline_init_default_pipeline (void);

void
_cogl_pipeline_init_default_layers (void);

static inline CoglPipeline *
_cogl_pipeline_get_parent (CoglPipeline *pipeline)
{
  CoglPipelineNode *parent_node = COGL_PIPELINE_NODE (pipeline)->parent;
  return COGL_PIPELINE (parent_node);
}

static inline CoglPipeline *
_cogl_pipeline_get_authority (CoglPipeline *pipeline,
                              unsigned long difference)
{
  CoglPipeline *authority = pipeline;
  while (!(authority->differences & difference))
    authority = _cogl_pipeline_get_parent (authority);
  return authority;
}

/*
 * SECTION:cogl-pipeline-internals
 * @short_description: Functions for creating custom primitives that make use
 *    of Cogl pipelines for filling.
 *
 * Normally you shouldn't need to use this API directly, but if you need to
 * developing a custom/specialised primitive - probably using raw OpenGL - then
 * this API aims to expose enough of the pipeline internals to support being
 * able to fill your geometry according to a given Cogl pipeline.
 */

gboolean
_cogl_pipeline_get_real_blend_enabled (CoglPipeline *pipeline);

gboolean
_cogl_pipeline_layer_has_user_matrix (CoglPipeline *pipeline,
                                      int layer_index);

/*
 * Calls the pre_paint method on the layer texture if there is
 * one. This will determine whether mipmaps are needed based on the
 * filter settings.
 */
void
_cogl_pipeline_layer_pre_paint (CoglPipelineLayer *layerr);

/*
 * Calls the pre_paint method on the layer texture if there is
 * one. This will determine whether mipmaps are needed based on the
 * filter settings.
 */
void
_cogl_pipeline_pre_paint_for_layer (CoglPipeline *pipeline,
                                    int layer_id);

/*
 * CoglPipelineFlushFlag:
 * @COGL_PIPELINE_FLUSH_FALLBACK_MASK: The fallback_layers member is set to
 *      a guint32 mask of the layers that can't be supported with the user
 *      supplied texture and need to be replaced with fallback textures. (1 =
 *      fallback, and the least significant bit = layer 0)
 * @COGL_PIPELINE_FLUSH_DISABLE_MASK: The disable_layers member is set to
 *      a guint32 mask of the layers that you want to completly disable
 *      texturing for (1 = fallback, and the least significant bit = layer 0)
 * @COGL_PIPELINE_FLUSH_LAYER0_OVERRIDE: The layer0_override_texture member is
 *      set to a GLuint OpenGL texture name to override the texture used for
 *      layer 0 of the pipeline. This is intended for dealing with sliced
 *      textures where you will need to point to each of the texture slices in
 *      turn when drawing your geometry.  Passing a value of 0 is the same as
 *      not passing the option at all.
 * @COGL_PIPELINE_FLUSH_SKIP_GL_COLOR: When flushing the GL state for the
 *      pipeline don't call glColor.
 */
typedef enum _CoglPipelineFlushFlag
{
  COGL_PIPELINE_FLUSH_FALLBACK_MASK       = 1L<<0,
  COGL_PIPELINE_FLUSH_DISABLE_MASK        = 1L<<1,
  COGL_PIPELINE_FLUSH_LAYER0_OVERRIDE     = 1L<<2,
  COGL_PIPELINE_FLUSH_SKIP_GL_COLOR       = 1L<<3
} CoglPipelineFlushFlag;

/*
 * CoglPipelineFlushOptions:
 *
 */
typedef struct _CoglPipelineFlushOptions
{
  CoglPipelineFlushFlag         flags;

  guint32                       fallback_layers;
  guint32                       disable_layers;
  CoglHandle                    layer0_override_texture;
} CoglPipelineFlushOptions;


int
_cogl_get_max_texture_image_units (void);


void
_cogl_use_fragment_program (GLuint gl_program, CoglPipelineProgramType type);

void
_cogl_use_vertex_program (GLuint gl_program, CoglPipelineProgramType type);

unsigned int
_cogl_get_n_args_for_combine_func (CoglPipelineCombineFunc func);

/*
 * _cogl_pipeline_weak_copy:
 * @pipeline: A #CoglPipeline object
 * @callback: A callback to notify when your weak pipeline is destroyed
 * @user_data: Private data to pass to your given callback.
 *
 * Returns a weak copy of the given source @pipeline. Unlike a normal
 * copy no internal reference is taken on the source @pipeline and you
 * can expect that later modifications of the source pipeline (or in
 * fact any other pipeline) can result in the weak pipeline being
 * destroyed.
 *
 * To understand this better its good to know a bit about the internal
 * design of #CoglPipeline...
 *
 * Internally #CoglPipeline<!-- -->s are represented as a graph of
 * property diff's, where each node is a diff of properties that gets
 * applied on top of its parent. Copying a pipeline creates an empty
 * diff and a child->parent relationship between the empty diff and
 * the source @pipeline, parent.
 *
 * Because of this internal graph design a single #CoglPipeline may
 * indirectly depend on a chain of ancestors to fully define all of
 * its properties. Because a node depends on its ancestors it normally
 * owns a reference to its parent to stop it from being freed. Also if
 * you try to modify a pipeline with children we internally use a
 * copy-on-write mechanism to ensure that you don't indirectly change
 * the properties those children.
 *
 * Weak pipelines avoid the use of copy-on-write to preserve the
 * integrity of weak dependants and instead weak dependants are
 * simply destroyed allowing the parent to be modified directly. Also
 * because weak pipelines don't own a reference to their parent they
 * won't stop the source @pipeline from being freed when the user
 * releases their reference on it.
 *
 * Because weak pipelines don't own a reference on their parent they
 * are the recommended mechanism for creating derived pipelines that you
 * want to cache as a private property of the original pipeline
 * because they won't result in a circular dependency.
 *
 * An example use case:
 *
 * Consider for example you are implementing a custom primitive that is
 * not compatible with certain source pipelines. To handle this you
 * implement a validation stage that given an arbitrary pipeline as
 * input will create a derived pipeline that is suitable for drawing
 * your primitive.
 *
 * Because you don't want to have to repeat this validation every time
 * the same incompatible pipeline is given as input you want to cache
 * the result as a private property of the original pipeline. If the
 * derived pipeline were created using cogl_pipeline_copy that would
 * create a circular dependency so the original pipeline can never be
 * freed.
 *
 * If you instead create a weak copy you won't stop the original pipeline
 * from being freed if it's no longer needed, and you will instead simply
 * be notified that your weak pipeline has been destroyed.
 *
 * This is the recommended coding pattern for validating an input
 * pipeline and caching a derived result:
 * |[
 * static CoglUserDataKey _cogl_my_cache_key;
 *
 * typedef struct {
 *   CoglPipeline *validated_source;
 * } MyValidatedMaterialCache;
 *
 * static void
 * destroy_cache_cb (CoglObject *object, void *user_data)
 * {
 *   g_slice_free (MyValidatedMaterialCache, user_data);
 * }
 *
 * static void
 * invalidate_cache_cb (CoglPipeline *destroyed, void *user_data)
 * {
 *   MyValidatedMaterialCache *cache = user_data;
 *   cogl_object_unref (cache->validated_source);
 *   cache->validated_source = NULL;
 * }
 *
 * static CoglPipeline *
 * get_validated_pipeline (CoglPipeline *source)
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
 *           cogl_pipeline_weak_copy (source,
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
CoglPipeline *
_cogl_pipeline_weak_copy (CoglPipeline *pipeline,
                          CoglPipelineDestroyCallback callback,
                          void *user_data);

void
_cogl_pipeline_set_fragend (CoglPipeline *pipeline, int fragend);

void
_cogl_pipeline_set_vertend (CoglPipeline *pipeline, int vertend);

CoglPipeline *
_cogl_pipeline_get_parent (CoglPipeline *pipeline);

void
_cogl_pipeline_get_colorubv (CoglPipeline *pipeline,
                             guint8       *color);

/* XXX: At some point it could be good for this to accept a mask of
 * the state groups we are interested in comparing since we can
 * probably use that information in a number situations to reduce
 * the work we do. */
unsigned long
_cogl_pipeline_compare_differences (CoglPipeline *pipeline0,
                                    CoglPipeline *pipeline1);

/* Sometimes when evaluating pipelines, either during comparisons or
 * if calculating a hash value we need to tweak the evaluation
 * semantics */
typedef enum _CoglPipelineEvalFlags
{
  COGL_PIPELINE_EVAL_FLAG_NONE = 0
} CoglPipelineEvalFlags;

gboolean
_cogl_pipeline_equal (CoglPipeline *pipeline0,
                      CoglPipeline *pipeline1,
                      unsigned long differences,
                      unsigned long layer_differences,
                      CoglPipelineEvalFlags flags);

unsigned int
_cogl_pipeline_hash (CoglPipeline *pipeline,
                     unsigned long differences,
                     unsigned long layer_differences,
                     CoglPipelineEvalFlags flags);

CoglPipeline *
_cogl_pipeline_journal_ref (CoglPipeline *pipeline);

void
_cogl_pipeline_journal_unref (CoglPipeline *pipeline);

CoglPipelineFilter
_cogl_pipeline_get_layer_min_filter (CoglPipeline *pipeline,
                                     int layer_index);

CoglPipelineFilter
_cogl_pipeline_get_layer_mag_filter (CoglPipeline *pipeline,
                                     int layer_index);

const CoglMatrix *
_cogl_pipeline_get_layer_matrix (CoglPipeline *pipeline,
                                 int layer_index);

void
_cogl_pipeline_texture_storage_change_notify (CoglHandle texture);

void
_cogl_pipeline_apply_legacy_state (CoglPipeline *pipeline);

void
_cogl_pipeline_apply_overrides (CoglPipeline *pipeline,
                                CoglPipelineFlushOptions *options);

CoglPipelineBlendEnable
_cogl_pipeline_get_blend_enabled (CoglPipeline *pipeline);

void
_cogl_pipeline_set_blend_enabled (CoglPipeline *pipeline,
                                  CoglPipelineBlendEnable enable);

void
_cogl_pipeline_set_static_breadcrumb (CoglPipeline *pipeline,
                                      const char *breadcrumb);

unsigned long
_cogl_pipeline_get_age (CoglPipeline *pipeline);

CoglPipeline *
_cogl_pipeline_get_authority (CoglPipeline *pipeline,
                              unsigned long difference);

CoglPipeline *
_cogl_pipeline_find_equivalent_parent (CoglPipeline *pipeline,
                                       CoglPipelineState pipeline_state,
                                       CoglPipelineLayerState layer_state);

CoglHandle
_cogl_pipeline_get_layer_texture (CoglPipeline *pipeline,
                                  int layer_index);

void
_cogl_pipeline_get_layer_combine_constant (CoglPipeline *pipeline,
                                           int layer_index,
                                           float *constant);

void
_cogl_pipeline_prune_to_n_layers (CoglPipeline *pipeline, int n);


/*
 * API to support the deprecate cogl_pipeline_layer_xyz functions...
 */

const GList *
_cogl_pipeline_get_layers (CoglPipeline *pipeline);

void
_cogl_pipeline_layer_get_wrap_modes (CoglPipelineLayer *layer,
                                     CoglPipelineWrapModeInternal *wrap_mode_s,
                                     CoglPipelineWrapModeInternal *wrap_mode_t,
                                     CoglPipelineWrapModeInternal *wrap_mode_r);

void
_cogl_pipeline_layer_get_filters (CoglPipelineLayer *layer,
                                  CoglPipelineFilter *min_filter,
                                  CoglPipelineFilter *mag_filter);

void
_cogl_pipeline_get_layer_filters (CoglPipeline *pipeline,
                                  int layer_index,
                                  CoglPipelineFilter *min_filter,
                                  CoglPipelineFilter *mag_filter);

typedef enum {
  COGL_PIPELINE_LAYER_TYPE_TEXTURE
} CoglPipelineLayerType;

CoglPipelineLayerType
_cogl_pipeline_layer_get_type (CoglPipelineLayer *layer);

CoglHandle
_cogl_pipeline_layer_get_texture (CoglPipelineLayer *layer);

CoglHandle
_cogl_pipeline_layer_get_texture_real (CoglPipelineLayer *layer);

CoglPipelineFilter
_cogl_pipeline_layer_get_min_filter (CoglPipelineLayer *layer);

CoglPipelineFilter
_cogl_pipeline_layer_get_mag_filter (CoglPipelineLayer *layer);

CoglPipelineWrapMode
_cogl_pipeline_layer_get_wrap_mode_s (CoglPipelineLayer *layer);

CoglPipelineWrapMode
_cogl_pipeline_layer_get_wrap_mode_t (CoglPipelineLayer *layer);

CoglPipelineWrapMode
_cogl_pipeline_layer_get_wrap_mode_p (CoglPipelineLayer *layer);

unsigned long
_cogl_pipeline_layer_compare_differences (CoglPipelineLayer *layer0,
                                          CoglPipelineLayer *layer1);

CoglPipelineLayer *
_cogl_pipeline_layer_get_authority (CoglPipelineLayer *layer,
                                    unsigned long difference);

CoglHandle
_cogl_pipeline_layer_get_texture (CoglPipelineLayer *layer);

typedef gboolean (*CoglPipelineInternalLayerCallback) (CoglPipelineLayer *layer,
                                                       void *user_data);

void
_cogl_pipeline_foreach_layer_internal (CoglPipeline *pipeline,
                                       CoglPipelineInternalLayerCallback callback,
                                       void *user_data);

int
_cogl_pipeline_layer_get_unit_index (CoglPipelineLayer *layer);

gboolean
_cogl_pipeline_need_texture_combine_separate
                                    (CoglPipelineLayer *combine_authority);

void
_cogl_pipeline_init_state_hash_functions (void);

void
_cogl_pipeline_init_layer_state_hash_functions (void);

CoglPipelineLayerState
_cogl_pipeline_get_layer_state_for_fragment_codegen (CoglContext *context);

CoglPipelineState
_cogl_pipeline_get_state_for_fragment_codegen (CoglContext *context);

#endif /* __COGL_PIPELINE_PRIVATE_H */

