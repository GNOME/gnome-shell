/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2011,2012 Intel Corporation.
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
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"
#include "cogl-context-private.h"
#include "cogl-attribute.h"
#include "cogl-attribute-private.h"
#include "cogl-attribute-gl-private.h"
#include "cogl-pipeline-progend-glsl-private.h"
#include "cogl-buffer-gl-private.h"

typedef struct _ForeachChangedBitState
{
  CoglContext *context;
  const CoglBitmask *new_bits;
  CoglPipeline *pipeline;
} ForeachChangedBitState;

static CoglBool
toggle_builtin_attribute_enabled_cb (int bit_num, void *user_data)
{
  ForeachChangedBitState *state = user_data;
  CoglContext *context = state->context;

  _COGL_RETURN_VAL_IF_FAIL ((context->private_feature_flags &
                             COGL_PRIVATE_FEATURE_GL_FIXED),
                            FALSE);

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)
  {
    CoglBool enabled = _cogl_bitmask_get (state->new_bits, bit_num);
    GLenum cap;

    switch (bit_num)
      {
      case COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY:
        cap = GL_COLOR_ARRAY;
        break;
      case COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY:
        cap = GL_VERTEX_ARRAY;
        break;
      case COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY:
        cap = GL_NORMAL_ARRAY;
        break;
      }
    if (enabled)
      GE (context, glEnableClientState (cap));
    else
      GE (context, glDisableClientState (cap));
  }
#endif

  return TRUE;
}

static CoglBool
toggle_texcood_attribute_enabled_cb (int bit_num, void *user_data)
{
  ForeachChangedBitState *state = user_data;
  CoglContext *context = state->context;

  _COGL_RETURN_VAL_IF_FAIL ((context->private_feature_flags &
                             COGL_PRIVATE_FEATURE_GL_FIXED),
                            FALSE);

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)
  {
    CoglBool enabled = _cogl_bitmask_get (state->new_bits, bit_num);

    GE( context, glClientActiveTexture (GL_TEXTURE0 + bit_num) );

    if (enabled)
      GE( context, glEnableClientState (GL_TEXTURE_COORD_ARRAY) );
    else
      GE( context, glDisableClientState (GL_TEXTURE_COORD_ARRAY) );
  }
#endif

  return TRUE;
}

static CoglBool
toggle_custom_attribute_enabled_cb (int bit_num, void *user_data)
{
  ForeachChangedBitState *state = user_data;
  CoglBool enabled = _cogl_bitmask_get (state->new_bits, bit_num);
  CoglContext *context = state->context;

  if (enabled)
    GE( context, glEnableVertexAttribArray (bit_num) );
  else
    GE( context, glDisableVertexAttribArray (bit_num) );

  return TRUE;
}

static void
foreach_changed_bit_and_save (CoglContext *context,
                              CoglBitmask *current_bits,
                              const CoglBitmask *new_bits,
                              CoglBitmaskForeachFunc callback,
                              ForeachChangedBitState *state)
{
  /* Get the list of bits that are different */
  _cogl_bitmask_clear_all (&context->changed_bits_tmp);
  _cogl_bitmask_set_bits (&context->changed_bits_tmp, current_bits);
  _cogl_bitmask_xor_bits (&context->changed_bits_tmp, new_bits);

  /* Iterate over each bit to change */
  state->new_bits = new_bits;
  _cogl_bitmask_foreach (&context->changed_bits_tmp,
                         callback,
                         state);

  /* Store the new values */
  _cogl_bitmask_clear_all (current_bits);
  _cogl_bitmask_set_bits (current_bits, new_bits);
}

#ifdef COGL_PIPELINE_PROGEND_GLSL

static void
setup_generic_buffered_attribute (CoglContext *context,
                                  CoglPipeline *pipeline,
                                  CoglAttribute *attribute,
                                  uint8_t *base)
{
  int name_index = attribute->name_state->name_index;
  int attrib_location =
    _cogl_pipeline_progend_glsl_get_attrib_location (pipeline, name_index);

  if (attrib_location == -1)
    return;

  GE( context, glVertexAttribPointer (attrib_location,
                                      attribute->d.buffered.n_components,
                                      attribute->d.buffered.type,
                                      attribute->normalized,
                                      attribute->d.buffered.stride,
                                      base + attribute->d.buffered.offset) );
  _cogl_bitmask_set (&context->enable_custom_attributes_tmp,
                     attrib_location, TRUE);
}

static void
setup_generic_const_attribute (CoglContext *context,
                               CoglPipeline *pipeline,
                               CoglAttribute *attribute)
{
  int name_index = attribute->name_state->name_index;
  int attrib_location =
    _cogl_pipeline_progend_glsl_get_attrib_location (pipeline, name_index);
  int columns;
  int i;

  if (attrib_location == -1)
    return;

  if (attribute->d.constant.boxed.type == COGL_BOXED_MATRIX)
    columns = attribute->d.constant.boxed.size;
  else
    columns = 1;

  /* Note: it's ok to access a COGL_BOXED_FLOAT as a matrix with only
   * one column... */

  switch (attribute->d.constant.boxed.size)
    {
    case 1:
      GE( context, glVertexAttrib1fv (attrib_location,
                                      attribute->d.constant.boxed.v.matrix));
      break;
    case 2:
      for (i = 0; i < columns; i++)
        GE( context, glVertexAttrib2fv (attrib_location + i,
                                        attribute->d.constant.boxed.v.matrix));
      break;
    case 3:
      for (i = 0; i < columns; i++)
        GE( context, glVertexAttrib3fv (attrib_location + i,
                                        attribute->d.constant.boxed.v.matrix));
      break;
    case 4:
      for (i = 0; i < columns; i++)
        GE( context, glVertexAttrib4fv (attrib_location + i,
                                        attribute->d.constant.boxed.v.matrix));
      break;
    default:
      g_warn_if_reached ();
    }
}

#endif /* COGL_PIPELINE_PROGEND_GLSL */

static void
setup_legacy_buffered_attribute (CoglContext *ctx,
                                 CoglPipeline *pipeline,
                                 CoglAttribute *attribute,
                                 uint8_t *base)
{
  switch (attribute->name_state->name_id)
    {
    case COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY:
      _cogl_bitmask_set (&ctx->enable_builtin_attributes_tmp,
                         COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY, TRUE);
      GE (ctx, glColorPointer (attribute->d.buffered.n_components,
                               attribute->d.buffered.type,
                               attribute->d.buffered.stride,
                               base + attribute->d.buffered.offset));
      break;
    case COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY:
      _cogl_bitmask_set (&ctx->enable_builtin_attributes_tmp,
                         COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY, TRUE);
      GE (ctx, glNormalPointer (attribute->d.buffered.type,
                                attribute->d.buffered.stride,
                                base + attribute->d.buffered.offset));
      break;
    case COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY:
      {
        int layer_number = attribute->name_state->layer_number;
        const CoglPipelineGetLayerFlags flags =
          COGL_PIPELINE_GET_LAYER_NO_CREATE;
        CoglPipelineLayer *layer =
          _cogl_pipeline_get_layer_with_flags (pipeline, layer_number, flags);

        if (layer)
          {
            int unit = _cogl_pipeline_layer_get_unit_index (layer);

            _cogl_bitmask_set (&ctx->enable_texcoord_attributes_tmp,
                               unit,
                               TRUE);

            GE (ctx, glClientActiveTexture (GL_TEXTURE0 + unit));
            GE (ctx, glTexCoordPointer (attribute->d.buffered.n_components,
                                        attribute->d.buffered.type,
                                        attribute->d.buffered.stride,
                                        base + attribute->d.buffered.offset));
          }
        break;
      }
    case COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY:
      _cogl_bitmask_set (&ctx->enable_builtin_attributes_tmp,
                         COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY, TRUE);
      GE (ctx, glVertexPointer (attribute->d.buffered.n_components,
                                attribute->d.buffered.type,
                                attribute->d.buffered.stride,
                                base + attribute->d.buffered.offset));
      break;
    case COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY:
#ifdef COGL_PIPELINE_PROGEND_GLSL
      if (ctx->private_feature_flags & COGL_PRIVATE_FEATURE_GL_PROGRAMMABLE)
        setup_generic_buffered_attribute (ctx, pipeline, attribute, base);
#endif
      break;
    default:
      g_warn_if_reached ();
    }
}

static void
setup_legacy_const_attribute (CoglContext *ctx,
                              CoglPipeline *pipeline,
                              CoglAttribute *attribute)
{
#ifdef COGL_PIPELINE_PROGEND_GLSL
  if (attribute->name_state->name_id == COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY)
    {
      if (ctx->private_feature_flags & COGL_PRIVATE_FEATURE_GL_PROGRAMMABLE)
        setup_generic_const_attribute (ctx, pipeline, attribute);
    }
  else
#endif
    {
      float vector[4] = { 0, 0, 0, 1 };
      float *boxed = attribute->d.constant.boxed.v.float_value;
      int n_components = attribute->d.constant.boxed.size;
      int i;

      for (i = 0; i < n_components; i++)
        vector[i] = boxed[i];

      switch (attribute->name_state->name_id)
        {
        case COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY:
          GE (ctx, glColor4f (vector[0], vector[1], vector[2], vector[3]));
          break;
        case COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY:
          GE (ctx, glNormal3f (vector[0], vector[1], vector[2]));
          break;
        case COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY:
          {
            int layer_number = attribute->name_state->layer_number;
            const CoglPipelineGetLayerFlags flags =
              COGL_PIPELINE_GET_LAYER_NO_CREATE;
            CoglPipelineLayer *layer =
              _cogl_pipeline_get_layer_with_flags (pipeline,
                                                   layer_number,
                                                   flags);

            if (layer)
              {
                int unit = _cogl_pipeline_layer_get_unit_index (layer);

                GE (ctx, glClientActiveTexture (GL_TEXTURE0 + unit));

                GE (ctx, glMultiTexCoord4f (vector[0],
                                            vector[1],
                                            vector[2],
                                            vector[3]));
              }
            break;
          }
        case COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY:
          GE (ctx, glVertex4f (vector[0], vector[1], vector[2], vector[3]));
          break;
        default:
          g_warn_if_reached ();
        }
    }
}

static void
apply_attribute_enable_updates (CoglContext *context,
                                CoglPipeline *pipeline)
{
  ForeachChangedBitState changed_bits_state;

  changed_bits_state.context = context;
  changed_bits_state.new_bits = &context->enable_builtin_attributes_tmp;
  changed_bits_state.pipeline = pipeline;

  foreach_changed_bit_and_save (context,
                                &context->enabled_builtin_attributes,
                                &context->enable_builtin_attributes_tmp,
                                toggle_builtin_attribute_enabled_cb,
                                &changed_bits_state);

  changed_bits_state.new_bits = &context->enable_texcoord_attributes_tmp;
  foreach_changed_bit_and_save (context,
                                &context->enabled_texcoord_attributes,
                                &context->enable_texcoord_attributes_tmp,
                                toggle_texcood_attribute_enabled_cb,
                                &changed_bits_state);

  changed_bits_state.new_bits = &context->enable_custom_attributes_tmp;
  foreach_changed_bit_and_save (context,
                                &context->enabled_custom_attributes,
                                &context->enable_custom_attributes_tmp,
                                toggle_custom_attribute_enabled_cb,
                                &changed_bits_state);
}

void
_cogl_gl_flush_attributes_state (CoglFramebuffer *framebuffer,
                                 CoglPipeline *pipeline,
                                 CoglFlushLayerState *layers_state,
                                 CoglDrawFlags flags,
                                 CoglAttribute **attributes,
                                 int n_attributes)
{
  CoglContext *ctx = framebuffer->context;
  int i;
  CoglBool with_color_attrib = FALSE;
  CoglBool unknown_color_alpha = FALSE;
  CoglPipeline *copy = NULL;

  /* Iterate the attributes to see if we have a color attribute which
   * may affect our decision to enable blending or not.
   *
   * We need to do this before flushing the pipeline. */
  for (i = 0; i < n_attributes; i++)
    switch (attributes[i]->name_state->name_id)
      {
      case COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY:
        if ((flags & COGL_DRAW_COLOR_ATTRIBUTE_IS_OPAQUE) == 0 &&
            _cogl_attribute_get_n_components (attributes[i]) == 4)
          unknown_color_alpha = TRUE;
        with_color_attrib = TRUE;
        break;

      default:
        break;
      }

  if (G_UNLIKELY (layers_state->options.flags))
    {
      /* If we haven't already created a derived pipeline... */
      if (!copy)
        {
          copy = cogl_pipeline_copy (pipeline);
          pipeline = copy;
        }
      _cogl_pipeline_apply_overrides (pipeline, &layers_state->options);

      /* TODO:
       * overrides = cogl_pipeline_get_data (pipeline,
       *                                     last_overrides_key);
       * if (overrides)
       *   {
       *     age = cogl_pipeline_get_age (pipeline);
       *     XXX: actually we also need to check for legacy_state
       *     and blending overrides for use of glColorPointer...
       *     if (overrides->ags != age ||
       *         memcmp (&overrides->options, &options,
       *                 sizeof (options) != 0)
       *       {
       *         cogl_object_unref (overrides->weak_pipeline);
       *         g_slice_free (Overrides, overrides);
       *         overrides = NULL;
       *       }
       *   }
       * if (!overrides)
       *   {
       *     overrides = g_slice_new (Overrides);
       *     overrides->weak_pipeline =
       *       cogl_pipeline_weak_copy (pipeline);
       *     _cogl_pipeline_apply_overrides (overrides->weak_pipeline,
       *                                     &options);
       *
       *     cogl_pipeline_set_data (pipeline, last_overrides_key,
       *                             weak_overrides,
       *                             free_overrides_cb,
       *                             NULL);
       *   }
       * pipeline = overrides->weak_pipeline;
       */
    }

  _cogl_pipeline_flush_gl_state (ctx,
                                 pipeline,
                                 framebuffer,
                                 with_color_attrib,
                                 unknown_color_alpha);

  _cogl_bitmask_clear_all (&ctx->enable_builtin_attributes_tmp);
  _cogl_bitmask_clear_all (&ctx->enable_texcoord_attributes_tmp);
  _cogl_bitmask_clear_all (&ctx->enable_custom_attributes_tmp);

  /* Bind the attribute pointers. We need to do this after the
   * pipeline is flushed because when using GLSL that is the only
   * point when we can determine the attribute locations */

  for (i = 0; i < n_attributes; i++)
    {
      CoglAttribute *attribute = attributes[i];
      CoglAttributeBuffer *attribute_buffer;
      CoglBuffer *buffer;
      uint8_t *base;

      if (attribute->is_buffered)
        {
          attribute_buffer = cogl_attribute_get_buffer (attribute);
          buffer = COGL_BUFFER (attribute_buffer);

          /* Note: we don't try and catch errors with binding buffers
           * here since OOM errors at this point indicate that nothing
           * has yet been uploaded to attribute buffer which we
           * consider to be a programmer error.
           */
          base =
            _cogl_buffer_gl_bind (buffer,
                                  COGL_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER,
                                  NULL);

          if (pipeline->progend == COGL_PIPELINE_PROGEND_GLSL)
            setup_generic_buffered_attribute (ctx, pipeline, attribute, base);
          else
            setup_legacy_buffered_attribute (ctx, pipeline, attribute, base);

          _cogl_buffer_gl_unbind (buffer);
        }
      else
        {
          if (pipeline->progend == COGL_PIPELINE_PROGEND_GLSL)
            setup_generic_const_attribute (ctx, pipeline, attribute);
          else
            setup_legacy_const_attribute (ctx, pipeline, attribute);
        }
    }

  apply_attribute_enable_updates (ctx, pipeline);

  if (copy)
    cogl_object_unref (copy);
}

void
_cogl_gl_disable_all_attributes (CoglContext *ctx)
{
  _cogl_bitmask_clear_all (&ctx->enable_builtin_attributes_tmp);
  _cogl_bitmask_clear_all (&ctx->enable_texcoord_attributes_tmp);
  _cogl_bitmask_clear_all (&ctx->enable_custom_attributes_tmp);

  /* XXX: we can pass a NULL source pipeline here because we know a
   * source pipeline only needs to be referenced when enabling
   * attributes. */
  apply_attribute_enable_updates (ctx, NULL);
}
