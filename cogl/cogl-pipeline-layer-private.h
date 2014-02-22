/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010,2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_PIPELINE_LAYER_PRIVATE_H
#define __COGL_PIPELINE_LAYER_PRIVATE_H

#include "cogl-private.h"
#include "cogl-pipeline.h"
#include "cogl-node-private.h"
#include "cogl-texture.h"
#include "cogl-matrix.h"
#include "cogl-pipeline-layer-state.h"
#include "cogl-pipeline-snippet-private.h"
#include "cogl-sampler-cache-private.h"

#include <glib.h>

typedef struct _CoglPipelineLayer     CoglPipelineLayer;
#define COGL_PIPELINE_LAYER(OBJECT) ((CoglPipelineLayer *)OBJECT)

/* XXX: should I rename these as
 * COGL_PIPELINE_LAYER_STATE_INDEX_XYZ... ?
 */
typedef enum
{
  /* sparse state */
  COGL_PIPELINE_LAYER_STATE_UNIT_INDEX,
  COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX,
  COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX,
  COGL_PIPELINE_LAYER_STATE_SAMPLER_INDEX,
  COGL_PIPELINE_LAYER_STATE_COMBINE_INDEX,
  COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT_INDEX,
  COGL_PIPELINE_LAYER_STATE_USER_MATRIX_INDEX,
  COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX,
  COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX,
  COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX,

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
  COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE =
    1L<<COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX,
  COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA =
    1L<<COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX,
  COGL_PIPELINE_LAYER_STATE_SAMPLER =
    1L<<COGL_PIPELINE_LAYER_STATE_SAMPLER_INDEX,

  COGL_PIPELINE_LAYER_STATE_COMBINE =
    1L<<COGL_PIPELINE_LAYER_STATE_COMBINE_INDEX,
  COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT =
    1L<<COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT_INDEX,
  COGL_PIPELINE_LAYER_STATE_USER_MATRIX =
    1L<<COGL_PIPELINE_LAYER_STATE_USER_MATRIX_INDEX,

  COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS =
    1L<<COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX,

  COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS =
    1L<<COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX,
  COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS =
    1L<<COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX,

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
   COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS | \
   COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS | \
   COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS)

#define COGL_PIPELINE_LAYER_STATE_MULTI_PROPERTY \
  (COGL_PIPELINE_LAYER_STATE_COMBINE | \
   COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS | \
   COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS)

#define COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN \
  COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS

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
  /* Note that these numbers are deliberately not the same as the GL
     numbers so that we can reserve all numbers > TEXTURE0 to store
     very large layer numbers */
  COGL_PIPELINE_COMBINE_SOURCE_TEXTURE,
  COGL_PIPELINE_COMBINE_SOURCE_CONSTANT,
  COGL_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR,
  COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS,
  COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0
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

  CoglPipelineSnippetList vertex_snippets;
  CoglPipelineSnippetList fragment_snippets;

  CoglBool point_sprite_coords;
} CoglPipelineLayerBigState;

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
  CoglNode _parent;

  /* Some layers have a pipeline owner, which is to say that the layer
   * is referenced in that pipelines->layer_differences list.  A layer
   * doesn't always have an owner and may simply be an ancestor for
   * other layers that keeps track of some shared state. */
  CoglPipeline      *owner;

  /* The lowest index is blended first then others on top */
  int	             index;

  /* A mask of which state groups are different in this layer
   * in comparison to its parent. */
  unsigned int       differences;

  /* Common differences
   *
   * As a basic way to reduce memory usage we divide the layer
   * state into two groups; the minimal state modified in 90% of
   * all layers and the rest, so that the second group can
   * be allocated dynamically when required.
   */

  /* Each layer is directly associated with a single texture unit */
  int                        unit_index;

  /* The type of the texture. This is always set even if the texture
     is NULL and it will be used to determine what type of texture
     lookups to use in any shaders generated by the pipeline
     backends. */
  CoglTextureType            texture_type;
  /* The texture for this layer, or NULL for an empty
   * layer */
  CoglTexture               *texture;

  const CoglSamplerCacheEntry *sampler_cache_entry;

  /* Infrequent differences aren't currently tracked in
   * a separate, dynamically allocated structure as they are
   * for pipelines... */
  CoglPipelineLayerBigState *big_state;

  /* bitfields */

  /* Determines if layer->big_state is valid */
  unsigned int          has_big_state:1;

};

typedef CoglBool
(*CoglPipelineLayerStateComparitor) (CoglPipelineLayer *authority0,
                                     CoglPipelineLayer *authority1);



void
_cogl_pipeline_init_default_layers (void);

static inline CoglPipelineLayer *
_cogl_pipeline_layer_get_parent (CoglPipelineLayer *layer)
{
  CoglNode *parent_node = COGL_NODE (layer)->parent;
  return COGL_PIPELINE_LAYER (parent_node);
}

CoglPipelineLayer *
_cogl_pipeline_layer_copy (CoglPipelineLayer *layer);

void
_cogl_pipeline_layer_resolve_authorities (CoglPipelineLayer *layer,
                                          unsigned long differences,
                                          CoglPipelineLayer **authorities);

CoglBool
_cogl_pipeline_layer_equal (CoglPipelineLayer *layer0,
                            CoglPipelineLayer *layer1,
                            unsigned long differences_mask,
                            CoglPipelineEvalFlags flags);

CoglPipelineLayer *
_cogl_pipeline_layer_pre_change_notify (CoglPipeline *required_owner,
                                        CoglPipelineLayer *layer,
                                        CoglPipelineLayerState change);

void
_cogl_pipeline_layer_prune_redundant_ancestry (CoglPipelineLayer *layer);

CoglBool
_cogl_pipeline_layer_has_alpha (CoglPipelineLayer *layer);

CoglBool
_cogl_pipeline_layer_has_user_matrix (CoglPipeline *pipeline,
                                      int layer_index);

/*
 * Calls the pre_paint method on the layer texture if there is
 * one. This will determine whether mipmaps are needed based on the
 * filter settings.
 */
void
_cogl_pipeline_layer_pre_paint (CoglPipelineLayer *layerr);

void
_cogl_pipeline_layer_get_wrap_modes (CoglPipelineLayer *layer,
                                     CoglSamplerCacheWrapMode *wrap_mode_s,
                                     CoglSamplerCacheWrapMode *wrap_mode_t,
                                     CoglSamplerCacheWrapMode *wrap_mode_r);

void
_cogl_pipeline_layer_get_filters (CoglPipelineLayer *layer,
                                  CoglPipelineFilter *min_filter,
                                  CoglPipelineFilter *mag_filter);

const CoglSamplerCacheEntry *
_cogl_pipeline_layer_get_sampler_state (CoglPipelineLayer *layer);

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

CoglTexture *
_cogl_pipeline_layer_get_texture (CoglPipelineLayer *layer);

CoglTexture *
_cogl_pipeline_layer_get_texture_real (CoglPipelineLayer *layer);

CoglTextureType
_cogl_pipeline_layer_get_texture_type (CoglPipelineLayer *layer);

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

void
_cogl_pipeline_layer_copy_differences (CoglPipelineLayer *dest,
                                       CoglPipelineLayer *src,
                                       unsigned long differences);

unsigned long
_cogl_pipeline_layer_compare_differences (CoglPipelineLayer *layer0,
                                          CoglPipelineLayer *layer1);

CoglPipelineLayer *
_cogl_pipeline_layer_get_authority (CoglPipelineLayer *layer,
                                    unsigned long difference);

CoglTexture *
_cogl_pipeline_layer_get_texture (CoglPipelineLayer *layer);

int
_cogl_pipeline_layer_get_unit_index (CoglPipelineLayer *layer);

CoglBool
_cogl_pipeline_layer_needs_combine_separate
                                       (CoglPipelineLayer *combine_authority);

#endif /* __COGL_PIPELINE_LAYER_PRIVATE_H */
