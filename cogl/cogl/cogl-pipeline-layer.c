/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-texture-private.h"

#include "cogl-pipeline.h"
#include "cogl-pipeline-layer-private.h"
#include "cogl-pipeline-layer-state-private.h"
#include "cogl-pipeline-layer-state.h"
#include "cogl-node-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-context-private.h"
#include "cogl-texture-private.h"

#include <string.h>

static void
_cogl_pipeline_layer_free (CoglPipelineLayer *layer);

/* This type was made deprecated before the cogl_is_pipeline_layer
   function was ever exposed in the public headers so there's no need
   to make the cogl_is_pipeline_layer function public. We use INTERNAL
   so that the cogl_is_* function won't get defined */
COGL_OBJECT_INTERNAL_DEFINE (PipelineLayer, pipeline_layer);


CoglPipelineLayer *
_cogl_pipeline_layer_get_authority (CoglPipelineLayer *layer,
                                    unsigned long difference)
{
  CoglPipelineLayer *authority = layer;
  while (!(authority->differences & difference))
    authority = _cogl_pipeline_layer_get_parent (authority);
  return authority;
}

int
_cogl_pipeline_layer_get_unit_index (CoglPipelineLayer *layer)
{
  CoglPipelineLayer *authority =
    _cogl_pipeline_layer_get_authority (layer, COGL_PIPELINE_LAYER_STATE_UNIT);
  return authority->unit_index;
}

CoglBool
_cogl_pipeline_layer_has_alpha (CoglPipelineLayer *layer)
{
  CoglPipelineLayer *combine_authority =
    _cogl_pipeline_layer_get_authority (layer,
                                        COGL_PIPELINE_LAYER_STATE_COMBINE);
  CoglPipelineLayerBigState *big_state = combine_authority->big_state;
  CoglPipelineLayer *tex_authority;
  CoglPipelineLayer *snippets_authority;

  /* has_alpha maintains the alpha status for the GL_PREVIOUS layer */

  /* For anything but the default texture combine we currently just
   * assume it may result in an alpha value < 1
   *
   * FIXME: we could do better than this. */
  if (big_state->texture_combine_alpha_func !=
      COGL_PIPELINE_COMBINE_FUNC_MODULATE ||
      big_state->texture_combine_alpha_src[0] !=
      COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS ||
      big_state->texture_combine_alpha_op[0] !=
      COGL_PIPELINE_COMBINE_OP_SRC_ALPHA ||
      big_state->texture_combine_alpha_src[1] !=
      COGL_PIPELINE_COMBINE_SOURCE_TEXTURE ||
      big_state->texture_combine_alpha_op[1] !=
      COGL_PIPELINE_COMBINE_OP_SRC_ALPHA)
    {
      return TRUE;
    }

  /* NB: A layer may have a combine mode set on it but not yet
   * have an associated texture which would mean we'd fallback
   * to the default texture which doesn't have an alpha component
   */
  tex_authority =
    _cogl_pipeline_layer_get_authority (layer,
                                        COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA);
  if (tex_authority->texture &&
      _cogl_texture_get_format (tex_authority->texture) & COGL_A_BIT)
    {
      return TRUE;
    }

  /* All bets are off if the layer contains any snippets */
  snippets_authority = _cogl_pipeline_layer_get_authority
    (layer, COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS);
  if (snippets_authority->big_state->vertex_snippets.entries != NULL)
    return TRUE;
  snippets_authority = _cogl_pipeline_layer_get_authority
    (layer, COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS);
  if (snippets_authority->big_state->fragment_snippets.entries != NULL)
    return TRUE;

  return FALSE;
}

unsigned int
_cogl_get_n_args_for_combine_func (CoglPipelineCombineFunc func)
{
  switch (func)
    {
    case COGL_PIPELINE_COMBINE_FUNC_REPLACE:
      return 1;
    case COGL_PIPELINE_COMBINE_FUNC_MODULATE:
    case COGL_PIPELINE_COMBINE_FUNC_ADD:
    case COGL_PIPELINE_COMBINE_FUNC_ADD_SIGNED:
    case COGL_PIPELINE_COMBINE_FUNC_SUBTRACT:
    case COGL_PIPELINE_COMBINE_FUNC_DOT3_RGB:
    case COGL_PIPELINE_COMBINE_FUNC_DOT3_RGBA:
      return 2;
    case COGL_PIPELINE_COMBINE_FUNC_INTERPOLATE:
      return 3;
    }
  return 0;
}

void
_cogl_pipeline_layer_copy_differences (CoglPipelineLayer *dest,
                                       CoglPipelineLayer *src,
                                       unsigned long differences)
{
  CoglPipelineLayerBigState *big_dest, *big_src;

  if ((differences & COGL_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE) &&
      !dest->has_big_state)
    {
      dest->big_state = g_slice_new (CoglPipelineLayerBigState);
      dest->has_big_state = TRUE;
    }

  big_dest = dest->big_state;
  big_src = src->big_state;

  dest->differences |= differences;

  while (differences)
    {
      int index = _cogl_util_ffs (differences) - 1;

      differences &= ~(1 << index);

      /* This convoluted switch statement is just here so that we'll
       * get a warning if a new state is added without handling it
       * here */
      switch (index)
        {
        case COGL_PIPELINE_LAYER_STATE_COUNT:
        case COGL_PIPELINE_LAYER_STATE_UNIT_INDEX:
          g_warn_if_reached ();
          break;

        case COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX:
          dest->texture_type = src->texture_type;
          break;

        case COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX:
          dest->texture = src->texture;
          if (dest->texture)
            cogl_object_ref (dest->texture);
          break;

        case COGL_PIPELINE_LAYER_STATE_SAMPLER_INDEX:
          dest->sampler_cache_entry = src->sampler_cache_entry;
          break;

        case COGL_PIPELINE_LAYER_STATE_COMBINE_INDEX:
          {
            CoglPipelineCombineFunc func;
            int n_args, i;

            func = big_src->texture_combine_rgb_func;
            big_dest->texture_combine_rgb_func = func;
            n_args = _cogl_get_n_args_for_combine_func (func);
            for (i = 0; i < n_args; i++)
              {
                big_dest->texture_combine_rgb_src[i] =
                  big_src->texture_combine_rgb_src[i];
                big_dest->texture_combine_rgb_op[i] =
                  big_src->texture_combine_rgb_op[i];
              }

            func = big_src->texture_combine_alpha_func;
            big_dest->texture_combine_alpha_func = func;
            n_args = _cogl_get_n_args_for_combine_func (func);
            for (i = 0; i < n_args; i++)
              {
                big_dest->texture_combine_alpha_src[i] =
                  big_src->texture_combine_alpha_src[i];
                big_dest->texture_combine_alpha_op[i] =
                  big_src->texture_combine_alpha_op[i];
              }
          }
          break;

        case COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT_INDEX:
          memcpy (big_dest->texture_combine_constant,
                  big_src->texture_combine_constant,
                  sizeof (big_dest->texture_combine_constant));
          break;

        case COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX:
          big_dest->point_sprite_coords = big_src->point_sprite_coords;
          break;

        case COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX:
          _cogl_pipeline_snippet_list_copy (&big_dest->vertex_snippets,
                                            &big_src->vertex_snippets);
          break;

        case COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX:
          _cogl_pipeline_snippet_list_copy (&big_dest->fragment_snippets,
                                            &big_src->fragment_snippets);
          break;
        }
    }
}

static void
_cogl_pipeline_layer_init_multi_property_sparse_state (
                                                  CoglPipelineLayer *layer,
                                                  CoglPipelineLayerState change)
{
  CoglPipelineLayer *authority;

  /* Nothing to initialize in these cases since they are all comprised
   * of one member which we expect to immediately be overwritten. */
  if (!(change & COGL_PIPELINE_LAYER_STATE_MULTI_PROPERTY))
    return;

  authority = _cogl_pipeline_layer_get_authority (layer, change);

  switch (change)
    {
    /* XXX: avoid using a default: label so we get a warning if we
     * don't explicitly handle a newly defined state-group here. */
    case COGL_PIPELINE_LAYER_STATE_UNIT:
    case COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE:
    case COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA:
    case COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS:
    case COGL_PIPELINE_LAYER_STATE_USER_MATRIX:
    case COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT:
    case COGL_PIPELINE_LAYER_STATE_SAMPLER:
      g_return_if_reached ();

    /* XXX: technically we could probably even consider these as
     * single property state-groups from the pov that currently the
     * corresponding property setters always update all of the values
     * at the same time. */
    case COGL_PIPELINE_LAYER_STATE_COMBINE:
      {
        int n_args;
        int i;
        CoglPipelineLayerBigState *src_big_state = authority->big_state;
        CoglPipelineLayerBigState *dest_big_state = layer->big_state;
        GLint func = src_big_state->texture_combine_rgb_func;

        dest_big_state->texture_combine_rgb_func = func;
        n_args = _cogl_get_n_args_for_combine_func (func);
        for (i = 0; i < n_args; i++)
          {
            dest_big_state->texture_combine_rgb_src[i] =
              src_big_state->texture_combine_rgb_src[i];
            dest_big_state->texture_combine_rgb_op[i] =
              src_big_state->texture_combine_rgb_op[i];
          }

        func = src_big_state->texture_combine_alpha_func;
        dest_big_state->texture_combine_alpha_func = func;
        n_args = _cogl_get_n_args_for_combine_func (func);
        for (i = 0; i < n_args; i++)
          {
            dest_big_state->texture_combine_alpha_src[i] =
              src_big_state->texture_combine_alpha_src[i];
            dest_big_state->texture_combine_alpha_op[i] =
              src_big_state->texture_combine_alpha_op[i];
          }
        break;
      }
    case COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS:
      _cogl_pipeline_snippet_list_copy (&layer->big_state->vertex_snippets,
                                        &authority->big_state->
                                        vertex_snippets);
      break;
    case COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS:
      _cogl_pipeline_snippet_list_copy (&layer->big_state->fragment_snippets,
                                        &authority->big_state->
                                        fragment_snippets);
      break;
    }
}

/* NB: If a layer has descendants we can't modify the layer
 * NB: If the layer is owned and the owner has descendants we can't
 *     modify the layer.
 *
 * This function will allocate a new derived layer if you are trying
 * to change the state of a layer with dependants (as described above)
 * so you must always check the return value.
 *
 * If a new layer is returned it will be owned by required_owner.
 * (NB: a layer is always modified with respect to a pipeline - the
 *  "required_owner")
 *
 * required_owner can only by NULL for new, currently unowned layers
 * with no dependants.
 */
CoglPipelineLayer *
_cogl_pipeline_layer_pre_change_notify (CoglPipeline *required_owner,
                                        CoglPipelineLayer *layer,
                                        CoglPipelineLayerState change)
{
  CoglTextureUnit *unit;

  /* Identify the case where the layer is new with no owner or
   * dependants and so we don't need to do anything. */
  if (_cogl_list_empty (&COGL_NODE (layer)->children) &&
      layer->owner == NULL)
    goto init_layer_state;

  /* We only allow a NULL required_owner for new layers */
  _COGL_RETURN_VAL_IF_FAIL (required_owner != NULL, layer);

  /* Chain up:
   * A modification of a layer is indirectly also a modification of
   * its owner so first make sure to flush the journal of any
   * references to the current owner state and if necessary perform
   * a copy-on-write for the required_owner if it has dependants.
   */
  _cogl_pipeline_pre_change_notify (required_owner,
                                    COGL_PIPELINE_STATE_LAYERS,
                                    NULL,
                                    TRUE);

  /* Unlike pipelines; layers are simply considered immutable once
   * they have dependants - either direct children, or another
   * pipeline as an owner.
   */
  if (!_cogl_list_empty (&COGL_NODE (layer)->children) ||
      layer->owner != required_owner)
    {
      CoglPipelineLayer *new = _cogl_pipeline_layer_copy (layer);
      if (layer->owner == required_owner)
        _cogl_pipeline_remove_layer_difference (required_owner, layer, FALSE);
      _cogl_pipeline_add_layer_difference (required_owner, new, FALSE);
      cogl_object_unref (new);
      layer = new;
      goto init_layer_state;
    }

  /* Note: At this point we know there is only one pipeline dependant on
   * this layer (required_owner), and there are no other layers
   * dependant on this layer so it's ok to modify it. */

  /* NB: Although layers can have private state associated with them
   * by multiple backends we know that a layer can't be *changed* if
   * it has multiple dependants so if we reach here we know we only
   * have a single owner and can only be associated with a single
   * backend that needs to be notified of the layer change...
   */
  if (required_owner->progend != COGL_PIPELINE_PROGEND_UNDEFINED)
    {
      const CoglPipelineProgend *progend =
        _cogl_pipeline_progends[required_owner->progend];
      const CoglPipelineFragend *fragend =
        _cogl_pipeline_fragends[progend->fragend];
      const CoglPipelineVertend *vertend =
        _cogl_pipeline_vertends[progend->vertend];

      if (fragend->layer_pre_change_notify)
        fragend->layer_pre_change_notify (required_owner, layer, change);
      if (vertend->layer_pre_change_notify)
        vertend->layer_pre_change_notify (required_owner, layer, change);
      if (progend->layer_pre_change_notify)
        progend->layer_pre_change_notify (required_owner, layer, change);
    }

  /* If the layer being changed is the same as the last layer we
   * flushed to the corresponding texture unit then we keep a track of
   * the changes so we can try to minimize redundant OpenGL calls if
   * the same layer is flushed again.
   */
  unit = _cogl_get_texture_unit (_cogl_pipeline_layer_get_unit_index (layer));
  if (unit->layer == layer)
    unit->layer_changes_since_flush |= change;

init_layer_state:

  if (required_owner)
    required_owner->age++;

  if (change & COGL_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE &&
      !layer->has_big_state)
    {
      layer->big_state = g_slice_new (CoglPipelineLayerBigState);
      layer->has_big_state = TRUE;
    }

  /* Note: conceptually we have just been notified that a single
   * property value is about to change, but since some state-groups
   * contain multiple properties and 'layer' is about to take over
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
  if (change & COGL_PIPELINE_LAYER_STATE_ALL_SPARSE &&
      !(layer->differences & change))
    {
      _cogl_pipeline_layer_init_multi_property_sparse_state (layer, change);
      layer->differences |= change;
    }

  return layer;
}

static void
_cogl_pipeline_layer_unparent (CoglNode *layer)
{
  /* Chain up */
  _cogl_pipeline_node_unparent_real (layer);
}

static void
_cogl_pipeline_layer_set_parent (CoglPipelineLayer *layer,
                                 CoglPipelineLayer *parent)
{
  /* Chain up */
  _cogl_pipeline_node_set_parent_real (COGL_NODE (layer),
                                       COGL_NODE (parent),
                                       _cogl_pipeline_layer_unparent,
                                       TRUE);
}

CoglPipelineLayer *
_cogl_pipeline_layer_copy (CoglPipelineLayer *src)
{
  CoglPipelineLayer *layer = g_slice_new (CoglPipelineLayer);

  _cogl_pipeline_node_init (COGL_NODE (layer));

  layer->owner = NULL;
  layer->index = src->index;
  layer->differences = 0;
  layer->has_big_state = FALSE;

  _cogl_pipeline_layer_set_parent (layer, src);

  return _cogl_pipeline_layer_object_new (layer);
}

/* XXX: This is duplicated logic; the same as for
 * _cogl_pipeline_prune_redundant_ancestry it would be nice to find a
 * way to consolidate these functions! */
void
_cogl_pipeline_layer_prune_redundant_ancestry (CoglPipelineLayer *layer)
{
  CoglPipelineLayer *new_parent = _cogl_pipeline_layer_get_parent (layer);

  /* walk up past ancestors that are now redundant and potentially
   * reparent the layer. */
  while (_cogl_pipeline_layer_get_parent (new_parent) &&
         (new_parent->differences | layer->differences) ==
         layer->differences)
    new_parent = _cogl_pipeline_layer_get_parent (new_parent);

  _cogl_pipeline_layer_set_parent (layer, new_parent);
}

/* Determine the mask of differences between two layers.
 *
 * XXX: If layers and pipelines could both be cast to a common Tree
 * type of some kind then we could have a unified
 * compare_differences() function.
 */
unsigned long
_cogl_pipeline_layer_compare_differences (CoglPipelineLayer *layer0,
                                          CoglPipelineLayer *layer1)
{
  GSList *head0 = NULL;
  GSList *head1 = NULL;
  CoglPipelineLayer *node0;
  CoglPipelineLayer *node1;
  int len0 = 0;
  int len1 = 0;
  int count;
  GSList *common_ancestor0;
  GSList *common_ancestor1;
  unsigned long layers_difference = 0;

  /* Algorithm:
   *
   * 1) Walk the ancestors of each layer to the root node, adding a
   *    pointer to each ancester node to two linked lists
   *
   * 2) Compare the lists to find the nodes where they start to
   *    differ marking the common_ancestor node for each list.
   *
   * 3) For each list now iterate starting after the common_ancestor
   *    nodes ORing each nodes ->difference mask into the final
   *    differences mask.
   */

  for (node0 = layer0; node0; node0 = _cogl_pipeline_layer_get_parent (node0))
    {
      GSList *link = alloca (sizeof (GSList));
      link->next = head0;
      link->data = node0;
      head0 = link;
      len0++;
    }
  for (node1 = layer1; node1; node1 = _cogl_pipeline_layer_get_parent (node1))
    {
      GSList *link = alloca (sizeof (GSList));
      link->next = head1;
      link->data = node1;
      head1 = link;
      len1++;
    }

  /* NB: There's no point looking at the head entries since we know both layers
   * must have the same default layer as their root node. */
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
      layers_difference |= node0->differences;
    }
  for (head1 = common_ancestor1->next; head1; head1 = head1->next)
    {
      node1 = head1->data;
      layers_difference |= node1->differences;
    }

  return layers_difference;
}

static CoglBool
layer_state_equal (CoglPipelineLayerStateIndex state_index,
                   CoglPipelineLayer **authorities0,
                   CoglPipelineLayer **authorities1,
                   CoglPipelineLayerStateComparitor comparitor)
{
  return comparitor (authorities0[state_index], authorities1[state_index]);
}

void
_cogl_pipeline_layer_resolve_authorities (CoglPipelineLayer *layer,
                                          unsigned long differences,
                                          CoglPipelineLayer **authorities)
{
  unsigned long remaining = differences;
  CoglPipelineLayer *authority = layer;

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
  while ((authority = _cogl_pipeline_layer_get_parent (authority)));

  g_assert (remaining == 0);
}

CoglBool
_cogl_pipeline_layer_equal (CoglPipelineLayer *layer0,
                            CoglPipelineLayer *layer1,
                            unsigned long differences_mask,
                            CoglPipelineEvalFlags flags)
{
  unsigned long layers_difference;
  CoglPipelineLayer *authorities0[COGL_PIPELINE_LAYER_STATE_SPARSE_COUNT];
  CoglPipelineLayer *authorities1[COGL_PIPELINE_LAYER_STATE_SPARSE_COUNT];

  if (layer0 == layer1)
    return TRUE;

  layers_difference =
    _cogl_pipeline_layer_compare_differences (layer0, layer1);

  /* Only compare the sparse state groups requested by the caller... */
  layers_difference &= differences_mask;

  _cogl_pipeline_layer_resolve_authorities (layer0,
                                            layers_difference,
                                            authorities0);
  _cogl_pipeline_layer_resolve_authorities (layer1,
                                            layers_difference,
                                            authorities1);

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE)
    {
      CoglPipelineLayerStateIndex state_index =
        COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE_INDEX;
      if (!_cogl_pipeline_layer_texture_type_equal (authorities0[state_index],
                                                    authorities1[state_index],
                                                    flags))
        return FALSE;
    }

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA)
    {
      CoglPipelineLayerStateIndex state_index =
        COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA_INDEX;
      if (!_cogl_pipeline_layer_texture_data_equal (authorities0[state_index],
                                                    authorities1[state_index],
                                                    flags))
        return FALSE;
    }

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_COMBINE &&
      !layer_state_equal (COGL_PIPELINE_LAYER_STATE_COMBINE_INDEX,
                          authorities0, authorities1,
                          _cogl_pipeline_layer_combine_state_equal))
    return FALSE;

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT &&
      !layer_state_equal (COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT_INDEX,
                          authorities0, authorities1,
                          _cogl_pipeline_layer_combine_constant_equal))
    return FALSE;

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_SAMPLER &&
      !layer_state_equal (COGL_PIPELINE_LAYER_STATE_SAMPLER_INDEX,
                          authorities0, authorities1,
                          _cogl_pipeline_layer_sampler_equal))
    return FALSE;

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_USER_MATRIX &&
      !layer_state_equal (COGL_PIPELINE_LAYER_STATE_USER_MATRIX_INDEX,
                          authorities0, authorities1,
                          _cogl_pipeline_layer_user_matrix_equal))
    return FALSE;

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS &&
      !layer_state_equal (COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS_INDEX,
                          authorities0, authorities1,
                          _cogl_pipeline_layer_point_sprite_coords_equal))
    return FALSE;

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS &&
      !layer_state_equal (COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS_INDEX,
                          authorities0, authorities1,
                          _cogl_pipeline_layer_vertex_snippets_equal))
    return FALSE;

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS &&
      !layer_state_equal (COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS_INDEX,
                          authorities0, authorities1,
                          _cogl_pipeline_layer_fragment_snippets_equal))
    return FALSE;

  return TRUE;
}

static void
_cogl_pipeline_layer_free (CoglPipelineLayer *layer)
{
  _cogl_pipeline_layer_unparent (COGL_NODE (layer));

  if (layer->differences & COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA &&
      layer->texture != NULL)
    cogl_object_unref (layer->texture);

  if (layer->differences & COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS)
    _cogl_pipeline_snippet_list_free (&layer->big_state->vertex_snippets);

  if (layer->differences & COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS)
    _cogl_pipeline_snippet_list_free (&layer->big_state->fragment_snippets);

  if (layer->differences & COGL_PIPELINE_LAYER_STATE_NEEDS_BIG_STATE)
    g_slice_free (CoglPipelineLayerBigState, layer->big_state);

  g_slice_free (CoglPipelineLayer, layer);
}

void
_cogl_pipeline_init_default_layers (void)
{
  CoglPipelineLayer *layer = g_slice_new0 (CoglPipelineLayer);
  CoglPipelineLayerBigState *big_state =
    g_slice_new0 (CoglPipelineLayerBigState);
  CoglPipelineLayer *new;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_pipeline_node_init (COGL_NODE (layer));

  layer->index = 0;

  layer->differences = COGL_PIPELINE_LAYER_STATE_ALL_SPARSE;

  layer->unit_index = 0;

  layer->texture = NULL;
  layer->texture_type = COGL_TEXTURE_TYPE_2D;

  layer->sampler_cache_entry =
    _cogl_sampler_cache_get_default_entry (ctx->sampler_cache);

  layer->big_state = big_state;
  layer->has_big_state = TRUE;

  /* Choose the same default combine mode as OpenGL:
   * RGBA = MODULATE(PREVIOUS[RGBA],TEXTURE[RGBA]) */
  big_state->texture_combine_rgb_func =
    COGL_PIPELINE_COMBINE_FUNC_MODULATE;
  big_state->texture_combine_rgb_src[0] =
    COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS;
  big_state->texture_combine_rgb_src[1] =
    COGL_PIPELINE_COMBINE_SOURCE_TEXTURE;
  big_state->texture_combine_rgb_op[0] =
    COGL_PIPELINE_COMBINE_OP_SRC_COLOR;
  big_state->texture_combine_rgb_op[1] =
    COGL_PIPELINE_COMBINE_OP_SRC_COLOR;
  big_state->texture_combine_alpha_func =
    COGL_PIPELINE_COMBINE_FUNC_MODULATE;
  big_state->texture_combine_alpha_src[0] =
    COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS;
  big_state->texture_combine_alpha_src[1] =
    COGL_PIPELINE_COMBINE_SOURCE_TEXTURE;
  big_state->texture_combine_alpha_op[0] =
    COGL_PIPELINE_COMBINE_OP_SRC_ALPHA;
  big_state->texture_combine_alpha_op[1] =
    COGL_PIPELINE_COMBINE_OP_SRC_ALPHA;

  big_state->point_sprite_coords = FALSE;

  cogl_matrix_init_identity (&big_state->matrix);

  ctx->default_layer_0 = _cogl_pipeline_layer_object_new (layer);

  /* TODO: we should make default_layer_n comprise of two
   * descendants of default_layer_0:
   * - the first descendant should change the texture combine
   *   to what we expect is most commonly used for multitexturing
   * - the second should revert the above change.
   *
   * why? the documentation for how a new layer is initialized
   * doesn't say that layers > 0 have different defaults so unless
   * we change the documentation we can't use different defaults,
   * but if the user does what we expect and changes the
   * texture combine then we can revert the authority to the
   * first descendant which means we can maximize the number
   * of layers with a common ancestor.
   *
   * The main problem will be that we'll need to disable the
   * optimizations for flattening the ancestry when we make
   * the second descendant which reverts the state.
   */
  ctx->default_layer_n = _cogl_pipeline_layer_copy (layer);
  new = _cogl_pipeline_set_layer_unit (NULL, ctx->default_layer_n, 1);
  g_assert (new == ctx->default_layer_n);
  /* Since we passed a newly allocated layer we don't expect that
   * _set_layer_unit() will have to allocate *another* layer. */

  /* Finally we create a dummy dependant for ->default_layer_n which
   * effectively ensures that ->default_layer_n and ->default_layer_0
   * remain immutable.
   */
  ctx->dummy_layer_dependant =
    _cogl_pipeline_layer_copy (ctx->default_layer_n);
}

void
_cogl_pipeline_layer_pre_paint (CoglPipelineLayer *layer)
{
  CoglPipelineLayer *texture_authority;

  texture_authority =
    _cogl_pipeline_layer_get_authority (layer,
                                        COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA);

  if (texture_authority->texture != NULL)
    {
      CoglTexturePrePaintFlags flags = 0;
      CoglPipelineFilter min_filter;
      CoglPipelineFilter mag_filter;

      _cogl_pipeline_layer_get_filters (layer, &min_filter, &mag_filter);

      if (min_filter == COGL_PIPELINE_FILTER_NEAREST_MIPMAP_NEAREST
          || min_filter == COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST
          || min_filter == COGL_PIPELINE_FILTER_NEAREST_MIPMAP_LINEAR
          || min_filter == COGL_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR)
        flags |= COGL_TEXTURE_NEEDS_MIPMAP;

      _cogl_texture_pre_paint (texture_authority->texture, flags);
    }
}

/* Determines if we need to handle the RGB and A texture combining
 * separately or is the same function used for both channel masks and
 * with the same arguments...
 */
CoglBool
_cogl_pipeline_layer_needs_combine_separate
                                       (CoglPipelineLayer *combine_authority)
{
  CoglPipelineLayerBigState *big_state = combine_authority->big_state;
  int n_args;
  int i;

  if (big_state->texture_combine_rgb_func !=
      big_state->texture_combine_alpha_func)
    return TRUE;

  n_args = _cogl_get_n_args_for_combine_func (big_state->texture_combine_rgb_func);

  for (i = 0; i < n_args; i++)
    {
      if (big_state->texture_combine_rgb_src[i] !=
          big_state->texture_combine_alpha_src[i])
        return TRUE;

      /*
       * We can allow some variation of the source operands without
       * needing a separation...
       *
       * "A = REPLACE (CONSTANT[A])" + either of the following...
       * "RGB = REPLACE (CONSTANT[RGB])"
       * "RGB = REPLACE (CONSTANT[A])"
       *
       * can be combined as:
       * "RGBA = REPLACE (CONSTANT)" or
       * "RGBA = REPLACE (CONSTANT[A])" or
       *
       * And "A = REPLACE (1-CONSTANT[A])" + either of the following...
       * "RGB = REPLACE (1-CONSTANT)" or
       * "RGB = REPLACE (1-CONSTANT[A])"
       *
       * can be combined as:
       * "RGBA = REPLACE (1-CONSTANT)" or
       * "RGBA = REPLACE (1-CONSTANT[A])"
       */
      switch (big_state->texture_combine_alpha_op[i])
        {
        case GL_SRC_ALPHA:
          switch (big_state->texture_combine_rgb_op[i])
            {
            case GL_SRC_COLOR:
            case GL_SRC_ALPHA:
              break;
            default:
              return FALSE;
            }
          break;
        case GL_ONE_MINUS_SRC_ALPHA:
          switch (big_state->texture_combine_rgb_op[i])
            {
            case GL_ONE_MINUS_SRC_COLOR:
            case GL_ONE_MINUS_SRC_ALPHA:
              break;
            default:
              return FALSE;
            }
          break;
        default:
          return FALSE;	/* impossible */
        }
    }

   return FALSE;
}


