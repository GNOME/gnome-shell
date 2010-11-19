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

#include <glib.h>

typedef struct _CoglPipelineLayer     CoglPipelineLayer;
#define COGL_PIPELINE_LAYER(OBJECT) ((CoglPipelineLayer *)OBJECT)

#if defined (HAVE_COGL_GL)

/* NB: pipeline->backend is currently a 3bit unsigned int bitfield */
#define COGL_PIPELINE_BACKEND_GLSL       0
#define COGL_PIPELINE_BACKEND_GLSL_MASK  (1L<<0)
#define COGL_PIPELINE_BACKEND_ARBFP      1
#define COGL_PIPELINE_BACKEND_ARBFP_MASK (1L<<1)
#define COGL_PIPELINE_BACKEND_FIXED      2
#define COGL_PIPELINE_BACKEND_FIXED_MASK (1L<<2)

#define COGL_PIPELINE_N_BACKENDS         3

#elif defined (HAVE_COGL_GLES2)

#define COGL_PIPELINE_BACKEND_GLSL       0
#define COGL_PIPELINE_BACKEND_GLSL_MASK  (1L<<0)
#define COGL_PIPELINE_BACKEND_FIXED      1
#define COGL_PIPELINE_BACKEND_FIXED_MASK (1L<<1)

#define COGL_PIPELINE_N_BACKENDS         2

#else /* HAVE_COGL_GLES */

#define COGL_PIPELINE_BACKEND_FIXED      0
#define COGL_PIPELINE_BACKEND_FIXED_MASK (1L<<0)

#define COGL_PIPELINE_N_BACKENDS         1

#endif

#define COGL_PIPELINE_BACKEND_DEFAULT    0
#define COGL_PIPELINE_BACKEND_UNDEFINED  3

typedef enum
{
  COGL_PIPELINE_LAYER_STATE_UNIT             = 1L<<0,
  COGL_PIPELINE_LAYER_STATE_TEXTURE          = 1L<<1,
  COGL_PIPELINE_LAYER_STATE_FILTERS          = 1L<<2,
  COGL_PIPELINE_LAYER_STATE_WRAP_MODES       = 1L<<3,

  COGL_PIPELINE_LAYER_STATE_COMBINE          = 1L<<4,
  COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT = 1L<<5,
  COGL_PIPELINE_LAYER_STATE_USER_MATRIX      = 1L<<6,

  COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS = 1L<<7,

  /* COGL_PIPELINE_LAYER_STATE_TEXTURE_INTERN   = 1L<<8, */

  COGL_PIPELINE_LAYER_STATE_ALL_SPARSE =
    COGL_PIPELINE_LAYER_STATE_UNIT |
    COGL_PIPELINE_LAYER_STATE_TEXTURE |
    COGL_PIPELINE_LAYER_STATE_FILTERS |
    COGL_PIPELINE_LAYER_STATE_WRAP_MODES |
    COGL_PIPELINE_LAYER_STATE_COMBINE |
    COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT |
    COGL_PIPELINE_LAYER_STATE_USER_MATRIX |
    COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS,

  COGL_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE =
    COGL_PIPELINE_LAYER_STATE_COMBINE |
    COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT |
    COGL_PIPELINE_LAYER_STATE_USER_MATRIX |
    COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS,

} CoglPipelineLayerState;

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

} CoglPipelineLayerBigState;

/* Materials and layers represent their state in a tree structure where
 * some of the state relating to a given pipeline or layer may actually
 * be owned by one if is ancestors in the tree. We have a common data
 * type to track the tree heirachy so we can share code... */
typedef struct _CoglPipelineNode CoglPipelineNode;
struct _CoglPipelineNode
{
  /* the parent in terms of class hierarchy, so anything inheriting
   * from CoglPipelineNode also inherits from CoglObject. */
  CoglObject _parent;

  /* The parent pipeline/layer */
  CoglPipelineNode *parent;

  /* TRUE if the node took a strong reference on its parent. Weak
   * pipelines for instance don't take a reference on their parent. */
  gboolean has_parent_reference;

  /* As an optimization for creating leaf node pipelines/layers (the
   * most common) we don't require any list node allocations to link
   * to a single descendant. */
  CoglPipelineNode *first_child;

  /* Determines if node->first_child and node->children are
   * initialized pointers. */
  gboolean has_children;

  /* Materials and layers are sparse structures defined as a diff
   * against their parent and may have multiple children which depend
   * on them to define the values of properties which they don't
   * change. */
  GList *children;
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

  /* Different pipeline backends (GLSL/ARBfp/Fixed Function) may
   * want to associate private data with a layer...
   *
   * NB: we have per backend pointers because a layer may be
   * associated with multiple pipelines with different backends.
   */
  void              *backend_priv[COGL_PIPELINE_N_BACKENDS];

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

/* Used in pipeline->differences masks and for notifying pipeline
 * state changes... */
typedef enum _CoglPipelineState
{
  COGL_PIPELINE_STATE_COLOR             = 1L<<0,
  COGL_PIPELINE_STATE_BLEND_ENABLE      = 1L<<1,
  COGL_PIPELINE_STATE_LAYERS            = 1L<<2,

  COGL_PIPELINE_STATE_LIGHTING          = 1L<<3,
  COGL_PIPELINE_STATE_ALPHA_FUNC        = 1L<<4,
  COGL_PIPELINE_STATE_BLEND             = 1L<<5,
  COGL_PIPELINE_STATE_USER_SHADER       = 1L<<6,
  COGL_PIPELINE_STATE_DEPTH             = 1L<<7,
  COGL_PIPELINE_STATE_FOG               = 1L<<8,
  COGL_PIPELINE_STATE_POINT_SIZE        = 1L<<9,

  COGL_PIPELINE_STATE_REAL_BLEND_ENABLE = 1L<<10,

  COGL_PIPELINE_STATE_ALL_SPARSE =
    COGL_PIPELINE_STATE_COLOR |
    COGL_PIPELINE_STATE_BLEND_ENABLE |
    COGL_PIPELINE_STATE_LAYERS |
    COGL_PIPELINE_STATE_LIGHTING |
    COGL_PIPELINE_STATE_ALPHA_FUNC |
    COGL_PIPELINE_STATE_BLEND |
    COGL_PIPELINE_STATE_USER_SHADER |
    COGL_PIPELINE_STATE_DEPTH |
    COGL_PIPELINE_STATE_FOG |
    COGL_PIPELINE_STATE_POINT_SIZE,

  COGL_PIPELINE_STATE_AFFECTS_BLENDING =
    COGL_PIPELINE_STATE_COLOR |
    COGL_PIPELINE_STATE_BLEND_ENABLE |
    COGL_PIPELINE_STATE_LAYERS |
    COGL_PIPELINE_STATE_LIGHTING |
    COGL_PIPELINE_STATE_BLEND |
    COGL_PIPELINE_STATE_USER_SHADER,

  COGL_PIPELINE_STATE_NEEDS_BIG_STATE =
    COGL_PIPELINE_STATE_LIGHTING |
    COGL_PIPELINE_STATE_ALPHA_FUNC |
    COGL_PIPELINE_STATE_BLEND |
    COGL_PIPELINE_STATE_USER_SHADER |
    COGL_PIPELINE_STATE_DEPTH |
    COGL_PIPELINE_STATE_FOG |
    COGL_PIPELINE_STATE_POINT_SIZE

} CoglPipelineState;

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
  GLfloat		alpha_func_reference;
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
#ifndef HAVE_COGL_GLES
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
  gboolean              depth_test_enabled;
  CoglDepthTestFunction depth_test_function;
  gboolean              depth_writing_enabled;
  float                 depth_range_near;
  float                 depth_range_far;
} CoglPipelineDepthState;

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
  CoglPipelineDepthState depth_state;
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
  void		  *backend_privs[COGL_PIPELINE_N_BACKENDS];

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
   * Each set bit indicates if the correspondong ->backend_privs[]
   * entry is valid.
   */
  unsigned int          backend_priv_set_mask:COGL_PIPELINE_N_BACKENDS;

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
  unsigned int          backend:3;
};

typedef struct _CoglPipelineBackend
{
  int (*get_max_texture_units) (void);

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
  void (*free_layer_priv) (CoglPipelineLayer *layer);
} CoglPipelineBackend;

typedef enum
{
  COGL_PIPELINE_PROGRAM_TYPE_GLSL = 1,
  COGL_PIPELINE_PROGRAM_TYPE_ARBFP,
  COGL_PIPELINE_PROGRAM_TYPE_FIXED
} CoglPipelineProgramType;

extern const CoglPipelineBackend *
_cogl_pipeline_backends[COGL_PIPELINE_N_BACKENDS];

void
_cogl_pipeline_init_default_pipeline (void);

void
_cogl_pipeline_init_default_layers (void);

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
_cogl_use_program (GLuint gl_program, CoglPipelineProgramType type);

unsigned int
_cogl_get_n_args_for_combine_func (GLint func);

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
_cogl_pipeline_set_backend (CoglPipeline *pipeline, int backend);

CoglPipeline *
_cogl_pipeline_get_parent (CoglPipeline *pipeline);

void
_cogl_pipeline_get_colorubv (CoglPipeline *pipeline,
                             guint8       *color);

unsigned long
_cogl_pipeline_compare_differences (CoglPipeline *pipeline0,
                                    CoglPipeline *pipeline1);

gboolean
_cogl_pipeline_equal (CoglPipeline *pipeline0,
                      CoglPipeline *pipeline1,
                      gboolean skip_gl_color);

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

G_CONST_RETURN GList *
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

CoglPipeline *
_cogl_pipeline_find_codegen_authority (CoglPipeline *pipeline,
                                       CoglHandle user_program);

#endif /* __COGL_PIPELINE_PRIVATE_H */

