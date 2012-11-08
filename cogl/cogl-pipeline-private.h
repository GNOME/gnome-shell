/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010,2011 Intel Corporation.
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

#include "cogl-node-private.h"
#include "cogl-pipeline-layer-private.h"
#include "cogl-pipeline.h"
#include "cogl-matrix.h"
#include "cogl-object-private.h"
#include "cogl-profile.h"
#include "cogl-queue.h"
#include "cogl-boxed-value.h"
#include "cogl-pipeline-snippet-private.h"
#include "cogl-pipeline-state.h"
#include "cogl-framebuffer.h"
#include "cogl-bitmask.h"

#include <glib.h>

#ifdef HAVE_COGL_GL

#define COGL_PIPELINE_PROGEND_FIXED_ARBFP 0
#define COGL_PIPELINE_PROGEND_FIXED       1
#define COGL_PIPELINE_PROGEND_GLSL        2
#define COGL_PIPELINE_N_PROGENDS          3

#define COGL_PIPELINE_VERTEND_FIXED 0
#define COGL_PIPELINE_VERTEND_GLSL  1
#define COGL_PIPELINE_N_VERTENDS    2

#define COGL_PIPELINE_FRAGEND_ARBFP 0
#define COGL_PIPELINE_FRAGEND_FIXED 1
#define COGL_PIPELINE_FRAGEND_GLSL  2
#define COGL_PIPELINE_N_FRAGENDS    3

#else /* HAVE_COGL_GL */

#ifdef HAVE_COGL_GLES2

#define COGL_PIPELINE_PROGEND_GLSL 0
#define COGL_PIPELINE_VERTEND_GLSL 0
#define COGL_PIPELINE_FRAGEND_GLSL 0

#ifdef HAVE_COGL_GLES
#define COGL_PIPELINE_PROGEND_FIXED 1
#define COGL_PIPELINE_VERTEND_FIXED 1
#define COGL_PIPELINE_FRAGEND_FIXED 1

#define COGL_PIPELINE_N_PROGENDS    2
#define COGL_PIPELINE_N_VERTENDS    2
#define COGL_PIPELINE_N_FRAGENDS    2
#else
#define COGL_PIPELINE_N_PROGENDS    1
#define COGL_PIPELINE_N_VERTENDS    1
#define COGL_PIPELINE_N_FRAGENDS    1
#endif

#else /* HAVE_COGL_GLES2 */

#ifdef HAVE_COGL_GLES
#define COGL_PIPELINE_PROGEND_FIXED 0
#define COGL_PIPELINE_VERTEND_FIXED 0
#define COGL_PIPELINE_FRAGEND_FIXED 0
#define COGL_PIPELINE_N_PROGENDS    1
#define COGL_PIPELINE_N_VERTENDS    1
#define COGL_PIPELINE_N_FRAGENDS    1
#else
#error No drivers defined
#endif

#endif /* HAVE_COGL_GLES2 */

#endif /* HAVE_COGL_GL */

#define COGL_PIPELINE_PROGEND_DEFAULT    0
#define COGL_PIPELINE_PROGEND_UNDEFINED  3

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
  COGL_PIPELINE_STATE_PER_VERTEX_POINT_SIZE_INDEX,
  COGL_PIPELINE_STATE_LOGIC_OPS_INDEX,
  COGL_PIPELINE_STATE_CULL_FACE_INDEX,
  COGL_PIPELINE_STATE_UNIFORMS_INDEX,
  COGL_PIPELINE_STATE_VERTEX_SNIPPETS_INDEX,
  COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS_INDEX,

  /* non-sparse */
  COGL_PIPELINE_STATE_REAL_BLEND_ENABLE_INDEX,

  COGL_PIPELINE_STATE_COUNT
} CoglPipelineStateIndex;

#define COGL_PIPELINE_STATE_SPARSE_COUNT (COGL_PIPELINE_STATE_COUNT - 1)

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
  COGL_PIPELINE_STATE_PER_VERTEX_POINT_SIZE =
    1L<<COGL_PIPELINE_STATE_PER_VERTEX_POINT_SIZE_INDEX,
  COGL_PIPELINE_STATE_LOGIC_OPS =
    1L<<COGL_PIPELINE_STATE_LOGIC_OPS_INDEX,
  COGL_PIPELINE_STATE_CULL_FACE =
    1L<<COGL_PIPELINE_STATE_CULL_FACE_INDEX,
  COGL_PIPELINE_STATE_UNIFORMS =
    1L<<COGL_PIPELINE_STATE_UNIFORMS_INDEX,
  COGL_PIPELINE_STATE_VERTEX_SNIPPETS =
    1L<<COGL_PIPELINE_STATE_VERTEX_SNIPPETS_INDEX,
  COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS =
    1L<<COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS_INDEX,

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
   COGL_PIPELINE_STATE_USER_SHADER | \
   COGL_PIPELINE_STATE_VERTEX_SNIPPETS | \
   COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS)

#define COGL_PIPELINE_STATE_NEEDS_BIG_STATE \
  (COGL_PIPELINE_STATE_LIGHTING | \
   COGL_PIPELINE_STATE_ALPHA_FUNC | \
   COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE | \
   COGL_PIPELINE_STATE_BLEND | \
   COGL_PIPELINE_STATE_USER_SHADER | \
   COGL_PIPELINE_STATE_DEPTH | \
   COGL_PIPELINE_STATE_FOG | \
   COGL_PIPELINE_STATE_POINT_SIZE | \
   COGL_PIPELINE_STATE_PER_VERTEX_POINT_SIZE | \
   COGL_PIPELINE_STATE_LOGIC_OPS | \
   COGL_PIPELINE_STATE_CULL_FACE | \
   COGL_PIPELINE_STATE_UNIFORMS | \
   COGL_PIPELINE_STATE_VERTEX_SNIPPETS | \
   COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS)

#define COGL_PIPELINE_STATE_MULTI_PROPERTY \
  (COGL_PIPELINE_STATE_LAYERS | \
   COGL_PIPELINE_STATE_LIGHTING | \
   COGL_PIPELINE_STATE_BLEND | \
   COGL_PIPELINE_STATE_DEPTH | \
   COGL_PIPELINE_STATE_FOG | \
   COGL_PIPELINE_STATE_LOGIC_OPS | \
   COGL_PIPELINE_STATE_CULL_FACE | \
   COGL_PIPELINE_STATE_UNIFORMS | \
   COGL_PIPELINE_STATE_VERTEX_SNIPPETS | \
   COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS)

#define COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN \
  (COGL_PIPELINE_STATE_LAYERS | \
   COGL_PIPELINE_STATE_USER_SHADER | \
   COGL_PIPELINE_STATE_PER_VERTEX_POINT_SIZE |  \
   COGL_PIPELINE_STATE_VERTEX_SNIPPETS)

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
  CoglBool        enabled;
  CoglColor       color;
  CoglFogMode     mode;
  float           density;
  float           z_near;
  float           z_far;
} CoglPipelineFogState;

typedef struct
{
  CoglColorMask color_mask;
} CoglPipelineLogicOpsState;

typedef struct
{
  CoglPipelineCullFaceMode mode;
  CoglWinding front_winding;
} CoglPipelineCullFaceState;

typedef struct
{
  CoglBitmask override_mask;

  /* This is an array of values. Only the uniforms that have a bit set
     in override_mask have a corresponding value here. The uniform's
     location is implicit from the order in this array */
  CoglBoxedValue *override_values;

  /* Uniforms that have been modified since this pipeline was last
     flushed */
  CoglBitmask changed_mask;
} CoglPipelineUniformsState;

typedef struct
{
  CoglPipelineLightingState lighting_state;
  CoglPipelineAlphaFuncState alpha_state;
  CoglPipelineBlendState blend_state;
  CoglHandle user_program;
  CoglDepthState depth_state;
  CoglPipelineFogState fog_state;
  float point_size;
  CoglBool per_vertex_point_size;
  CoglPipelineLogicOpsState logic_ops_state;
  CoglPipelineCullFaceState cull_face_state;
  CoglPipelineUniformsState uniforms_state;
  CoglPipelineSnippetList vertex_snippets;
  CoglPipelineSnippetList fragment_snippets;
} CoglPipelineBigState;

typedef struct
{
  CoglPipeline *owner;
  CoglPipelineLayer *layer;
} CoglPipelineLayerCacheEntry;

typedef struct _CoglPipelineHashState
{
  unsigned long layer_differences;
  CoglPipelineEvalFlags flags;
  unsigned int hash;
} CoglPipelineHashState;

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
  CoglNode _parent;

  /* When weak pipelines are destroyed the user is notified via this
   * callback */
  CoglPipelineDestroyCallback destroy_callback;

  /* When notifying that a weak pipeline has been destroyed this
   * private data is passed to the above callback */
  void *destroy_data;

  /* We need to track if a pipeline is referenced in the journal
   * because we can't allow modification to these pipelines without
   * flushing the journal first */
  unsigned int journal_ref_count;

  /* A mask of which sparse state groups are different in this
   * pipeline in comparison to its parent. */
  unsigned int differences;

  /* Whenever a pipeline is modified we increment the age. There's no
   * guarantee that it won't wrap but it can nevertheless be a
   * convenient mechanism to determine when a pipeline has been
   * changed to you can invalidate some some associated cache that
   * depends on the old state. */
  unsigned int age;

  /* This is the primary color of the pipeline.
   *
   * This is a sparse property, ref COGL_PIPELINE_STATE_COLOR */
  CoglColor color;

  /* A pipeline may be made up with multiple layers used to combine
   * textures together.
   *
   * This is sparse state, ref COGL_PIPELINE_STATE_LAYERS */
  unsigned int     n_layers;
  GList	          *layer_differences;

  /* As a basic way to reduce memory usage we divide the pipeline
   * state into two groups; the minimal state modified in 90% of
   * all pipelines and the rest, so that the second group can
   * be allocated dynamically when required... */
  CoglPipelineBigState *big_state;

#ifdef COGL_DEBUG_ENABLED
  /* For debugging purposes it's possible to associate a static const
   * string with a pipeline which can be an aid when trying to trace
   * where the pipeline originates from */
  const char      *static_breadcrumb;
#endif

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

  /* Since the code for deciding if blending really needs to be
   * enabled for a particular pipeline is quite expensive we update
   * the real_blend_enable flag lazily when flushing a pipeline if
   * this dirty flag has been set. */
  unsigned int          dirty_real_blend_enable:1;

  /* Whenever a pipeline is flushed we keep track of whether the
   * pipeline was used with a color attribute where we don't know
   * whether the colors are opaque. The real_blend_enable state
   * depends on this, and must be updated whenever this changes (even
   * if dirty_real_blend_enable isn't set) */
  unsigned int          unknown_color_alpha:1;

  unsigned int          layers_cache_dirty:1;
  unsigned int          deprecated_get_layers_list_dirty:1;

#ifdef COGL_DEBUG_ENABLED
  /* For debugging purposes it's possible to associate a static const
   * string with a pipeline which can be an aid when trying to trace
   * where the pipeline originates from */
  unsigned int          has_static_breadcrumb:1;
#endif

  /* There are multiple fragment and vertex processing backends for
   * CoglPipeline, glsl, arbfp and fixed that are bundled under a
   * "progend". This identifies the backend being used for the
   * pipeline. */
  unsigned int          progend:3;
};

typedef struct _CoglPipelineFragend
{
  void (*start) (CoglPipeline *pipeline,
                 int n_layers,
                 unsigned long pipelines_difference);
  CoglBool (*add_layer) (CoglPipeline *pipeline,
                         CoglPipelineLayer *layer,
                         unsigned long layers_difference);
  CoglBool (*passthrough) (CoglPipeline *pipeline);
  CoglBool (*end) (CoglPipeline *pipeline,
                   unsigned long pipelines_difference);

  void (*pipeline_pre_change_notify) (CoglPipeline *pipeline,
                                      CoglPipelineState change,
                                      const CoglColor *new_color);
  void (*pipeline_set_parent_notify) (CoglPipeline *pipeline);
  void (*layer_pre_change_notify) (CoglPipeline *owner,
                                   CoglPipelineLayer *layer,
                                   CoglPipelineLayerState change);
} CoglPipelineFragend;

typedef struct _CoglPipelineVertend
{
  void (*start) (CoglPipeline *pipeline,
                 int n_layers,
                 unsigned long pipelines_difference);
  CoglBool (*add_layer) (CoglPipeline *pipeline,
                         CoglPipelineLayer *layer,
                         unsigned long layers_difference,
                         CoglFramebuffer *framebuffer);
  CoglBool (*end) (CoglPipeline *pipeline,
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
  int vertend;
  int fragend;
  CoglBool (*start) (CoglPipeline *pipeline);
  void (*end) (CoglPipeline *pipeline,
               unsigned long pipelines_difference);
  void (*pipeline_pre_change_notify) (CoglPipeline *pipeline,
                                      CoglPipelineState change,
                                      const CoglColor *new_color);
  void (*layer_pre_change_notify) (CoglPipeline *owner,
                                   CoglPipelineLayer *layer,
                                   CoglPipelineLayerState change);
  /* This is called after all of the other functions whenever the
     pipeline is flushed, even if the pipeline hasn't changed since
     the last flush */
  void (* pre_paint) (CoglPipeline *pipeline, CoglFramebuffer *framebuffer);
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

static inline CoglPipeline *
_cogl_pipeline_get_parent (CoglPipeline *pipeline)
{
  CoglNode *parent_node = COGL_NODE (pipeline)->parent;
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

typedef CoglBool (*CoglPipelineStateComparitor) (CoglPipeline *authority0,
                                                 CoglPipeline *authority1);

void
_cogl_pipeline_update_authority (CoglPipeline *pipeline,
                                 CoglPipeline *authority,
                                 CoglPipelineState state,
                                 CoglPipelineStateComparitor comparitor);

void
_cogl_pipeline_pre_change_notify (CoglPipeline     *pipeline,
                                  CoglPipelineState change,
                                  const CoglColor  *new_color,
                                  CoglBool          from_layer_change);

void
_cogl_pipeline_prune_redundant_ancestry (CoglPipeline *pipeline);

void
_cogl_pipeline_update_real_blend_enable (CoglPipeline *pipeline,
                                         CoglBool unknown_color_alpha);

typedef enum
{
  COGL_PIPELINE_GET_LAYER_NO_CREATE = 1<<0
} CoglPipelineGetLayerFlags;

CoglPipelineLayer *
_cogl_pipeline_get_layer_with_flags (CoglPipeline *pipeline,
                                     int layer_index,
                                     CoglPipelineGetLayerFlags flags);

#define _cogl_pipeline_get_layer(p, l) \
  _cogl_pipeline_get_layer_with_flags (p, l, 0)

CoglBool
_cogl_is_pipeline_layer (void *object);

void
_cogl_pipeline_prune_empty_layer_difference (CoglPipeline *layers_authority,
                                             CoglPipelineLayer *layer);

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

CoglBool
_cogl_pipeline_get_real_blend_enabled (CoglPipeline *pipeline);

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
 *      a uint32_t mask of the layers that can't be supported with the user
 *      supplied texture and need to be replaced with fallback textures. (1 =
 *      fallback, and the least significant bit = layer 0)
 * @COGL_PIPELINE_FLUSH_DISABLE_MASK: The disable_layers member is set to
 *      a uint32_t mask of the layers that you want to completly disable
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
  CoglPipelineFlushFlag flags;

  uint32_t fallback_layers;
  uint32_t disable_layers;
  CoglTexture *layer0_override_texture;
} CoglPipelineFlushOptions;

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
_cogl_pipeline_set_progend (CoglPipeline *pipeline, int progend);

CoglPipeline *
_cogl_pipeline_get_parent (CoglPipeline *pipeline);

void
_cogl_pipeline_get_colorubv (CoglPipeline *pipeline,
                             uint8_t       *color);

/* XXX: At some point it could be good for this to accept a mask of
 * the state groups we are interested in comparing since we can
 * probably use that information in a number situations to reduce
 * the work we do. */
unsigned long
_cogl_pipeline_compare_differences (CoglPipeline *pipeline0,
                                    CoglPipeline *pipeline1);

CoglBool
_cogl_pipeline_equal (CoglPipeline *pipeline0,
                      CoglPipeline *pipeline1,
                      unsigned int differences,
                      unsigned long layer_differences,
                      CoglPipelineEvalFlags flags);

unsigned int
_cogl_pipeline_hash (CoglPipeline *pipeline,
                     unsigned int differences,
                     unsigned long layer_differences,
                     CoglPipelineEvalFlags flags);

/* Makes a copy of the given pipeline that is a child of the root
 * pipeline rather than a child of the source pipeline. That way the
 * new pipeline won't hold a reference to the source pipeline. The
 * differences specified in @differences and @layer_differences are
 * copied across and all other state is left with the default
 * values. */
CoglPipeline *
_cogl_pipeline_deep_copy (CoglPipeline *pipeline,
                          unsigned long differences,
                          unsigned long layer_differences);

CoglPipeline *
_cogl_pipeline_journal_ref (CoglPipeline *pipeline);

void
_cogl_pipeline_journal_unref (CoglPipeline *pipeline);

const CoglMatrix *
_cogl_pipeline_get_layer_matrix (CoglPipeline *pipeline,
                                 int layer_index);

void
_cogl_pipeline_texture_storage_change_notify (CoglTexture *texture);

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

CoglBool
_cogl_pipeline_get_fog_enabled (CoglPipeline *pipeline);

#ifdef COGL_DEBUG_ENABLED
void
_cogl_pipeline_set_static_breadcrumb (CoglPipeline *pipeline,
                                      const char *breadcrumb);
#endif

unsigned long
_cogl_pipeline_get_age (CoglPipeline *pipeline);

CoglPipeline *
_cogl_pipeline_get_authority (CoglPipeline *pipeline,
                              unsigned long difference);

void
_cogl_pipeline_add_layer_difference (CoglPipeline *pipeline,
                                     CoglPipelineLayer *layer,
                                     CoglBool inc_n_layers);

void
_cogl_pipeline_remove_layer_difference (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        CoglBool dec_n_layers);

CoglPipeline *
_cogl_pipeline_find_equivalent_parent (CoglPipeline *pipeline,
                                       CoglPipelineState pipeline_state,
                                       CoglPipelineLayerState layer_state);

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

typedef CoglBool (*CoglPipelineInternalLayerCallback) (CoglPipelineLayer *layer,
                                                       void *user_data);

void
_cogl_pipeline_foreach_layer_internal (CoglPipeline *pipeline,
                                       CoglPipelineInternalLayerCallback callback,
                                       void *user_data);

CoglBool
_cogl_pipeline_layer_numbers_equal (CoglPipeline *pipeline0,
                                    CoglPipeline *pipeline1);

CoglBool
_cogl_pipeline_layer_and_unit_numbers_equal (CoglPipeline *pipeline0,
                                             CoglPipeline *pipeline1);

CoglBool
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

