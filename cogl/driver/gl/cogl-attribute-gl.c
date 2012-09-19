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

  _COGL_RETURN_VAL_IF_FAIL (context->driver == COGL_DRIVER_GL ||
                            context->driver == COGL_DRIVER_GLES1,
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

  _COGL_RETURN_VAL_IF_FAIL (context->driver == COGL_DRIVER_GL ||
                            context->driver == COGL_DRIVER_GLES1,
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
setup_generic_attribute (CoglContext *context,
                         CoglPipeline *pipeline,
                         CoglAttribute *attribute,
                         uint8_t *base)
{
  int name_index = attribute->name_state->name_index;
  int attrib_location =
    _cogl_pipeline_progend_glsl_get_attrib_location (pipeline, name_index);
  if (attrib_location != -1)
    {
      GE( context, glVertexAttribPointer (attrib_location,
                                          attribute->n_components,
                                          attribute->type,
                                          attribute->normalized,
                                          attribute->stride,
                                          base + attribute->offset) );
      _cogl_bitmask_set (&context->enable_custom_attributes_tmp,
                         attrib_location, TRUE);
    }
}

#endif /* COGL_PIPELINE_PROGEND_GLSL */

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
  CoglBool skip_gl_color = FALSE;
  CoglPipeline *copy = NULL;
  int n_tex_coord_attribs = 0;

  /* Iterate the attributes to work out whether blending needs to be
     enabled and how many texture coords there are. We need to do this
     before flushing the pipeline. */
  for (i = 0; i < n_attributes; i++)
    switch (attributes[i]->name_state->name_id)
      {
      case COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY:
        if ((flags & COGL_DRAW_COLOR_ATTRIBUTE_IS_OPAQUE) == 0 &&
            !_cogl_pipeline_get_real_blend_enabled (pipeline))
          {
            CoglPipelineBlendEnable blend_enable =
              COGL_PIPELINE_BLEND_ENABLE_ENABLED;
            copy = cogl_pipeline_copy (pipeline);
            _cogl_pipeline_set_blend_enabled (copy, blend_enable);
            pipeline = copy;
          }
        skip_gl_color = TRUE;
        break;

      case COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY:
        n_tex_coord_attribs++;
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

  _cogl_pipeline_flush_gl_state (pipeline,
                                 framebuffer,
                                 skip_gl_color,
                                 n_tex_coord_attribs);

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

      attribute_buffer = cogl_attribute_get_buffer (attribute);
      buffer = COGL_BUFFER (attribute_buffer);
      base = _cogl_buffer_gl_bind (buffer, COGL_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER);

      switch (attribute->name_state->name_id)
        {
        case COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY:
#ifdef HAVE_COGL_GLES2
          if (ctx->driver == COGL_DRIVER_GLES2)
            setup_generic_attribute (ctx, pipeline, attribute, base);
          else
#endif
            {
              _cogl_bitmask_set (&ctx->enable_builtin_attributes_tmp,
                                 COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY, TRUE);
              GE (ctx, glColorPointer (attribute->n_components,
                                       attribute->type,
                                       attribute->stride,
                                       base + attribute->offset));
            }
          break;
        case COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY:
#ifdef HAVE_COGL_GLES2
          if (ctx->driver == COGL_DRIVER_GLES2)
            setup_generic_attribute (ctx, pipeline, attribute, base);
          else
#endif
            {
              _cogl_bitmask_set (&ctx->enable_builtin_attributes_tmp,
                                 COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY, TRUE);
              GE (ctx, glNormalPointer (attribute->type,
                                        attribute->stride,
                                        base + attribute->offset));
            }
          break;
        case COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY:
#ifdef HAVE_COGL_GLES2
          if (ctx->driver == COGL_DRIVER_GLES2)
            setup_generic_attribute (ctx, pipeline, attribute, base);
          else
#endif
            {
              _cogl_bitmask_set (&ctx->enable_texcoord_attributes_tmp,
                                 attribute->name_state->texture_unit, TRUE);
              GE (ctx,
                  glClientActiveTexture (GL_TEXTURE0 +
                                         attribute->name_state->texture_unit));
              GE (ctx, glTexCoordPointer (attribute->n_components,
                                          attribute->type,
                                          attribute->stride,
                                          base + attribute->offset));
            }
          break;
        case COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY:
#ifdef HAVE_COGL_GLES2
          if (ctx->driver == COGL_DRIVER_GLES2)
            setup_generic_attribute (ctx, pipeline, attribute, base);
          else
#endif
            {
              _cogl_bitmask_set (&ctx->enable_builtin_attributes_tmp,
                                 COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY, TRUE);
              GE (ctx, glVertexPointer (attribute->n_components,
                                        attribute->type,
                                        attribute->stride,
                                        base + attribute->offset));
            }
          break;
        case COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY:
#ifdef COGL_PIPELINE_PROGEND_GLSL
          if (ctx->driver != COGL_DRIVER_GLES1)
            setup_generic_attribute (ctx, pipeline, attribute, base);
#endif
          break;
        default:
          g_warning ("Unrecognised attribute type 0x%08x", attribute->type);
        }

      _cogl_buffer_gl_unbind (buffer);
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
