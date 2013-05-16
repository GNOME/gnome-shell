/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010,2013 Intel Corporation.
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

#include "cogl-debug.h"
#include "cogl-context-private.h"
#include "cogl-object.h"

#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-pipeline-layer-state-private.h"
#include "cogl-texture-private.h"
#include "cogl-blend-string.h"
#include "cogl-journal-private.h"
#include "cogl-color-private.h"
#include "cogl-util.h"
#include "cogl-profile.h"
#include "cogl-depth-state-private.h"
#include "cogl1-context.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

static void _cogl_pipeline_free (CoglPipeline *tex);
static void recursively_free_layer_caches (CoglPipeline *pipeline);
static CoglBool _cogl_pipeline_is_weak (CoglPipeline *pipeline);

const CoglPipelineFragend *_cogl_pipeline_fragends[COGL_PIPELINE_N_FRAGENDS];
const CoglPipelineVertend *_cogl_pipeline_vertends[COGL_PIPELINE_N_VERTENDS];
/* The 'MAX' here is so that we don't define an empty array when there
   are no progends */
const CoglPipelineProgend *
_cogl_pipeline_progends[MAX (COGL_PIPELINE_N_PROGENDS, 1)];

#ifdef COGL_PIPELINE_FRAGEND_GLSL
#include "cogl-pipeline-fragend-glsl-private.h"
#endif
#ifdef COGL_PIPELINE_FRAGEND_ARBFP
#include "cogl-pipeline-fragend-arbfp-private.h"
#endif
#ifdef COGL_PIPELINE_FRAGEND_FIXED
#include "cogl-pipeline-fragend-fixed-private.h"
#endif

#ifdef COGL_PIPELINE_VERTEND_GLSL
#include "cogl-pipeline-vertend-glsl-private.h"
#endif
#ifdef COGL_PIPELINE_VERTEND_FIXED
#include "cogl-pipeline-vertend-fixed-private.h"
#endif

#ifdef COGL_PIPELINE_PROGEND_FIXED_ARBFP
#include "cogl-pipeline-progend-fixed-arbfp-private.h"
#endif
#ifdef COGL_PIPELINE_PROGEND_FIXED
#include "cogl-pipeline-progend-fixed-private.h"
#endif
#ifdef COGL_PIPELINE_PROGEND_GLSL
#include "cogl-pipeline-progend-glsl-private.h"
#endif

COGL_OBJECT_DEFINE (Pipeline, pipeline);

/*
 * This initializes the first pipeline owned by the Cogl context. All
 * subsequently instantiated pipelines created via the cogl_pipeline_new()
 * API will initially be a copy of this pipeline.
 *
 * The default pipeline is the topmost ancester for all pipelines.
 */
void
_cogl_pipeline_init_default_pipeline (void)
{
  /* Create new - blank - pipeline */
  CoglPipeline *pipeline = g_slice_new0 (CoglPipeline);
  /* XXX: NB: It's important that we zero this to avoid polluting
   * pipeline hash values with un-initialized data */
  CoglPipelineBigState *big_state = g_slice_new0 (CoglPipelineBigState);
  CoglPipelineLightingState *lighting_state = &big_state->lighting_state;
  CoglPipelineAlphaFuncState *alpha_state = &big_state->alpha_state;
  CoglPipelineBlendState *blend_state = &big_state->blend_state;
  CoglPipelineLogicOpsState *logic_ops_state = &big_state->logic_ops_state;
  CoglPipelineCullFaceState *cull_face_state = &big_state->cull_face_state;
  CoglPipelineUniformsState *uniforms_state = &big_state->uniforms_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Take this opportunity to setup the backends... */
#ifdef COGL_PIPELINE_FRAGEND_GLSL
  _cogl_pipeline_fragends[COGL_PIPELINE_FRAGEND_GLSL] =
    &_cogl_pipeline_glsl_fragend;
#endif
#ifdef COGL_PIPELINE_FRAGEND_ARBFP
  _cogl_pipeline_fragends[COGL_PIPELINE_FRAGEND_ARBFP] =
    &_cogl_pipeline_arbfp_fragend;
#endif
#ifdef COGL_PIPELINE_FRAGEND_FIXED
  _cogl_pipeline_fragends[COGL_PIPELINE_FRAGEND_FIXED] =
    &_cogl_pipeline_fixed_fragend;
#endif
#ifdef COGL_PIPELINE_PROGEND_FIXED
  _cogl_pipeline_progends[COGL_PIPELINE_PROGEND_FIXED_ARBFP] =
    &_cogl_pipeline_fixed_arbfp_progend;
#endif
#ifdef COGL_PIPELINE_PROGEND_FIXED
  _cogl_pipeline_progends[COGL_PIPELINE_PROGEND_FIXED] =
    &_cogl_pipeline_fixed_progend;
#endif
#ifdef COGL_PIPELINE_PROGEND_GLSL
  _cogl_pipeline_progends[COGL_PIPELINE_PROGEND_GLSL] =
    &_cogl_pipeline_glsl_progend;
#endif

#ifdef COGL_PIPELINE_VERTEND_GLSL
  _cogl_pipeline_vertends[COGL_PIPELINE_VERTEND_GLSL] =
    &_cogl_pipeline_glsl_vertend;
#endif
#ifdef COGL_PIPELINE_VERTEND_FIXED
  _cogl_pipeline_vertends[COGL_PIPELINE_VERTEND_FIXED] =
    &_cogl_pipeline_fixed_vertend;
#endif

  _cogl_pipeline_node_init (COGL_NODE (pipeline));

  pipeline->is_weak = FALSE;
  pipeline->journal_ref_count = 0;
  pipeline->progend = COGL_PIPELINE_PROGEND_UNDEFINED;
  pipeline->differences = COGL_PIPELINE_STATE_ALL_SPARSE;

  pipeline->real_blend_enable = FALSE;

  pipeline->blend_enable = COGL_PIPELINE_BLEND_ENABLE_AUTOMATIC;
  pipeline->layer_differences = NULL;
  pipeline->n_layers = 0;

  pipeline->big_state = big_state;
  pipeline->has_big_state = TRUE;

  pipeline->static_breadcrumb = "default pipeline";
  pipeline->has_static_breadcrumb = TRUE;

  pipeline->age = 0;

  /* Use the same defaults as the GL spec... */
  cogl_color_init_from_4ub (&pipeline->color, 0xff, 0xff, 0xff, 0xff);

  /* Use the same defaults as the GL spec... */
  lighting_state->ambient[0] = 0.2;
  lighting_state->ambient[1] = 0.2;
  lighting_state->ambient[2] = 0.2;
  lighting_state->ambient[3] = 1.0;

  lighting_state->diffuse[0] = 0.8;
  lighting_state->diffuse[1] = 0.8;
  lighting_state->diffuse[2] = 0.8;
  lighting_state->diffuse[3] = 1.0;

  lighting_state->specular[0] = 0;
  lighting_state->specular[1] = 0;
  lighting_state->specular[2] = 0;
  lighting_state->specular[3] = 1.0;

  lighting_state->emission[0] = 0;
  lighting_state->emission[1] = 0;
  lighting_state->emission[2] = 0;
  lighting_state->emission[3] = 1.0;

  lighting_state->shininess = 0.0f;

  /* Use the same defaults as the GL spec... */
  alpha_state->alpha_func = COGL_PIPELINE_ALPHA_FUNC_ALWAYS;
  alpha_state->alpha_func_reference = 0.0;

  /* Not the same as the GL default, but seems saner... */
#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)
  blend_state->blend_equation_rgb = GL_FUNC_ADD;
  blend_state->blend_equation_alpha = GL_FUNC_ADD;
  blend_state->blend_src_factor_alpha = GL_ONE;
  blend_state->blend_dst_factor_alpha = GL_ONE_MINUS_SRC_ALPHA;
  cogl_color_init_from_4ub (&blend_state->blend_constant,
                            0x00, 0x00, 0x00, 0x00);
#endif
  blend_state->blend_src_factor_rgb = GL_ONE;
  blend_state->blend_dst_factor_rgb = GL_ONE_MINUS_SRC_ALPHA;

  big_state->user_program = COGL_INVALID_HANDLE;

  cogl_depth_state_init (&big_state->depth_state);

  big_state->point_size = 1.0f;

  logic_ops_state->color_mask = COGL_COLOR_MASK_ALL;

  cull_face_state->mode = COGL_PIPELINE_CULL_FACE_MODE_NONE;
  cull_face_state->front_winding = COGL_WINDING_COUNTER_CLOCKWISE;

  _cogl_bitmask_init (&uniforms_state->override_mask);
  _cogl_bitmask_init (&uniforms_state->changed_mask);
  uniforms_state->override_values = NULL;

  ctx->default_pipeline = _cogl_pipeline_object_new (pipeline);
}

static void
_cogl_pipeline_unparent (CoglNode *pipeline)
{
  /* Chain up */
  _cogl_pipeline_node_unparent_real (pipeline);
}

static CoglBool
recursively_free_layer_caches_cb (CoglNode *node,
                                  void *user_data)
{
  recursively_free_layer_caches (COGL_PIPELINE (node));
  return TRUE;
}

/* This recursively frees the layers_cache of a pipeline and all of
 * its descendants.
 *
 * For instance if we change a pipelines ->layer_differences list
 * then that pipeline and all of its descendants may now have
 * incorrect layer caches. */
static void
recursively_free_layer_caches (CoglPipeline *pipeline)
{
  /* Note: we maintain the invariable that if a pipeline already has a
   * dirty layers_cache then so do all of its descendants. */
  if (pipeline->layers_cache_dirty)
    return;

  if (G_UNLIKELY (pipeline->layers_cache != pipeline->short_layers_cache))
    g_slice_free1 (sizeof (CoglPipelineLayer *) * pipeline->n_layers,
                   pipeline->layers_cache);
  pipeline->layers_cache_dirty = TRUE;

  _cogl_pipeline_node_foreach_child (COGL_NODE (pipeline),
                                     recursively_free_layer_caches_cb,
                                     NULL);
}

static void
_cogl_pipeline_set_parent (CoglPipeline *pipeline,
                           CoglPipeline *parent,
                           CoglBool take_strong_reference)
{
  /* Chain up */
  _cogl_pipeline_node_set_parent_real (COGL_NODE (pipeline),
                                       COGL_NODE (parent),
                                       _cogl_pipeline_unparent,
                                       take_strong_reference);

  /* Since we just changed the ancestry of the pipeline its cache of
   * layers could now be invalid so free it... */
  if (pipeline->differences & COGL_PIPELINE_STATE_LAYERS)
    recursively_free_layer_caches (pipeline);

  /* If the backends are also caching state along with the pipeline
   * that depends on the pipeline's ancestry then it may be notified
   * here...
   */
  if (pipeline->progend != COGL_PIPELINE_PROGEND_UNDEFINED)
    {
      const CoglPipelineProgend *progend =
        _cogl_pipeline_progends[pipeline->progend];
      const CoglPipelineFragend *fragend =
        _cogl_pipeline_fragends[progend->fragend];

      /* Currently only the fragends ever care about reparenting of
       * pipelines... */
      if (fragend->pipeline_set_parent_notify)
        fragend->pipeline_set_parent_notify (pipeline);
    }
}

static void
_cogl_pipeline_promote_weak_ancestors (CoglPipeline *strong)
{
  CoglNode *n;

  _COGL_RETURN_IF_FAIL (!strong->is_weak);

  /* If the parent of strong is weak, then we want to promote it by
     taking a reference on strong's grandparent. We don't need to take
     a reference on strong's direct parent */

  if (COGL_NODE (strong)->parent == NULL)
    return;

  for (n = COGL_NODE (strong)->parent;
       /* We can assume that all weak pipelines have a parent */
       COGL_PIPELINE (n)->is_weak;
       n = n->parent)
    /* 'n' is weak so we take a reference on its parent */
    cogl_object_ref (n->parent);
}

static void
_cogl_pipeline_revert_weak_ancestors (CoglPipeline *strong)
{
  CoglNode *n;

  _COGL_RETURN_IF_FAIL (!strong->is_weak);

  /* This reverts the effect of calling
     _cogl_pipeline_promote_weak_ancestors */

  if (COGL_NODE (strong)->parent == NULL)
    return;

  for (n = COGL_NODE (strong)->parent;
       /* We can assume that all weak pipelines have a parent */
       COGL_PIPELINE (n)->is_weak;
       n = n->parent)
    /* 'n' is weak so we unref its parent */
    cogl_object_unref (n->parent);
}

/* XXX: Always have an eye out for opportunities to lower the cost of
 * cogl_pipeline_copy. */
static CoglPipeline *
_cogl_pipeline_copy (CoglPipeline *src, CoglBool is_weak)
{
  CoglPipeline *pipeline = g_slice_new (CoglPipeline);

  _cogl_pipeline_node_init (COGL_NODE (pipeline));

  pipeline->is_weak = is_weak;

  pipeline->journal_ref_count = 0;

  pipeline->differences = 0;

  pipeline->has_big_state = FALSE;

  /* NB: real_blend_enable isn't a sparse property, it's valid for
   * every pipeline node so we have fast access to it. */
  pipeline->real_blend_enable = src->real_blend_enable;

  /* XXX:
   * consider generalizing the idea of "cached" properties. These
   * would still have an authority like other sparse properties but
   * you wouldn't have to walk up the ancestry to find the authority
   * because the value would be cached directly in each pipeline.
   */

  pipeline->layers_cache_dirty = TRUE;
  pipeline->deprecated_get_layers_list = NULL;
  pipeline->deprecated_get_layers_list_dirty = TRUE;

  pipeline->progend = src->progend;

  pipeline->has_static_breadcrumb = FALSE;

  pipeline->age = 0;

  _cogl_pipeline_set_parent (pipeline, src, !is_weak);

  /* The semantics for copying a weak pipeline are that we promote all
   * weak ancestors to temporarily become strong pipelines until the
   * copy is freed. */
  if (!is_weak)
    _cogl_pipeline_promote_weak_ancestors (pipeline);

  return _cogl_pipeline_object_new (pipeline);
}

CoglPipeline *
cogl_pipeline_copy (CoglPipeline *src)
{
  return _cogl_pipeline_copy (src, FALSE);
}

CoglPipeline *
_cogl_pipeline_weak_copy (CoglPipeline *pipeline,
                          CoglPipelineDestroyCallback callback,
                          void *user_data)
{
  CoglPipeline *copy;
  CoglPipeline *copy_pipeline;

  copy = _cogl_pipeline_copy (pipeline, TRUE);
  copy_pipeline = COGL_PIPELINE (copy);
  copy_pipeline->destroy_callback = callback;
  copy_pipeline->destroy_data = user_data;

  return copy;
}

CoglPipeline *
cogl_pipeline_new (CoglContext *context)
{
  CoglPipeline *new;

  new = cogl_pipeline_copy (context->default_pipeline);
  _cogl_pipeline_set_static_breadcrumb (new, "new");
  return new;
}

static CoglBool
destroy_weak_children_cb (CoglNode *node,
                          void *user_data)
{
  CoglPipeline *pipeline = COGL_PIPELINE (node);

  if (_cogl_pipeline_is_weak (pipeline))
    {
      _cogl_pipeline_node_foreach_child (COGL_NODE (pipeline),
                                         destroy_weak_children_cb,
                                         NULL);

      pipeline->destroy_callback (pipeline, pipeline->destroy_data);
      _cogl_pipeline_unparent (COGL_NODE (pipeline));
    }

  return TRUE;
}

static void
_cogl_pipeline_free (CoglPipeline *pipeline)
{
  if (!pipeline->is_weak)
    _cogl_pipeline_revert_weak_ancestors (pipeline);

  /* Weak pipelines don't take a reference on their parent */
  _cogl_pipeline_node_foreach_child (COGL_NODE (pipeline),
                                     destroy_weak_children_cb,
                                     NULL);

  g_assert (COGL_LIST_EMPTY (&COGL_NODE (pipeline)->children));

  _cogl_pipeline_unparent (COGL_NODE (pipeline));

  if (pipeline->differences & COGL_PIPELINE_STATE_USER_SHADER &&
      pipeline->big_state->user_program)
    cogl_handle_unref (pipeline->big_state->user_program);

  if (pipeline->differences & COGL_PIPELINE_STATE_UNIFORMS)
    {
      CoglPipelineUniformsState *uniforms_state
        = &pipeline->big_state->uniforms_state;
      int n_overrides = _cogl_bitmask_popcount (&uniforms_state->override_mask);
      int i;

      for (i = 0; i < n_overrides; i++)
        _cogl_boxed_value_destroy (uniforms_state->override_values + i);
      g_free (uniforms_state->override_values);

      _cogl_bitmask_destroy (&uniforms_state->override_mask);
      _cogl_bitmask_destroy (&uniforms_state->changed_mask);
    }

  if (pipeline->differences & COGL_PIPELINE_STATE_NEEDS_BIG_STATE)
    g_slice_free (CoglPipelineBigState, pipeline->big_state);

  if (pipeline->differences & COGL_PIPELINE_STATE_LAYERS)
    {
      g_list_foreach (pipeline->layer_differences,
                      (GFunc)cogl_object_unref, NULL);
      g_list_free (pipeline->layer_differences);
    }

  if (pipeline->differences & COGL_PIPELINE_STATE_VERTEX_SNIPPETS)
    _cogl_pipeline_snippet_list_free (&pipeline->big_state->vertex_snippets);

  if (pipeline->differences & COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS)
    _cogl_pipeline_snippet_list_free (&pipeline->big_state->fragment_snippets);

  g_list_free (pipeline->deprecated_get_layers_list);

  recursively_free_layer_caches (pipeline);

  g_slice_free (CoglPipeline, pipeline);
}

CoglBool
_cogl_pipeline_get_real_blend_enabled (CoglPipeline *pipeline)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), FALSE);

  return pipeline->real_blend_enable;
}

static void
_cogl_pipeline_update_layers_cache (CoglPipeline *pipeline)
{
  /* Note: we assume this pipeline is a _LAYERS authority */
  int n_layers;
  CoglPipeline *current;
  int layers_found;

  if (G_LIKELY (!pipeline->layers_cache_dirty) ||
      pipeline->n_layers == 0)
    return;

  pipeline->layers_cache_dirty = FALSE;

  n_layers = pipeline->n_layers;
  if (G_LIKELY (n_layers < G_N_ELEMENTS (pipeline->short_layers_cache)))
    {
      pipeline->layers_cache = pipeline->short_layers_cache;
      memset (pipeline->layers_cache, 0,
              sizeof (CoglPipelineLayer *) *
              G_N_ELEMENTS (pipeline->short_layers_cache));
    }
  else
    {
      pipeline->layers_cache =
        g_slice_alloc0 (sizeof (CoglPipelineLayer *) * n_layers);
    }

  /* Notes:
   *
   * Each pipeline doesn't have to contain a complete list of the layers
   * it depends on, some of them are indirectly referenced through the
   * pipeline's ancestors.
   *
   * pipeline->layer_differences only contains a list of layers that
   * have changed in relation to its parent.
   *
   * pipeline->layer_differences is not maintained sorted, but it
   * won't contain multiple layers corresponding to a particular
   * ->unit_index.
   *
   * Some of the ancestor pipelines may reference layers with
   * ->unit_index values >= n_layers so we ignore them.
   *
   * As we ascend through the ancestors we are searching for any
   * CoglPipelineLayers corresponding to the texture ->unit_index
   * values in the range [0,n_layers-1]. As soon as a pointer is found
   * we ignore layers of further ancestors with the same ->unit_index
   * values.
   */

  layers_found = 0;
  for (current = pipeline;
       _cogl_pipeline_get_parent (current);
       current = _cogl_pipeline_get_parent (current))
    {
      GList *l;

      if (!(current->differences & COGL_PIPELINE_STATE_LAYERS))
        continue;

      for (l = current->layer_differences; l; l = l->next)
        {
          CoglPipelineLayer *layer = l->data;
          int unit_index = _cogl_pipeline_layer_get_unit_index (layer);

          if (unit_index < n_layers && !pipeline->layers_cache[unit_index])
            {
              pipeline->layers_cache[unit_index] = layer;
              layers_found++;
              if (layers_found == n_layers)
                return;
            }
        }
    }

  g_warn_if_reached ();
}

/* XXX: Be carefull when using this API that the callback given doesn't result
 * in the layer cache being invalidated during the iteration! */
void
_cogl_pipeline_foreach_layer_internal (CoglPipeline *pipeline,
                                       CoglPipelineInternalLayerCallback callback,
                                       void *user_data)
{
  CoglPipeline *authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LAYERS);
  int n_layers;
  int i;
  CoglBool cont;

  n_layers = authority->n_layers;
  if (n_layers == 0)
    return;

  _cogl_pipeline_update_layers_cache (authority);

  for (i = 0, cont = TRUE; i < n_layers && cont == TRUE; i++)
    {
      _COGL_RETURN_IF_FAIL (authority->layers_cache_dirty == FALSE);
      cont = callback (authority->layers_cache[i], user_data);
    }
}

CoglBool
_cogl_pipeline_layer_numbers_equal (CoglPipeline *pipeline0,
                                    CoglPipeline *pipeline1)
{
  CoglPipeline *authority0 =
    _cogl_pipeline_get_authority (pipeline0, COGL_PIPELINE_STATE_LAYERS);
  CoglPipeline *authority1 =
    _cogl_pipeline_get_authority (pipeline1, COGL_PIPELINE_STATE_LAYERS);
  int n_layers = authority0->n_layers;
  int i;

  if (authority1->n_layers != n_layers)
    return FALSE;

  _cogl_pipeline_update_layers_cache (authority0);
  _cogl_pipeline_update_layers_cache (authority1);

  for (i = 0; i < n_layers; i++)
    {
      CoglPipelineLayer *layer0 = authority0->layers_cache[i];
      CoglPipelineLayer *layer1 = authority1->layers_cache[i];

      if (layer0->index != layer1->index)
        return FALSE;
    }

  return TRUE;
}

CoglBool
_cogl_pipeline_layer_and_unit_numbers_equal (CoglPipeline *pipeline0,
                                             CoglPipeline *pipeline1)
{
  CoglPipeline *authority0 =
    _cogl_pipeline_get_authority (pipeline0, COGL_PIPELINE_STATE_LAYERS);
  CoglPipeline *authority1 =
    _cogl_pipeline_get_authority (pipeline1, COGL_PIPELINE_STATE_LAYERS);
  int n_layers = authority0->n_layers;
  int i;

  if (authority1->n_layers != n_layers)
    return FALSE;

  _cogl_pipeline_update_layers_cache (authority0);
  _cogl_pipeline_update_layers_cache (authority1);

  for (i = 0; i < n_layers; i++)
    {
      CoglPipelineLayer *layer0 = authority0->layers_cache[i];
      CoglPipelineLayer *layer1 = authority1->layers_cache[i];
      int unit0, unit1;

      if (layer0->index != layer1->index)
        return FALSE;

      unit0 = _cogl_pipeline_layer_get_unit_index (layer0);
      unit1 = _cogl_pipeline_layer_get_unit_index (layer1);
      if (unit0 != unit1)
        return FALSE;
    }

  return TRUE;
}

typedef struct
{
  int i;
  int *indices;
} AppendLayerIndexState;

static CoglBool
append_layer_index_cb (CoglPipelineLayer *layer,
                       void *user_data)
{
  AppendLayerIndexState *state = user_data;
  state->indices[state->i++] = layer->index;
  return TRUE;
}

void
cogl_pipeline_foreach_layer (CoglPipeline *pipeline,
                             CoglPipelineLayerCallback callback,
                             void *user_data)
{
  CoglPipeline *authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LAYERS);
  AppendLayerIndexState state;
  CoglBool cont;
  int i;

  /* XXX: We don't know what the user is going to want to do to the layers
   * but any modification of layers can result in the layer graph changing
   * which could confuse _cogl_pipeline_foreach_layer_internal(). We first
   * get a list of layer indices which will remain valid so long as the
   * user doesn't remove layers. */

  state.i = 0;
  state.indices = g_alloca (authority->n_layers * sizeof (int));

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         append_layer_index_cb,
                                         &state);

  for (i = 0, cont = TRUE; i < authority->n_layers && cont; i++)
    cont = callback (pipeline, state.indices[i], user_data);
}

static CoglBool
layer_has_alpha_cb (CoglPipelineLayer *layer, void *data)
{
  CoglBool *has_alpha = data;
  *has_alpha = _cogl_pipeline_layer_has_alpha (layer);

  /* return FALSE to stop iterating layers if we find any layer
   * has alpha ...
   *
   * FIXME: actually we should never be bailing out because it's
   * always possible that a later layer could discard any previous
   * alpha!
   */

  return !(*has_alpha);
}

/* NB: If this pipeline returns FALSE that doesn't mean that the
 * pipeline is definitely opaque, it just means that that the
 * given changes dont imply transparency.
 *
 * If you want to find out of the pipeline is opaque then assuming
 * this returns FALSE for a set of changes then you can follow
 * up
 */
static CoglBool
_cogl_pipeline_change_implies_transparency (CoglPipeline *pipeline,
                                            unsigned int changes,
                                            const CoglColor *override_color,
                                            CoglBool unknown_color_alpha)
{
  /* In the case of a layer state change we need to check everything
   * else first since they contribute to the has_alpha status of the
   * "PREVIOUS" layer. */
  if (changes & COGL_PIPELINE_STATE_LAYERS)
    changes = COGL_PIPELINE_STATE_AFFECTS_BLENDING;

  if (unknown_color_alpha)
    return TRUE;

  if ((override_color && cogl_color_get_alpha_byte (override_color) != 0xff))
    return TRUE;

  if (changes & COGL_PIPELINE_STATE_COLOR)
    {
      CoglColor tmp;
      cogl_pipeline_get_color (pipeline, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
    }

  if (changes & COGL_PIPELINE_STATE_USER_SHADER)
    {
      /* We can't make any assumptions about the alpha channel if the user
       * is using an unknown fragment shader.
       *
       * TODO: check that it isn't just a vertex shader!
       */
      if (_cogl_pipeline_get_user_program (pipeline) != COGL_INVALID_HANDLE)
        return TRUE;
    }

  if (changes & COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS)
    {
      if (!_cogl_pipeline_has_non_layer_fragment_snippets (pipeline))
        return TRUE;
    }

  if (changes & COGL_PIPELINE_STATE_VERTEX_SNIPPETS)
    {
      if (!_cogl_pipeline_has_non_layer_vertex_snippets (pipeline))
        return TRUE;
    }

  /* XXX: we should only need to look at these if lighting is enabled
   */
  if (changes & COGL_PIPELINE_STATE_LIGHTING)
    {
      /* XXX: This stuff is showing up in sysprof reports which is
       * silly because lighting isn't currently actually supported
       * by Cogl except for these token properties. When we actually
       * expose lighting support we can avoid these checks when
       * lighting is disabled. */
#if 0
      CoglColor tmp;
      cogl_pipeline_get_ambient (pipeline, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
      cogl_pipeline_get_diffuse (pipeline, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
      cogl_pipeline_get_specular (pipeline, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
      cogl_pipeline_get_emission (pipeline, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
#endif
    }

  if (changes & COGL_PIPELINE_STATE_LAYERS)
    {
      /* has_alpha tracks the alpha status of the GL_PREVIOUS layer.
       * To start with that's defined by the pipeline color which
       * must be fully opaque if we got this far. */
      CoglBool has_alpha = FALSE;
      _cogl_pipeline_foreach_layer_internal (pipeline,
                                             layer_has_alpha_cb,
                                             &has_alpha);
      if (has_alpha)
        return TRUE;
    }

  return FALSE;
}

static CoglBool
_cogl_pipeline_needs_blending_enabled (CoglPipeline *pipeline,
                                       unsigned int changes,
                                       const CoglColor *override_color,
                                       CoglBool unknown_color_alpha)
{
  CoglPipeline *enable_authority;
  CoglPipeline *blend_authority;
  CoglPipelineBlendState *blend_state;
  CoglPipelineBlendEnable enabled;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_BLENDING)))
    return FALSE;

  /* We unconditionally check the _BLEND_ENABLE state first because
   * all the other changes are irrelevent if blend_enable != _AUTOMATIC
   */
  enable_authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_BLEND_ENABLE);

  enabled = enable_authority->blend_enable;
  if (enabled != COGL_PIPELINE_BLEND_ENABLE_AUTOMATIC)
    return enabled == COGL_PIPELINE_BLEND_ENABLE_ENABLED ? TRUE : FALSE;

  blend_authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_BLEND);

  blend_state = &blend_authority->big_state->blend_state;

  /* We are trying to identify some cases that are equivalent to
   * blending being disable, where the output is simply GL_SRC_COLOR.
   *
   * Note: we currently only consider a few cases that can be
   * optimized but there could be opportunities to special case more
   * blend functions later.
   */

  /* As the most common way that we currently use to effectively
   * disable blending is to use an equation of
   * "RGBA=ADD(SRC_COLOR, 0)" that's the first thing we check
   * for... */
  if (blend_state->blend_equation_rgb == GL_FUNC_ADD &&
      blend_state->blend_equation_alpha == GL_FUNC_ADD &&
      blend_state->blend_src_factor_alpha == GL_ONE &&
      blend_state->blend_dst_factor_alpha == GL_ZERO)
    {
      return FALSE;
    }

  /* NB: The default blending equation for Cogl is
   * "RGBA=ADD(SRC_COLOR, DST_COLOR * (1-SRC_COLOR[A]))"
   *
   * Next we check if the default blending equation is being used.  If
   * so then we follow that by looking for cases where SRC_COLOR[A] ==
   * 1 since that simplifies "DST_COLOR * (1-SRC_COLOR[A])" to 0 which
   * also effectively requires no blending.
   */

  if (blend_state->blend_equation_rgb != GL_FUNC_ADD ||
      blend_state->blend_equation_alpha != GL_FUNC_ADD)
    return TRUE;

  if (blend_state->blend_src_factor_alpha != GL_ONE ||
      blend_state->blend_dst_factor_alpha != GL_ONE_MINUS_SRC_ALPHA)
    return TRUE;

  if (blend_state->blend_src_factor_rgb != GL_ONE ||
      blend_state->blend_dst_factor_rgb != GL_ONE_MINUS_SRC_ALPHA)
    return TRUE;

  /* Given the above constraints, it's now a case of finding any
   * SRC_ALPHA that != 1 */

  if (_cogl_pipeline_change_implies_transparency (pipeline, changes,
                                                  override_color,
                                                  unknown_color_alpha))
    return TRUE;

  /* At this point, considering just the state that has changed it
   * looks like blending isn't needed. If blending was previously
   * enabled though it could be that some other state still requires
   * that we have blending enabled because it implies transparency.
   * In this case we still need to go and check the other state...
   *
   * XXX: We could explicitly keep track of the mask of state groups
   * that are currently causing blending to be enabled so that we
   * never have to resort to checking *all* the state and can instead
   * always limit the check to those in the mask.
   */
  if (pipeline->real_blend_enable)
    {
      unsigned int other_state =
        COGL_PIPELINE_STATE_AFFECTS_BLENDING & ~changes;
      if (other_state &&
          _cogl_pipeline_change_implies_transparency (pipeline, other_state, NULL, FALSE))
        return TRUE;
    }

  return FALSE;
}

void
_cogl_pipeline_set_progend (CoglPipeline *pipeline, int progend)
{
  pipeline->progend = progend;
}

static void
_cogl_pipeline_copy_differences (CoglPipeline *dest,
                                 CoglPipeline *src,
                                 unsigned long differences)
{
  CoglPipelineBigState *big_state;

  if (differences & COGL_PIPELINE_STATE_COLOR)
    dest->color = src->color;

  if (differences & COGL_PIPELINE_STATE_BLEND_ENABLE)
    dest->blend_enable = src->blend_enable;

  if (differences & COGL_PIPELINE_STATE_LAYERS)
    {
      GList *l;

      if (dest->differences & COGL_PIPELINE_STATE_LAYERS &&
          dest->layer_differences)
        {
          g_list_foreach (dest->layer_differences,
                          (GFunc)cogl_object_unref,
                          NULL);
          g_list_free (dest->layer_differences);
        }

      for (l = src->layer_differences; l; l = l->next)
        {
          /* NB: a layer can't have more than one ->owner so we can't
           * simply take a references on each of the original
           * layer_differences, we have to derive new layers from the
           * originals instead. */
          CoglPipelineLayer *copy = _cogl_pipeline_layer_copy (l->data);
          _cogl_pipeline_add_layer_difference (dest, copy, FALSE);
          cogl_object_unref (copy);
        }

      /* Note: we initialize n_layers after adding the layer differences
       * since the act of adding the layers will initialize n_layers to 0
       * because dest isn't initially a STATE_LAYERS authority. */
      dest->n_layers = src->n_layers;
    }

  if (differences & COGL_PIPELINE_STATE_NEEDS_BIG_STATE)
    {
      if (!dest->has_big_state)
        {
          dest->big_state = g_slice_new (CoglPipelineBigState);
          dest->has_big_state = TRUE;
        }
      big_state = dest->big_state;
    }
  else
    goto check_for_blending_change;

  if (differences & COGL_PIPELINE_STATE_LIGHTING)
    {
      memcpy (&big_state->lighting_state,
              &src->big_state->lighting_state,
              sizeof (CoglPipelineLightingState));
    }

  if (differences & COGL_PIPELINE_STATE_ALPHA_FUNC)
    big_state->alpha_state.alpha_func =
      src->big_state->alpha_state.alpha_func;

  if (differences & COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE)
    big_state->alpha_state.alpha_func_reference =
      src->big_state->alpha_state.alpha_func_reference;

  if (differences & COGL_PIPELINE_STATE_BLEND)
    {
      memcpy (&big_state->blend_state,
              &src->big_state->blend_state,
              sizeof (CoglPipelineBlendState));
    }

  if (differences & COGL_PIPELINE_STATE_USER_SHADER)
    {
      if (src->big_state->user_program)
        big_state->user_program =
          cogl_handle_ref (src->big_state->user_program);
      else
        big_state->user_program = COGL_INVALID_HANDLE;
    }

  if (differences & COGL_PIPELINE_STATE_DEPTH)
    {
      memcpy (&big_state->depth_state,
              &src->big_state->depth_state,
              sizeof (CoglDepthState));
    }

  if (differences & COGL_PIPELINE_STATE_FOG)
    {
      memcpy (&big_state->fog_state,
              &src->big_state->fog_state,
              sizeof (CoglPipelineFogState));
    }

  if (differences & COGL_PIPELINE_STATE_POINT_SIZE)
    big_state->point_size = src->big_state->point_size;

  if (differences & COGL_PIPELINE_STATE_LOGIC_OPS)
    {
      memcpy (&big_state->logic_ops_state,
              &src->big_state->logic_ops_state,
              sizeof (CoglPipelineLogicOpsState));
    }

  if (differences & COGL_PIPELINE_STATE_CULL_FACE)
    {
      memcpy (&big_state->cull_face_state,
              &src->big_state->cull_face_state,
              sizeof (CoglPipelineCullFaceState));
    }

  if (differences & COGL_PIPELINE_STATE_UNIFORMS)
    {
      int n_overrides =
        _cogl_bitmask_popcount (&src->big_state->uniforms_state.override_mask);
      int i;

      big_state->uniforms_state.override_values =
        g_malloc (n_overrides * sizeof (CoglBoxedValue));

      for (i = 0; i < n_overrides; i++)
        {
          CoglBoxedValue *dst_bv =
            big_state->uniforms_state.override_values + i;
          const CoglBoxedValue *src_bv =
            src->big_state->uniforms_state.override_values + i;

          _cogl_boxed_value_copy (dst_bv, src_bv);
        }

      _cogl_bitmask_init (&big_state->uniforms_state.override_mask);
      _cogl_bitmask_set_bits (&big_state->uniforms_state.override_mask,
                              &src->big_state->uniforms_state.override_mask);

      _cogl_bitmask_init (&big_state->uniforms_state.changed_mask);
    }

  if (differences & COGL_PIPELINE_STATE_VERTEX_SNIPPETS)
    _cogl_pipeline_snippet_list_copy (&big_state->vertex_snippets,
                                      &src->big_state->vertex_snippets);

  if (differences & COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS)
    _cogl_pipeline_snippet_list_copy (&big_state->fragment_snippets,
                                      &src->big_state->fragment_snippets);

  /* XXX: we shouldn't bother doing this in most cases since
   * _copy_differences is typically used to initialize pipeline state
   * by copying it from the current authority, so it's not actually
   * *changing* anything.
   */
check_for_blending_change:
  if (differences & COGL_PIPELINE_STATE_AFFECTS_BLENDING)
    dest->dirty_real_blend_enable = TRUE;

  dest->differences |= differences;
}

static void
_cogl_pipeline_init_multi_property_sparse_state (CoglPipeline *pipeline,
                                                 CoglPipelineState change)
{
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (change & COGL_PIPELINE_STATE_ALL_SPARSE);

  if (!(change & COGL_PIPELINE_STATE_MULTI_PROPERTY))
    return;

  authority = _cogl_pipeline_get_authority (pipeline, change);

  switch (change)
    {
    /* XXX: avoid using a default: label so we get a warning if we
     * don't explicitly handle a newly defined state-group here. */
    case COGL_PIPELINE_STATE_COLOR:
    case COGL_PIPELINE_STATE_BLEND_ENABLE:
    case COGL_PIPELINE_STATE_ALPHA_FUNC:
    case COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE:
    case COGL_PIPELINE_STATE_POINT_SIZE:
    case COGL_PIPELINE_STATE_USER_SHADER:
    case COGL_PIPELINE_STATE_REAL_BLEND_ENABLE:
      g_return_if_reached ();

    case COGL_PIPELINE_STATE_LAYERS:
      pipeline->n_layers = authority->n_layers;
      pipeline->layer_differences = NULL;
      break;
    case COGL_PIPELINE_STATE_LIGHTING:
      {
        memcpy (&pipeline->big_state->lighting_state,
                &authority->big_state->lighting_state,
                sizeof (CoglPipelineLightingState));
        break;
      }
    case COGL_PIPELINE_STATE_BLEND:
      {
        memcpy (&pipeline->big_state->blend_state,
                &authority->big_state->blend_state,
                sizeof (CoglPipelineBlendState));
        break;
      }
    case COGL_PIPELINE_STATE_DEPTH:
      {
        memcpy (&pipeline->big_state->depth_state,
                &authority->big_state->depth_state,
                sizeof (CoglDepthState));
        break;
      }
    case COGL_PIPELINE_STATE_FOG:
      {
        memcpy (&pipeline->big_state->fog_state,
                &authority->big_state->fog_state,
                sizeof (CoglPipelineFogState));
        break;
      }
    case COGL_PIPELINE_STATE_LOGIC_OPS:
      {
        memcpy (&pipeline->big_state->logic_ops_state,
                &authority->big_state->logic_ops_state,
                sizeof (CoglPipelineLogicOpsState));
        break;
      }
    case COGL_PIPELINE_STATE_CULL_FACE:
      {
        memcpy (&pipeline->big_state->cull_face_state,
                &authority->big_state->cull_face_state,
                sizeof (CoglPipelineCullFaceState));
        break;
      }
    case COGL_PIPELINE_STATE_UNIFORMS:
      {
        CoglPipelineUniformsState *uniforms_state =
          &pipeline->big_state->uniforms_state;
        _cogl_bitmask_init (&uniforms_state->override_mask);
        _cogl_bitmask_init (&uniforms_state->changed_mask);
        uniforms_state->override_values = NULL;
        break;
      }
    case COGL_PIPELINE_STATE_VERTEX_SNIPPETS:
      _cogl_pipeline_snippet_list_copy (&pipeline->big_state->vertex_snippets,
                                        &authority->big_state->vertex_snippets);
      break;

    case COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS:
      _cogl_pipeline_snippet_list_copy (&pipeline->big_state->fragment_snippets,
                                        &authority->big_state->
                                        fragment_snippets);
      break;
    }
}

static CoglBool
check_if_strong_cb (CoglNode *node, void *user_data)
{
  CoglPipeline *pipeline = COGL_PIPELINE (node);
  CoglBool *has_strong_child = user_data;

  if (!_cogl_pipeline_is_weak (pipeline))
    {
      *has_strong_child = TRUE;
      return FALSE;
    }

  return TRUE;
}

static CoglBool
has_strong_children (CoglPipeline *pipeline)
{
  CoglBool has_strong_child = FALSE;
  _cogl_pipeline_node_foreach_child (COGL_NODE (pipeline),
                                     check_if_strong_cb,
                                     &has_strong_child);
  return has_strong_child;
}

static CoglBool
_cogl_pipeline_is_weak (CoglPipeline *pipeline)
{
  if (pipeline->is_weak && !has_strong_children (pipeline))
    return TRUE;
  else
    return FALSE;
}

static CoglBool
reparent_children_cb (CoglNode *node,
                      void *user_data)
{
  CoglPipeline *pipeline = COGL_PIPELINE (node);
  CoglPipeline *parent = user_data;

  _cogl_pipeline_set_parent (pipeline, parent, TRUE);

  return TRUE;
}

void
_cogl_pipeline_pre_change_notify (CoglPipeline     *pipeline,
                                  CoglPipelineState change,
                                  const CoglColor  *new_color,
                                  CoglBool          from_layer_change)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If primitives have been logged in the journal referencing the
   * current state of this pipeline we need to flush the journal
   * before we can modify it... */
  if (pipeline->journal_ref_count)
    {
      CoglBool skip_journal_flush = FALSE;

      /* XXX: We don't usually need to flush the journal just due to
       * color changes since pipeline colors are logged in the
       * journal's vertex buffer. The exception is when the change in
       * color enables or disables the need for blending. */
      if (change == COGL_PIPELINE_STATE_COLOR)
        {
          CoglBool will_need_blending =
            _cogl_pipeline_needs_blending_enabled (pipeline,
                                                   change,
                                                   new_color,
                                                   FALSE);
          CoglBool blend_enable = pipeline->real_blend_enable ? TRUE : FALSE;

          if (will_need_blending == blend_enable)
            skip_journal_flush = TRUE;
        }

      if (!skip_journal_flush)
        {
          /* XXX: note we use cogl_flush() not _cogl_flush_journal() so
           * we will flush *all* known journals that might reference the
           * current pipeline. */
          cogl_flush ();
        }
    }

  /* XXX:
   * To simplify things for the vertex, fragment and program backends
   * we are careful about how we report STATE_LAYERS changes.
   *
   * All STATE_LAYERS change notification with the exception of
   * ->n_layers will also result in layer_pre_change_notifications.
   * For backends that perform code generation for fragment processing
   * they typically need to understand the details of how layers get
   * changed to determine if they need to repeat codegen.  It doesn't
   * help them to report a pipeline STATE_LAYERS change for all layer
   * changes since it's so broad, they really need to wait for the
   * specific layer change to be notified.  What does help though is
   * to report a STATE_LAYERS change for a change in ->n_layers
   * because they typically do need to repeat codegen in that case.
   *
   * Here we ensure that change notifications against a pipeline or
   * against a layer are mutually exclusive as far as fragment, vertex
   * and program backends are concerned.
   */
  if (!from_layer_change &&
      pipeline->progend != COGL_PIPELINE_PROGEND_UNDEFINED)
    {
      const CoglPipelineProgend *progend =
        _cogl_pipeline_progends[pipeline->progend];
      const CoglPipelineVertend *vertend =
        _cogl_pipeline_vertends[progend->vertend];
      const CoglPipelineFragend *fragend =
        _cogl_pipeline_fragends[progend->fragend];

      if (vertend->pipeline_pre_change_notify)
        vertend->pipeline_pre_change_notify (pipeline, change, new_color);

      if (fragend->pipeline_pre_change_notify)
        fragend->pipeline_pre_change_notify (pipeline, change, new_color);

      if (progend->pipeline_pre_change_notify)
        progend->pipeline_pre_change_notify (pipeline, change, new_color);
    }

  /* There may be an arbitrary tree of descendants of this pipeline;
   * any of which may indirectly depend on this pipeline as the
   * authority for some set of properties. (Meaning for example that
   * one of its descendants derives its color or blending state from
   * this pipeline.)
   *
   * We can't modify any property that this pipeline is the authority
   * for unless we create another pipeline to take its place first and
   * make sure descendants reference this new pipeline instead.
   */

  /* The simplest descendants to handle are weak pipelines; we simply
   * destroy them if we are modifying a pipeline they depend on. This
   * means weak pipelines never cause us to do a copy-on-write. */
  _cogl_pipeline_node_foreach_child (COGL_NODE (pipeline),
                                     destroy_weak_children_cb,
                                     NULL);

  /* If there are still children remaining though we'll need to
   * perform a copy-on-write and reparent the dependants as children
   * of the copy. */
  if (!COGL_LIST_EMPTY (&COGL_NODE (pipeline)->children))
    {
      CoglPipeline *new_authority;

      COGL_STATIC_COUNTER (pipeline_copy_on_write_counter,
                           "pipeline copy on write counter",
                           "Increments each time a pipeline "
                           "must be copied to allow modification",
                           0 /* no application private data */);

      COGL_COUNTER_INC (_cogl_uprof_context, pipeline_copy_on_write_counter);

      new_authority =
        cogl_pipeline_copy (_cogl_pipeline_get_parent (pipeline));
      _cogl_pipeline_set_static_breadcrumb (new_authority,
                                            "pre_change_notify:copy-on-write");

      /* We could explicitly walk the descendants, OR together the set
       * of differences that we determine this pipeline is the
       * authority on and only copy those differences copied across.
       *
       * Or, if we don't explicitly walk the descendants we at least
       * know that pipeline->differences represents the largest set of
       * differences that this pipeline could possibly be an authority
       * on.
       *
       * We do the later just because it's simplest, but we might need
       * to come back to this later...
       */
      _cogl_pipeline_copy_differences (new_authority, pipeline,
                                       pipeline->differences);

      /* Reparent the dependants of pipeline to be children of
       * new_authority instead... */
      _cogl_pipeline_node_foreach_child (COGL_NODE (pipeline),
                                         reparent_children_cb,
                                         new_authority);

      /* The children will keep the new authority alive so drop the
       * reference we got when copying... */
      cogl_object_unref (new_authority);
    }

  /* At this point we know we have a pipeline with no strong
   * dependants (though we may have some weak children) so we are now
   * free to modify the pipeline. */

  pipeline->age++;

  if (change & COGL_PIPELINE_STATE_NEEDS_BIG_STATE &&
      !pipeline->has_big_state)
    {
      pipeline->big_state = g_slice_new (CoglPipelineBigState);
      pipeline->has_big_state = TRUE;
    }

  /* Note: conceptually we have just been notified that a single
   * property value is about to change, but since some state-groups
   * contain multiple properties and 'pipeline' is about to take over
   * being the authority for the property's corresponding state-group
   * we need to maintain the integrity of the other property values
   * too.
   *
   * To ensure this we handle multi-property state-groups by copying
   * all the values from the old-authority to the new...
   *
   * We don't have to worry about non-sparse property groups since
   * we never take over being an authority for such properties so
   * they automatically maintain integrity.
   */
  if (change & COGL_PIPELINE_STATE_ALL_SPARSE &&
      !(pipeline->differences & change))
    {
      _cogl_pipeline_init_multi_property_sparse_state (pipeline, change);
      pipeline->differences |= change;
    }

  /* Each pipeline has a sorted cache of the layers it depends on
   * which will need updating via _cogl_pipeline_update_layers_cache
   * if a pipeline's layers are changed. */
  if (change == COGL_PIPELINE_STATE_LAYERS)
    recursively_free_layer_caches (pipeline);

  /* If the pipeline being changed is the same as the last pipeline we
   * flushed then we keep a track of the changes so we can try to
   * minimize redundant OpenGL calls if the same pipeline is flushed
   * again.
   */
  if (ctx->current_pipeline == pipeline)
    ctx->current_pipeline_changes_since_flush |= change;
}


void
_cogl_pipeline_add_layer_difference (CoglPipeline *pipeline,
                                     CoglPipelineLayer *layer,
                                     CoglBool inc_n_layers)
{
  _COGL_RETURN_IF_FAIL (layer->owner == NULL);

  layer->owner = pipeline;
  cogl_object_ref (layer);

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  /* Note: the last argument to _cogl_pipeline_pre_change_notify is
   * needed to differentiate STATE_LAYER changes which don't affect
   * the number of layers from those that do. NB: Layer change
   * notifications that don't change the number of layers don't get
   * forwarded to the fragend. */
  _cogl_pipeline_pre_change_notify (pipeline,
                                    COGL_PIPELINE_STATE_LAYERS,
                                    NULL,
                                    !inc_n_layers);

  pipeline->differences |= COGL_PIPELINE_STATE_LAYERS;

  pipeline->layer_differences =
    g_list_prepend (pipeline->layer_differences, layer);

  if (inc_n_layers)
    pipeline->n_layers++;

  /* Adding a layer difference may mean this pipeline now overrides
   * all of the layers of its parent which might make the parent
   * redundant so we should try to prune the hierarchy */
  _cogl_pipeline_prune_redundant_ancestry (pipeline);
}

void
_cogl_pipeline_remove_layer_difference (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        CoglBool dec_n_layers)
{
  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  /* Note: the last argument to _cogl_pipeline_pre_change_notify is
   * needed to differentiate STATE_LAYER changes which don't affect
   * the number of layers from those that do. NB: Layer change
   * notifications that don't change the number of layers don't get
   * forwarded to the fragend. */
  _cogl_pipeline_pre_change_notify (pipeline,
                                    COGL_PIPELINE_STATE_LAYERS,
                                    NULL,
                                    !dec_n_layers);

  /* We only need to remove the layer difference if the pipeline is
   * currently the owner. If it is not the owner then one of two
   * things will happen to make sure this layer is replaced. If it is
   * the last layer being removed then decrementing n_layers will
   * ensure that the last layer is skipped. If it is any other layer
   * then the subsequent layers will have been shifted down and cause
   * it be replaced */
  if (layer->owner == pipeline)
    {
      layer->owner = NULL;
      cogl_object_unref (layer);

      pipeline->layer_differences =
        g_list_remove (pipeline->layer_differences, layer);
    }

  pipeline->differences |= COGL_PIPELINE_STATE_LAYERS;

  if (dec_n_layers)
    pipeline->n_layers--;
}

static void
_cogl_pipeline_try_reverting_layers_authority (CoglPipeline *authority,
                                               CoglPipeline *old_authority)
{
  if (authority->layer_differences == NULL &&
      _cogl_pipeline_get_parent (authority))
    {
      /* If the previous _STATE_LAYERS authority has the same
       * ->n_layers then we can revert to that being the authority
       *  again. */
      if (!old_authority)
        {
          old_authority =
            _cogl_pipeline_get_authority (_cogl_pipeline_get_parent (authority),
                                          COGL_PIPELINE_STATE_LAYERS);
        }

      if (old_authority->n_layers == authority->n_layers)
        authority->differences &= ~COGL_PIPELINE_STATE_LAYERS;
    }
}

void
_cogl_pipeline_update_real_blend_enable (CoglPipeline *pipeline,
                                         CoglBool unknown_color_alpha)
{
  CoglPipeline *parent;
  unsigned int differences;

  if (pipeline->dirty_real_blend_enable == FALSE &&
      pipeline->unknown_color_alpha == unknown_color_alpha)
    return;

  if (pipeline->dirty_real_blend_enable)
    {
      differences = pipeline->differences;

      parent = _cogl_pipeline_get_parent (pipeline);
      while (parent->dirty_real_blend_enable)
        {
          differences |= parent->differences;
          parent = _cogl_pipeline_get_parent (parent);
        }

      /* We initialize the pipeline's real_blend_enable with a known
       * reference value from its nearest ancestor with clean state so
       * we can then potentially reduce the work involved in checking
       * if the pipeline really needs blending itself because we can
       * just look at the things that differ between the ancestor and
       * this pipeline.
       */
      pipeline->real_blend_enable = parent->real_blend_enable;
    }
  else /* pipeline->unknown_color_alpha != unknown_color_alpha */
    differences = 0;

  /* Note we don't call _cogl_pipeline_pre_change_notify() for this
   * state change because ->real_blend_enable is lazily derived from
   * other state while flushing the pipeline and we'd need to avoid
   * recursion problems in cases where _pre_change_notify() flushes
   * the journal if the pipeline is referenced by a journal.
   */
  pipeline->real_blend_enable =
    _cogl_pipeline_needs_blending_enabled (pipeline, differences,
                                           NULL, unknown_color_alpha);
  pipeline->dirty_real_blend_enable = FALSE;
  pipeline->unknown_color_alpha = unknown_color_alpha;
}

typedef struct
{
  int keep_n;
  int current_pos;
  int first_index_to_prune;
} CoglPipelinePruneLayersInfo;

static CoglBool
update_prune_layers_info_cb (CoglPipelineLayer *layer, void *user_data)
{
  CoglPipelinePruneLayersInfo *state = user_data;

  if (state->current_pos == state->keep_n)
    {
      state->first_index_to_prune = layer->index;
      return FALSE;
    }
  state->current_pos++;
  return TRUE;
}

void
_cogl_pipeline_prune_to_n_layers (CoglPipeline *pipeline, int n)
{
  CoglPipeline *authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LAYERS);
  CoglPipelinePruneLayersInfo state;
  GList *l;
  GList *next;

  if (authority->n_layers <= n)
    return;

  /* This call to foreach_layer_internal needs to be done before
   * calling pre_change_notify because it recreates the layer cache.
   * We are relying on pre_change_notify to clear the layer cache
   * before we change the number of layers */
  state.keep_n = n;
  state.current_pos = 0;
  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         update_prune_layers_info_cb,
                                         &state);

  _cogl_pipeline_pre_change_notify (pipeline,
                                    COGL_PIPELINE_STATE_LAYERS,
                                    NULL,
                                    FALSE);

  pipeline->differences |= COGL_PIPELINE_STATE_LAYERS;
  pipeline->n_layers = n;

  /* It's possible that this pipeline owns some of the layers being
   * discarded, so we'll need to unlink them... */
  for (l = pipeline->layer_differences; l; l = next)
    {
      CoglPipelineLayer *layer = l->data;
      next = l->next; /* we're modifying the list we're iterating */

      if (layer->index >= state.first_index_to_prune)
        _cogl_pipeline_remove_layer_difference (pipeline, layer, FALSE);
    }

  pipeline->differences |= COGL_PIPELINE_STATE_LAYERS;
}

typedef struct
{
  /* The layer we are trying to find */
  int                         layer_index;

  /* The layer we find or untouched if not found */
  CoglPipelineLayer          *layer;

  /* If the layer can't be found then a new layer should be
   * inserted after this texture unit index... */
  int                         insert_after;

  /* When adding a layer we need the list of layers to shift up
   * to a new texture unit. When removing we need the list of
   * layers to shift down.
   *
   * Note: the list isn't sorted */
  CoglPipelineLayer         **layers_to_shift;
  int                         n_layers_to_shift;

  /* When adding a layer we don't need a complete list of
   * layers_to_shift if we find a layer already corresponding to the
   * layer_index.  */
  CoglBool                    ignore_shift_layers_if_found;

} CoglPipelineLayerInfo;

/* Returns TRUE once we know there is nothing more to update */
static CoglBool
update_layer_info (CoglPipelineLayer *layer,
                   CoglPipelineLayerInfo *layer_info)
{
  if (layer->index == layer_info->layer_index)
    {
      layer_info->layer = layer;
      if (layer_info->ignore_shift_layers_if_found)
        return TRUE;
    }
  else if (layer->index < layer_info->layer_index)
    {
      int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
      layer_info->insert_after = unit_index;
    }
  else
    layer_info->layers_to_shift[layer_info->n_layers_to_shift++] =
      layer;

  return FALSE;
}

/* Returns FALSE to break out of a _foreach_layer () iteration */
static CoglBool
update_layer_info_cb (CoglPipelineLayer *layer,
                      void *user_data)
{
  CoglPipelineLayerInfo *layer_info = user_data;

  if (update_layer_info (layer, layer_info))
    return FALSE; /* break */
  else
    return TRUE; /* continue */
}

static void
_cogl_pipeline_get_layer_info (CoglPipeline *pipeline,
                               CoglPipelineLayerInfo *layer_info)
{
  /* Note: we are assuming this pipeline is a _STATE_LAYERS authority */
  int n_layers = pipeline->n_layers;
  int i;

  /* FIXME: _cogl_pipeline_foreach_layer_internal now calls
   * _cogl_pipeline_update_layers_cache anyway so this codepath is
   * pointless! */
  if (layer_info->ignore_shift_layers_if_found &&
      pipeline->layers_cache_dirty)
    {
      /* The expectation is that callers of
       * _cogl_pipeline_get_layer_info are likely to be modifying the
       * list of layers associated with a pipeline so in this case
       * where we don't have a cache of the layers and we don't
       * necessarily have to iterate all the layers of the pipeline we
       * use a foreach_layer callback instead of updating the cache
       * and iterating that as below. */
      _cogl_pipeline_foreach_layer_internal (pipeline,
                                             update_layer_info_cb,
                                             layer_info);
      return;
    }

  _cogl_pipeline_update_layers_cache (pipeline);
  for (i = 0; i < n_layers; i++)
    {
      CoglPipelineLayer *layer = pipeline->layers_cache[i];

      if (update_layer_info (layer, layer_info))
        return;
    }
}

CoglPipelineLayer *
_cogl_pipeline_get_layer_with_flags (CoglPipeline *pipeline,
                                     int layer_index,
                                     CoglPipelineGetLayerFlags flags)
{
  CoglPipeline *authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LAYERS);
  CoglPipelineLayerInfo layer_info;
  CoglPipelineLayer *layer;
  int unit_index;
  int i;
  CoglContext *ctx;

  /* The layer index of the layer we want info about */
  layer_info.layer_index = layer_index;

  /* If a layer already exists with the given index this will be
   * updated. */
  layer_info.layer = NULL;

  /* If a layer isn't found for the given index we'll need to know
   * where to insert a new layer. */
  layer_info.insert_after = -1;

  /* If a layer can't be found then we'll need to insert a new layer
   * and bump up the texture unit for all layers with an index
   * > layer_index. */
  layer_info.layers_to_shift =
    g_alloca (sizeof (CoglPipelineLayer *) * authority->n_layers);
  layer_info.n_layers_to_shift = 0;

  /* If an exact match is found though we don't need a complete
   * list of layers with indices > layer_index... */
  layer_info.ignore_shift_layers_if_found = TRUE;

  _cogl_pipeline_get_layer_info (authority, &layer_info);

  if (layer_info.layer || (flags & COGL_PIPELINE_GET_LAYER_NO_CREATE))
    return layer_info.layer;

  ctx = _cogl_context_get_default ();

  unit_index = layer_info.insert_after + 1;
  if (unit_index == 0)
    layer = _cogl_pipeline_layer_copy (ctx->default_layer_0);
  else
    {
      CoglPipelineLayer *new;
      layer = _cogl_pipeline_layer_copy (ctx->default_layer_n);
      new = _cogl_pipeline_set_layer_unit (NULL, layer, unit_index);
      /* Since we passed a newly allocated layer we wouldn't expect
       * _set_layer_unit() to have to allocate *another* layer. */
      g_assert (new == layer);
    }
  layer->index = layer_index;

  for (i = 0; i < layer_info.n_layers_to_shift; i++)
    {
      CoglPipelineLayer *shift_layer = layer_info.layers_to_shift[i];

      unit_index = _cogl_pipeline_layer_get_unit_index (shift_layer);
      _cogl_pipeline_set_layer_unit (pipeline, shift_layer, unit_index + 1);
      /* NB: shift_layer may not be writeable so _set_layer_unit()
       * will allocate a derived layer internally which will become
       * owned by pipeline. Check the return value if we need to do
       * anything else with this layer. */
    }

  _cogl_pipeline_add_layer_difference (pipeline, layer, TRUE);

  cogl_object_unref (layer);

  return layer;
}

void
_cogl_pipeline_prune_empty_layer_difference (CoglPipeline *layers_authority,
                                             CoglPipelineLayer *layer)
{
  /* Find the GList link that references the empty layer */
  GList *link = g_list_find (layers_authority->layer_differences, layer);
  /* No pipeline directly owns the root node layer so this is safe... */
  CoglPipelineLayer *layer_parent = _cogl_pipeline_layer_get_parent (layer);
  CoglPipelineLayerInfo layer_info;
  CoglPipeline *old_layers_authority;

  _COGL_RETURN_IF_FAIL (link != NULL);

  /* If the layer's parent doesn't have an owner then we can simply
   * take ownership ourselves and drop our reference on the empty
   * layer. We don't want to take ownership of the root node layer so
   * we also need to verify that the parent has a parent
   */
  if (layer_parent->index == layer->index && layer_parent->owner == NULL &&
      _cogl_pipeline_layer_get_parent (layer_parent) != NULL)
    {
      cogl_object_ref (layer_parent);
      layer_parent->owner = layers_authority;
      link->data = layer_parent;
      cogl_object_unref (layer);
      recursively_free_layer_caches (layers_authority);
      return;
    }

  /* Now we want to find the layer that would become the authority for
   * layer->index if we were to remove layer from
   * layers_authority->layer_differences
   */

  /* The layer index of the layer we want info about */
  layer_info.layer_index = layer->index;

  /* If a layer already exists with the given index this will be
   * updated. */
  layer_info.layer = NULL;

  /* If a layer can't be found then we'll need to insert a new layer
   * and bump up the texture unit for all layers with an index
   * > layer_index. */
  layer_info.layers_to_shift =
    g_alloca (sizeof (CoglPipelineLayer *) * layers_authority->n_layers);
  layer_info.n_layers_to_shift = 0;

  /* If an exact match is found though we don't need a complete
   * list of layers with indices > layer_index... */
  layer_info.ignore_shift_layers_if_found = TRUE;

  /* We know the default/root pipeline isn't a LAYERS authority so it's
   * safe to use the result of _cogl_pipeline_get_parent (layers_authority)
   * without checking it.
   */
  old_layers_authority =
    _cogl_pipeline_get_authority (_cogl_pipeline_get_parent (layers_authority),
                                  COGL_PIPELINE_STATE_LAYERS);

  _cogl_pipeline_get_layer_info (old_layers_authority, &layer_info);

  /* If layer is the defining layer for the corresponding ->index then
   * we can't get rid of it. */
  if (!layer_info.layer)
    return;

  /* If the layer that would become the authority for layer->index is
   * _cogl_pipeline_layer_get_parent (layer) then we can simply remove the
   * layer difference. */
  if (layer_info.layer == _cogl_pipeline_layer_get_parent (layer))
    {
      _cogl_pipeline_remove_layer_difference (layers_authority, layer, FALSE);
      _cogl_pipeline_try_reverting_layers_authority (layers_authority,
                                                     old_layers_authority);
    }
}

typedef struct
{
  int i;
  CoglPipeline *pipeline;
  unsigned long fallback_layers;
} CoglPipelineFallbackState;

static CoglBool
fallback_layer_cb (CoglPipelineLayer *layer, void *user_data)
{
  CoglPipelineFallbackState *state = user_data;
  CoglPipeline *pipeline = state->pipeline;
  CoglTextureType texture_type = _cogl_pipeline_layer_get_texture_type (layer);
  CoglTexture *texture = NULL;
  COGL_STATIC_COUNTER (layer_fallback_counter,
                       "layer fallback counter",
                       "Increments each time a layer's texture is "
                       "forced to a fallback texture",
                       0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!(state->fallback_layers & 1<<state->i))
    return TRUE;

  COGL_COUNTER_INC (_cogl_uprof_context, layer_fallback_counter);

  switch (texture_type)
    {
    case COGL_TEXTURE_TYPE_2D:
      texture = COGL_TEXTURE (ctx->default_gl_texture_2d_tex);
      break;

    case COGL_TEXTURE_TYPE_3D:
      texture = COGL_TEXTURE (ctx->default_gl_texture_3d_tex);
      break;

    case COGL_TEXTURE_TYPE_RECTANGLE:
      texture = COGL_TEXTURE (ctx->default_gl_texture_rect_tex);
      break;
    }

  if (texture == NULL)
    {
      g_warning ("We don't have a fallback texture we can use to fill "
                 "in for an invalid pipeline layer, since it was "
                 "using an unsupported texture target ");
      /* might get away with this... */
      texture = COGL_TEXTURE (ctx->default_gl_texture_2d_tex);
    }

  cogl_pipeline_set_layer_texture (pipeline, layer->index, texture);

  state->i++;

  return TRUE;
}

typedef struct
{
  CoglPipeline *pipeline;
  CoglTexture *texture;
} CoglPipelineOverrideLayerState;

static CoglBool
override_layer_texture_cb (CoglPipelineLayer *layer, void *user_data)
{
  CoglPipelineOverrideLayerState *state = user_data;

  cogl_pipeline_set_layer_texture (state->pipeline,
                                   layer->index,
                                   state->texture);

  return TRUE;
}

void
_cogl_pipeline_apply_overrides (CoglPipeline *pipeline,
                                CoglPipelineFlushOptions *options)
{
  COGL_STATIC_COUNTER (apply_overrides_counter,
                       "pipeline overrides counter",
                       "Increments each time we have to apply "
                       "override options to a pipeline",
                       0 /* no application private data */);

  COGL_COUNTER_INC (_cogl_uprof_context, apply_overrides_counter);

  if (options->flags & COGL_PIPELINE_FLUSH_DISABLE_MASK)
    {
      int i;

      /* NB: we can assume that once we see one bit to disable
       * a layer, all subsequent layers are also disabled. */
      for (i = 0; i < 32 && options->disable_layers & (1<<i); i++)
        ;

      _cogl_pipeline_prune_to_n_layers (pipeline, i);
    }

  if (options->flags & COGL_PIPELINE_FLUSH_FALLBACK_MASK)
    {
      CoglPipelineFallbackState state;

      state.i = 0;
      state.pipeline = pipeline;
      state.fallback_layers = options->fallback_layers;

      _cogl_pipeline_foreach_layer_internal (pipeline,
                                             fallback_layer_cb,
                                             &state);
    }

  if (options->flags & COGL_PIPELINE_FLUSH_LAYER0_OVERRIDE)
    {
      CoglPipelineOverrideLayerState state;

      _cogl_pipeline_prune_to_n_layers (pipeline, 1);

      /* NB: we are overriding the first layer, but we don't know
       * the user's given layer_index, which is why we use
       * _cogl_pipeline_foreach_layer_internal() here even though we know
       * there's only one layer. */
      state.pipeline = pipeline;
      state.texture = options->layer0_override_texture;
      _cogl_pipeline_foreach_layer_internal (pipeline,
                                             override_layer_texture_cb,
                                             &state);
    }
}

static CoglBool
_cogl_pipeline_layers_equal (CoglPipeline *authority0,
                             CoglPipeline *authority1,
                             unsigned long differences,
                             CoglPipelineEvalFlags flags)
{
  int i;

  if (authority0->n_layers != authority1->n_layers)
    return FALSE;

  _cogl_pipeline_update_layers_cache (authority0);
  _cogl_pipeline_update_layers_cache (authority1);

  for (i = 0; i < authority0->n_layers; i++)
    {
      if (!_cogl_pipeline_layer_equal (authority0->layers_cache[i],
                                       authority1->layers_cache[i],
                                       differences,
                                       flags))
        return FALSE;
    }
  return TRUE;
}

/* Determine the mask of differences between two pipelines */
unsigned long
_cogl_pipeline_compare_differences (CoglPipeline *pipeline0,
                                    CoglPipeline *pipeline1)
{
  GSList *head0 = NULL;
  GSList *head1 = NULL;
  CoglPipeline *node0;
  CoglPipeline *node1;
  int len0 = 0;
  int len1 = 0;
  int count;
  GSList *common_ancestor0;
  GSList *common_ancestor1;
  unsigned long pipelines_difference = 0;

  /* Algorithm:
   *
   * 1) Walk the ancestors of each pipeline to the root node, adding a
   *    pointer to each ancester node to two linked lists
   *
   * 2) Compare the lists to find the nodes where they start to
   *    differ marking the common_ancestor node for each list.
   *
   * 3) For each list now iterate starting after the common_ancestor
   *    nodes ORing each nodes ->difference mask into the final
   *    differences mask.
   */

  for (node0 = pipeline0; node0; node0 = _cogl_pipeline_get_parent (node0))
    {
      GSList *link = alloca (sizeof (GSList));
      link->next = head0;
      link->data = node0;
      head0 = link;
      len0++;
    }
  for (node1 = pipeline1; node1; node1 = _cogl_pipeline_get_parent (node1))
    {
      GSList *link = alloca (sizeof (GSList));
      link->next = head1;
      link->data = node1;
      head1 = link;
      len1++;
    }

  /* NB: There's no point looking at the head entries since we know both
   * pipelines must have the same default pipeline as their root node. */
  common_ancestor0 = head0;
  common_ancestor1 = head1;
  head0 = head0->next;
  head1 = head1->next;
  count = MIN (len0, len1) - 1;
  while (count--)
    {
      if (head0->data != head1->data)
        break;
      common_ancestor0 = head0;
      common_ancestor1 = head1;
      head0 = head0->next;
      head1 = head1->next;
    }

  for (head0 = common_ancestor0->next; head0; head0 = head0->next)
    {
      node0 = head0->data;
      pipelines_difference |= node0->differences;
    }
  for (head1 = common_ancestor1->next; head1; head1 = head1->next)
    {
      node1 = head1->data;
      pipelines_difference |= node1->differences;
    }

  return pipelines_difference;
}

static void
_cogl_pipeline_resolve_authorities (CoglPipeline *pipeline,
                                    unsigned long differences,
                                    CoglPipeline **authorities)
{
  unsigned long remaining = differences;
  CoglPipeline *authority = pipeline;

  do
    {
      unsigned long found = authority->differences & remaining;
      int i;

      if (found == 0)
        continue;

      for (i = 0; TRUE; i++)
        {
          unsigned long state = (1L<<i);

          if (state & found)
            authorities[i] = authority;
          else if (state > found)
            break;
        }

      remaining &= ~found;
      if (remaining == 0)
        return;
    }
  while ((authority = _cogl_pipeline_get_parent (authority)));

  g_assert (remaining == 0);
}

/* Comparison of two arbitrary pipelines is done by:
 * 1) walking up the parents of each pipeline until a common
 *    ancestor is found, and at each step ORing together the
 *    difference masks.
 *
 * 2) using the final difference mask to determine which state
 *    groups to compare.
 *
 * This is used, for example, by the Cogl journal to compare pipelines so that
 * it can split up geometry that needs different OpenGL state.
 *
 * XXX: When comparing texture layers, _cogl_pipeline_equal will actually
 * compare the underlying GL texture handle that the Cogl texture uses so that
 * atlas textures and sub textures will be considered equal if they point to
 * the same texture. This is useful for comparing pipelines in the journal but
 * it means that _cogl_pipeline_equal doesn't strictly compare whether the
 * pipelines are the same. If we needed those semantics we could perhaps add
 * another function or some flags to control the behaviour.
 *
 * XXX: Similarly when comparing the wrap modes,
 * COGL_PIPELINE_WRAP_MODE_AUTOMATIC is considered to be the same as
 * COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE because once they get to the
 * journal stage they act exactly the same.
 */
CoglBool
_cogl_pipeline_equal (CoglPipeline *pipeline0,
                      CoglPipeline *pipeline1,
                      unsigned int differences,
                      unsigned long layer_differences,
                      CoglPipelineEvalFlags flags)
{
  unsigned long pipelines_difference;
  CoglPipeline *authorities0[COGL_PIPELINE_STATE_SPARSE_COUNT];
  CoglPipeline *authorities1[COGL_PIPELINE_STATE_SPARSE_COUNT];
  int bit;
  CoglBool ret;

  COGL_STATIC_TIMER (pipeline_equal_timer,
                     "Mainloop", /* parent */
                     "_cogl_pipeline_equal",
                     "The time spent comparing cogl pipelines",
                     0 /* no application private data */);

  COGL_TIMER_START (_cogl_uprof_context, pipeline_equal_timer);

  if (pipeline0 == pipeline1)
    {
      ret = TRUE;
      goto done;
    }

  ret = FALSE;

  _cogl_pipeline_update_real_blend_enable (pipeline0, FALSE);
  _cogl_pipeline_update_real_blend_enable (pipeline1, FALSE);

  /* First check non-sparse properties */

  if (differences & COGL_PIPELINE_STATE_REAL_BLEND_ENABLE &&
      pipeline0->real_blend_enable != pipeline1->real_blend_enable)
    goto done;

  /* Then check sparse properties */

  pipelines_difference =
    _cogl_pipeline_compare_differences (pipeline0, pipeline1);

  /* Only compare the sparse state groups requested by the caller... */
  pipelines_difference &= differences;

  _cogl_pipeline_resolve_authorities (pipeline0,
                                      pipelines_difference,
                                      authorities0);
  _cogl_pipeline_resolve_authorities (pipeline1,
                                      pipelines_difference,
                                      authorities1);

  COGL_FLAGS_FOREACH_START (&pipelines_difference, 1, bit)
    {
      /* XXX: We considered having an array of callbacks for each state index
       * that we'd call here but decided that this way the compiler is more
       * likely going to be able to in-line the comparison functions and use
       * the index to jump straight to the required code. */
      switch ((CoglPipelineStateIndex)bit)
        {
        case COGL_PIPELINE_STATE_COLOR_INDEX:
          if (!cogl_color_equal (&authorities0[bit]->color,
                                 &authorities1[bit]->color))
            goto done;
          break;
        case COGL_PIPELINE_STATE_LIGHTING_INDEX:
          if (!_cogl_pipeline_lighting_state_equal (authorities0[bit],
                                                    authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_ALPHA_FUNC_INDEX:
          if (!_cogl_pipeline_alpha_func_state_equal (authorities0[bit],
                                                      authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE_INDEX:
          if (!_cogl_pipeline_alpha_func_reference_state_equal (
                                                            authorities0[bit],
                                                            authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_BLEND_INDEX:
          /* We don't need to compare the detailed blending state if we know
           * blending is disabled for both pipelines. */
          if (pipeline0->real_blend_enable)
            {
              if (!_cogl_pipeline_blend_state_equal (authorities0[bit],
                                                     authorities1[bit]))
                goto done;
            }
          break;
        case COGL_PIPELINE_STATE_DEPTH_INDEX:
          if (!_cogl_pipeline_depth_state_equal (authorities0[bit],
                                                 authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_FOG_INDEX:
          if (!_cogl_pipeline_fog_state_equal (authorities0[bit],
                                               authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_CULL_FACE_INDEX:
          if (!_cogl_pipeline_cull_face_state_equal (authorities0[bit],
                                                     authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_POINT_SIZE_INDEX:
          if (!_cogl_pipeline_point_size_equal (authorities0[bit],
                                                authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_LOGIC_OPS_INDEX:
          if (!_cogl_pipeline_logic_ops_state_equal (authorities0[bit],
                                                     authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_USER_SHADER_INDEX:
          if (!_cogl_pipeline_user_shader_equal (authorities0[bit],
                                                 authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_UNIFORMS_INDEX:
          if (!_cogl_pipeline_uniforms_state_equal (authorities0[bit],
                                                    authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_VERTEX_SNIPPETS_INDEX:
          if (!_cogl_pipeline_vertex_snippets_state_equal (authorities0[bit],
                                                           authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS_INDEX:
          if (!_cogl_pipeline_fragment_snippets_state_equal (authorities0[bit],
                                                             authorities1[bit]))
            goto done;
          break;
        case COGL_PIPELINE_STATE_LAYERS_INDEX:
          {
            if (!_cogl_pipeline_layers_equal (authorities0[bit],
                                              authorities1[bit],
                                              layer_differences,
                                              flags))
              goto done;
            break;
          }

        case COGL_PIPELINE_STATE_BLEND_ENABLE_INDEX:
        case COGL_PIPELINE_STATE_REAL_BLEND_ENABLE_INDEX:
        case COGL_PIPELINE_STATE_COUNT:
          g_warn_if_reached ();
        }
    }
  COGL_FLAGS_FOREACH_END;

  ret = TRUE;
done:
  COGL_TIMER_STOP (_cogl_uprof_context, pipeline_equal_timer);
  return ret;
}

void
_cogl_pipeline_prune_redundant_ancestry (CoglPipeline *pipeline)
{
  CoglPipeline *new_parent = _cogl_pipeline_get_parent (pipeline);

  /* Before considering pruning redundant ancestry we check if this
   * pipeline is an authority for layer state and if so only consider
   * reparenting if it *owns* all the layers it depends on. NB: A
   * pipeline can be be a STATE_LAYERS authority but it may still
   * defer to its ancestors to define the state for some of its
   * layers.
   *
   * For example a pipeline that derives from a parent with 5 layers
   * can become a STATE_LAYERS authority by simply changing it's
   * ->n_layers count to 4 and in that case it can still defer to its
   * ancestors to define the state of those 4 layers.
   *
   * If a pipeline depends on any ancestors for layer state then we
   * immediatly bail out.
   */
  if (pipeline->differences & COGL_PIPELINE_STATE_LAYERS)
    {
      if (pipeline->n_layers != g_list_length (pipeline->layer_differences))
        return;
    }

  /* walk up past ancestors that are now redundant and potentially
   * reparent the pipeline. */
  while (_cogl_pipeline_get_parent (new_parent) &&
         (new_parent->differences | pipeline->differences) ==
          pipeline->differences)
    new_parent = _cogl_pipeline_get_parent (new_parent);

  if (new_parent != _cogl_pipeline_get_parent (pipeline))
    {
      CoglBool is_weak = _cogl_pipeline_is_weak (pipeline);
      _cogl_pipeline_set_parent (pipeline, new_parent, is_weak ? FALSE : TRUE);
    }
}

void
_cogl_pipeline_update_authority (CoglPipeline *pipeline,
                                 CoglPipeline *authority,
                                 CoglPipelineState state,
                                 CoglPipelineStateComparitor comparitor)
{
  /* If we are the current authority see if we can revert to one of
   * our ancestors being the authority */
  if (pipeline == authority &&
      _cogl_pipeline_get_parent (authority) != NULL)
    {
      CoglPipeline *parent = _cogl_pipeline_get_parent (authority);
      CoglPipeline *old_authority =
        _cogl_pipeline_get_authority (parent, state);

      if (comparitor (authority, old_authority))
        pipeline->differences &= ~state;
    }
  else if (pipeline != authority)
    {
      /* If we weren't previously the authority on this state then we
       * need to extended our differences mask and so it's possible
       * that some of our ancestry will now become redundant, so we
       * aim to reparent ourselves if that's true... */
      pipeline->differences |= state;
      _cogl_pipeline_prune_redundant_ancestry (pipeline);
    }
}

CoglBool
_cogl_pipeline_get_fog_enabled (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), FALSE);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_FOG);
  return authority->big_state->fog_state.enabled;
}

unsigned long
_cogl_pipeline_get_age (CoglPipeline *pipeline)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), 0);

  return pipeline->age;
}

void
cogl_pipeline_remove_layer (CoglPipeline *pipeline, int layer_index)
{
  CoglPipeline         *authority;
  CoglPipelineLayerInfo layer_info;
  int                   i;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LAYERS);

  /* The layer index of the layer we want info about */
  layer_info.layer_index = layer_index;

  /* This will be updated with a reference to the layer being removed
   * if it can be found. */
  layer_info.layer = NULL;

  /* This will be filled in with a list of layers that need to be
   * dropped down to a lower texture unit to fill the gap of the
   * removed layer. */
  layer_info.layers_to_shift =
    g_alloca (sizeof (CoglPipelineLayer *) * authority->n_layers);
  layer_info.n_layers_to_shift = 0;

  /* Unlike when we query layer info when adding a layer we must
   * always have a complete layers_to_shift list... */
  layer_info.ignore_shift_layers_if_found = FALSE;

  _cogl_pipeline_get_layer_info (authority, &layer_info);

  if (layer_info.layer == NULL)
    return;

  for (i = 0; i < layer_info.n_layers_to_shift; i++)
    {
      CoglPipelineLayer *shift_layer = layer_info.layers_to_shift[i];
      int unit_index = _cogl_pipeline_layer_get_unit_index (shift_layer);
      _cogl_pipeline_set_layer_unit (pipeline, shift_layer, unit_index - 1);
      /* NB: shift_layer may not be writeable so _set_layer_unit()
       * will allocate a derived layer internally which will become
       * owned by pipeline. Check the return value if we need to do
       * anything else with this layer. */
    }

  _cogl_pipeline_remove_layer_difference (pipeline, layer_info.layer, TRUE);
  _cogl_pipeline_try_reverting_layers_authority (pipeline, NULL);

  pipeline->dirty_real_blend_enable = TRUE;
}

static CoglBool
prepend_layer_to_list_cb (CoglPipelineLayer *layer,
                          void *user_data)
{
  GList **layers = user_data;

  *layers = g_list_prepend (*layers, layer);
  return TRUE;
}

/* TODO: deprecate this API and replace it with
 * cogl_pipeline_foreach_layer
 * TODO: update the docs to note that if the user modifies any layers
 * then the list may become invalid.
 */
const GList *
_cogl_pipeline_get_layers (CoglPipeline *pipeline)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), NULL);

  if (!pipeline->deprecated_get_layers_list_dirty)
    g_list_free (pipeline->deprecated_get_layers_list);

  pipeline->deprecated_get_layers_list = NULL;

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         prepend_layer_to_list_cb,
                                         &pipeline->deprecated_get_layers_list);
  pipeline->deprecated_get_layers_list =
    g_list_reverse (pipeline->deprecated_get_layers_list);

  pipeline->deprecated_get_layers_list_dirty = 0;

  return pipeline->deprecated_get_layers_list;
}

int
cogl_pipeline_get_n_layers (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), 0);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LAYERS);

  return authority->n_layers;
}

void
_cogl_pipeline_pre_paint_for_layer (CoglPipeline *pipeline,
                                    int layer_id)
{
  CoglPipelineLayer *layer = _cogl_pipeline_get_layer (pipeline, layer_id);
  _cogl_pipeline_layer_pre_paint (layer);
}

/* While a pipeline is referenced by the Cogl journal we can not allow
 * modifications, so this gives us a mechanism to track journal
 * references separately */
CoglPipeline *
_cogl_pipeline_journal_ref (CoglPipeline *pipeline)
{
  pipeline->journal_ref_count++;
  return cogl_object_ref (pipeline);
}

void
_cogl_pipeline_journal_unref (CoglPipeline *pipeline)
{
  pipeline->journal_ref_count--;
  cogl_object_unref (pipeline);
}

void
_cogl_pipeline_apply_legacy_state (CoglPipeline *pipeline)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* It was a mistake that we ever copied the OpenGL style API for
   * associating these things directly with the context when we
   * originally wrote Cogl. Until the corresponding deprecated APIs
   * can be removed though we now shoehorn the state changes through
   * the cogl_pipeline API instead.
   */

  /* A program explicitly set on the pipeline has higher precedence than
   * one associated with the context using cogl_program_use() */
  if (ctx->current_program &&
      cogl_pipeline_get_user_program (pipeline) == COGL_INVALID_HANDLE)
    cogl_pipeline_set_user_program (pipeline, ctx->current_program);

  if (ctx->legacy_depth_test_enabled)
    {
      CoglDepthState depth_state;
      cogl_depth_state_init (&depth_state);
      cogl_depth_state_set_test_enabled (&depth_state, TRUE);
      cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);
    }

  if (ctx->legacy_fog_state.enabled)
    _cogl_pipeline_set_fog_state (pipeline, &ctx->legacy_fog_state);

  if (ctx->legacy_backface_culling_enabled)
    cogl_pipeline_set_cull_face_mode (pipeline,
                                      COGL_PIPELINE_CULL_FACE_MODE_BACK);
}

void
_cogl_pipeline_set_static_breadcrumb (CoglPipeline *pipeline,
                                      const char *breadcrumb)
{
  pipeline->has_static_breadcrumb = TRUE;
  pipeline->static_breadcrumb = breadcrumb;
}

typedef void (*LayerStateHashFunction) (CoglPipelineLayer *authority,
                                        CoglPipelineLayer **authorities,
                                        CoglPipelineHashState *state);

static LayerStateHashFunction
layer_state_hash_functions[COGL_PIPELINE_LAYER_STATE_SPARSE_COUNT];

/* XXX: We don't statically initialize the array of hash functions, so
 * we won't get caught out by later re-indexing the groups for some
 * reason. */
void
_cogl_pipeline_init_layer_state_hash_functions (void)
{
  CoglPipelineLayerStateIndex _index;
  layer_state_hash_functions[COGL_PIPELINE_LAYER_STATE_UNIT_INDEX] =
    _cogl_pipeline_layer_hash_unit_state;
  layer_state_hash_functions[COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX] =
    _cogl_pipeline_layer_hash_texture_type_state;
  layer_state_hash_functions[COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX] =
    _cogl_pipeline_layer_hash_texture_data_state;
  layer_state_hash_functions[COGL_PIPELINE_LAYER_STATE_SAMPLER_INDEX] =
    _cogl_pipeline_layer_hash_sampler_state;
  layer_state_hash_functions[COGL_PIPELINE_LAYER_STATE_COMBINE_INDEX] =
    _cogl_pipeline_layer_hash_combine_state;
  layer_state_hash_functions[COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT_INDEX] =
    _cogl_pipeline_layer_hash_combine_constant_state;
  layer_state_hash_functions[COGL_PIPELINE_LAYER_STATE_USER_MATRIX_INDEX] =
    _cogl_pipeline_layer_hash_user_matrix_state;
  _index = COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX;
  layer_state_hash_functions[_index] =
    _cogl_pipeline_layer_hash_point_sprite_state;
  _index = COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX;
  layer_state_hash_functions[_index] =
    _cogl_pipeline_layer_hash_point_sprite_state;
  _index = COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX;
  layer_state_hash_functions[_index] =
    _cogl_pipeline_layer_hash_fragment_snippets_state;

  {
  /* So we get a big error if we forget to update this code! */
  _COGL_STATIC_ASSERT (COGL_PIPELINE_LAYER_STATE_SPARSE_COUNT == 10,
                       "Don't forget to install a hash function for new "
                       "pipeline state and update assert at end of "
                       "_cogl_pipeline_init_state_hash_functions");
  }
}

static CoglBool
_cogl_pipeline_hash_layer_cb (CoglPipelineLayer *layer,
                              void *user_data)
{
  CoglPipelineHashState *state = user_data;
  unsigned long differences = state->layer_differences;
  CoglPipelineLayer *authorities[COGL_PIPELINE_LAYER_STATE_COUNT];
  unsigned long mask;
  int i;

  /* Theoretically we would hash non-sparse layer state here but
   * currently layers don't have any. */

  /* XXX: we resolve all the authorities here - not just those
   * corresponding to hash_state->layer_differences - because
   * the hashing of some state groups actually depends on the values
   * in other groups. For example we don't hash layer combine
   * constants if they are aren't referenced by the current layer
   * combine function.
   */
  mask = COGL_PIPELINE_LAYER_STATE_ALL_SPARSE;
  _cogl_pipeline_layer_resolve_authorities (layer,
                                            mask,
                                            authorities);

  /* So we go right ahead and hash the sparse state... */
  for (i = 0; i < COGL_PIPELINE_LAYER_STATE_COUNT; i++)
    {
      unsigned long current_state = (1L<<i);

      /* XXX: we are hashing the un-mixed hash values of all the
       * individual state groups; we should provide a means to test
       * the quality of the final hash values we are getting with this
       * approach... */
      if (differences & current_state)
        {
          CoglPipelineLayer *authority = authorities[i];
          layer_state_hash_functions[i] (authority, authorities, state);
        }

      if (current_state > differences)
        break;
    }

  return TRUE;
}

void
_cogl_pipeline_hash_layers_state (CoglPipeline *authority,
                                  CoglPipelineHashState *state)
{
  state->hash =
    _cogl_util_one_at_a_time_hash (state->hash, &authority->n_layers,
                                   sizeof (authority->n_layers));
  _cogl_pipeline_foreach_layer_internal (authority,
                                         _cogl_pipeline_hash_layer_cb,
                                         state);
}

typedef void (*StateHashFunction) (CoglPipeline *authority, CoglPipelineHashState *state);

static StateHashFunction
state_hash_functions[COGL_PIPELINE_STATE_SPARSE_COUNT];

/* We don't statically initialize the array of hash functions
 * so we won't get caught out by later re-indexing the groups for
 * some reason. */
void
_cogl_pipeline_init_state_hash_functions (void)
{
  state_hash_functions[COGL_PIPELINE_STATE_COLOR_INDEX] =
    _cogl_pipeline_hash_color_state;
  state_hash_functions[COGL_PIPELINE_STATE_BLEND_ENABLE_INDEX] =
    _cogl_pipeline_hash_blend_enable_state;
  state_hash_functions[COGL_PIPELINE_STATE_LAYERS_INDEX] =
    _cogl_pipeline_hash_layers_state;
  state_hash_functions[COGL_PIPELINE_STATE_LIGHTING_INDEX] =
    _cogl_pipeline_hash_lighting_state;
  state_hash_functions[COGL_PIPELINE_STATE_ALPHA_FUNC_INDEX] =
    _cogl_pipeline_hash_alpha_func_state;
  state_hash_functions[COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE_INDEX] =
    _cogl_pipeline_hash_alpha_func_reference_state;
  state_hash_functions[COGL_PIPELINE_STATE_BLEND_INDEX] =
    _cogl_pipeline_hash_blend_state;
  state_hash_functions[COGL_PIPELINE_STATE_USER_SHADER_INDEX] =
    _cogl_pipeline_hash_user_shader_state;
  state_hash_functions[COGL_PIPELINE_STATE_DEPTH_INDEX] =
    _cogl_pipeline_hash_depth_state;
  state_hash_functions[COGL_PIPELINE_STATE_FOG_INDEX] =
    _cogl_pipeline_hash_fog_state;
  state_hash_functions[COGL_PIPELINE_STATE_CULL_FACE_INDEX] =
    _cogl_pipeline_hash_cull_face_state;
  state_hash_functions[COGL_PIPELINE_STATE_POINT_SIZE_INDEX] =
    _cogl_pipeline_hash_point_size_state;
  state_hash_functions[COGL_PIPELINE_STATE_LOGIC_OPS_INDEX] =
    _cogl_pipeline_hash_logic_ops_state;
  state_hash_functions[COGL_PIPELINE_STATE_UNIFORMS_INDEX] =
    _cogl_pipeline_hash_uniforms_state;
  state_hash_functions[COGL_PIPELINE_STATE_VERTEX_SNIPPETS_INDEX] =
    _cogl_pipeline_hash_vertex_snippets_state;
  state_hash_functions[COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS_INDEX] =
    _cogl_pipeline_hash_fragment_snippets_state;

  {
  /* So we get a big error if we forget to update this code! */
  _COGL_STATIC_ASSERT (COGL_PIPELINE_STATE_SPARSE_COUNT == 16,
                       "Make sure to install a hash function for "
                       "newly added pipeline state and update assert "
                       "in _cogl_pipeline_init_state_hash_functions");
  }
}

unsigned int
_cogl_pipeline_hash (CoglPipeline *pipeline,
                     unsigned int differences,
                     unsigned long layer_differences,
                     CoglPipelineEvalFlags flags)
{
  CoglPipeline *authorities[COGL_PIPELINE_STATE_SPARSE_COUNT];
  unsigned int mask;
  int i;
  CoglPipelineHashState state;
  unsigned int final_hash = 0;

  state.hash = 0;
  state.layer_differences = layer_differences;
  state.flags = flags;

  _cogl_pipeline_update_real_blend_enable (pipeline, FALSE);

  /* hash non-sparse state */

  if (differences & COGL_PIPELINE_STATE_REAL_BLEND_ENABLE)
    {
      CoglBool enable = pipeline->real_blend_enable;
      state.hash =
        _cogl_util_one_at_a_time_hash (state.hash, &enable, sizeof (enable));
    }

  /* hash sparse state */

  mask = differences & COGL_PIPELINE_STATE_ALL_SPARSE;
  _cogl_pipeline_resolve_authorities (pipeline, mask, authorities);

  for (i = 0; i < COGL_PIPELINE_STATE_SPARSE_COUNT; i++)
    {
      unsigned int current_state = (1<<i);

      /* XXX: we are hashing the un-mixed hash values of all the
       * individual state groups; we should provide a means to test
       * the quality of the final hash values we are getting with this
       * approach... */
      if (differences & current_state)
        {
          CoglPipeline *authority = authorities[i];
          state_hash_functions[i] (authority, &state);
          final_hash = _cogl_util_one_at_a_time_hash (final_hash, &state.hash,
                                                      sizeof (state.hash));
        }

      if (current_state > differences)
        break;
    }

  return _cogl_util_one_at_a_time_mix (final_hash);
}

typedef struct
{
  CoglContext *context;
  CoglPipeline *src_pipeline;
  CoglPipeline *dst_pipeline;
  unsigned int layer_differences;
} DeepCopyData;

static CoglBool
deep_copy_layer_cb (CoglPipelineLayer *src_layer,
                    void *user_data)
{
  DeepCopyData *data = user_data;
  CoglPipelineLayer *dst_layer;
  unsigned int differences = data->layer_differences;

  dst_layer = _cogl_pipeline_get_layer (data->dst_pipeline, src_layer->index);

  while (src_layer != data->context->default_layer_n &&
         src_layer != data->context->default_layer_0 &&
         differences)
    {
      unsigned long to_copy = differences & src_layer->differences;

      if (to_copy)
        {
          _cogl_pipeline_layer_copy_differences (dst_layer, src_layer, to_copy);
          differences ^= to_copy;
        }

      src_layer = COGL_PIPELINE_LAYER (COGL_NODE (src_layer)->parent);
    }

  return TRUE;
}

CoglPipeline *
_cogl_pipeline_deep_copy (CoglPipeline *pipeline,
                          unsigned long differences,
                          unsigned long layer_differences)
{
  CoglPipeline *new, *authority;
  CoglBool copy_layer_state;

  _COGL_GET_CONTEXT (ctx, NULL);

  if ((differences & COGL_PIPELINE_STATE_LAYERS))
    {
      copy_layer_state = TRUE;
      differences &= ~COGL_PIPELINE_STATE_LAYERS;
    }
  else
    copy_layer_state = FALSE;

  new = cogl_pipeline_new (ctx);

  for (authority = pipeline;
       authority != ctx->default_pipeline && differences;
       authority = COGL_PIPELINE (COGL_NODE (authority)->parent))
    {
      unsigned long to_copy = differences & authority->differences;

      if (to_copy)
        {
          _cogl_pipeline_copy_differences (new, authority, to_copy);
          differences ^= to_copy;
        }
    }

  if (copy_layer_state)
    {
      DeepCopyData data;

      /* The unit index doesn't need to be copied because it should
       * end up with the same values anyway because the new pipeline
       * will have the same indices as the source pipeline */
      layer_differences &= ~COGL_PIPELINE_LAYER_STATE_UNIT;

      data.context = ctx;
      data.src_pipeline = pipeline;
      data.dst_pipeline = new;
      data.layer_differences = layer_differences;

      _cogl_pipeline_foreach_layer_internal (pipeline,
                                             deep_copy_layer_cb,
                                             &data);
    }

  return new;
}

typedef struct
{
  int i;
  CoglPipelineLayer **layers;
} AddLayersToArrayState;

static CoglBool
add_layer_to_array_cb (CoglPipelineLayer *layer,
                       void *user_data)
{
  AddLayersToArrayState *state = user_data;
  state->layers[state->i++] = layer;
  return TRUE;
}

/* This tries to find the oldest ancestor whose pipeline and layer
   state matches the given flags. This is mostly used to detect code
   gen authorities so that we can reduce the numer of programs
   generated */
CoglPipeline *
_cogl_pipeline_find_equivalent_parent (CoglPipeline *pipeline,
                                       CoglPipelineState pipeline_state,
                                       CoglPipelineLayerState layer_state)
{
  CoglPipeline *authority0;
  CoglPipeline *authority1;
  int n_layers;
  CoglPipelineLayer **authority0_layers;
  CoglPipelineLayer **authority1_layers;

  /* Find the first pipeline that modifies state that affects the
   * state or any layer state... */
  authority0 = _cogl_pipeline_get_authority (pipeline,
                                             pipeline_state |
                                             COGL_PIPELINE_STATE_LAYERS);

  /* Find the next ancestor after that, that also modifies the
   * state... */
  if (_cogl_pipeline_get_parent (authority0))
    {
      authority1 =
        _cogl_pipeline_get_authority (_cogl_pipeline_get_parent (authority0),
                                      pipeline_state |
                                      COGL_PIPELINE_STATE_LAYERS);
    }
  else
    return authority0;

  n_layers = cogl_pipeline_get_n_layers (authority0);

  for (;;)
    {
      AddLayersToArrayState state;
      int i;

      if (n_layers != cogl_pipeline_get_n_layers (authority1))
        return authority0;

      /* If the programs differ by anything that isn't part of the
         layer state then we can't continue */
      if (pipeline_state &&
          (_cogl_pipeline_compare_differences (authority0, authority1) &
           pipeline_state))
        return authority0;

      authority0_layers =
        g_alloca (sizeof (CoglPipelineLayer *) * n_layers);
      state.i = 0;
      state.layers = authority0_layers;
      _cogl_pipeline_foreach_layer_internal (authority0,
                                             add_layer_to_array_cb,
                                             &state);

      authority1_layers =
        g_alloca (sizeof (CoglPipelineLayer *) * n_layers);
      state.i = 0;
      state.layers = authority1_layers;
      _cogl_pipeline_foreach_layer_internal (authority1,
                                             add_layer_to_array_cb,
                                             &state);

      for (i = 0; i < n_layers; i++)
        {
          unsigned long layer_differences;

          if (authority0_layers[i] == authority1_layers[i])
            continue;

          layer_differences =
            _cogl_pipeline_layer_compare_differences (authority0_layers[i],
                                                      authority1_layers[i]);

          if (layer_differences & layer_state)
            return authority0;
        }

      /* Find the next ancestor after that, that also modifies state
       * affecting codegen... */

      if (!_cogl_pipeline_get_parent (authority1))
        break;

      authority0 = authority1;
      authority1 =
        _cogl_pipeline_get_authority (_cogl_pipeline_get_parent (authority1),
                                      pipeline_state |
                                      COGL_PIPELINE_STATE_LAYERS);
      if (authority1 == authority0)
        break;
    }

  return authority1;
}

CoglPipelineLayerState
_cogl_pipeline_get_layer_state_for_fragment_codegen (CoglContext *context)
{
  CoglPipelineLayerState state =
    (COGL_PIPELINE_LAYER_STATE_COMBINE |
     COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE |
     COGL_PIPELINE_LAYER_STATE_UNIT |
     COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS);

  /* If the driver supports GLSL then we might be using gl_PointCoord
   * to implement the sprite coords. In that case the generated code
   * depends on the point sprite state */
  if (cogl_has_feature (context, COGL_FEATURE_ID_GLSL))
    state |= COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS;

  return state;
}

CoglPipelineState
_cogl_pipeline_get_state_for_fragment_codegen (CoglContext *context)
{
  CoglPipelineState state = (COGL_PIPELINE_STATE_LAYERS |
                             COGL_PIPELINE_STATE_USER_SHADER |
                             COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS);

  if (!(context->private_feature_flags & COGL_PRIVATE_FEATURE_ALPHA_TEST))
    state |= COGL_PIPELINE_STATE_ALPHA_FUNC;

  return state;
}

int
cogl_pipeline_get_uniform_location (CoglPipeline *pipeline,
                                    const char *uniform_name)
{
  void *location_ptr;
  char *uniform_name_copy;

  _COGL_GET_CONTEXT (ctx, -1);

  /* This API is designed as if the uniform locations are specific to
     a pipeline but they are actually unique across a whole
     CoglContext. Potentially this could just be
     cogl_context_get_uniform_location but it seems to make sense to
     keep the API this way so that we can change the internals if need
     be. */

  /* Look for an existing uniform with this name */
  if (g_hash_table_lookup_extended (ctx->uniform_name_hash,
                                    uniform_name,
                                    NULL,
                                    &location_ptr))
    return GPOINTER_TO_INT (location_ptr);

  uniform_name_copy = g_strdup (uniform_name);
  g_ptr_array_add (ctx->uniform_names, uniform_name_copy);
  g_hash_table_insert (ctx->uniform_name_hash,
                       uniform_name_copy,
                       GINT_TO_POINTER (ctx->n_uniform_names));

  return ctx->n_uniform_names++;
}
