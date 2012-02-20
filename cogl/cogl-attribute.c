/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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

#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-journal-private.h"
#include "cogl-attribute.h"
#include "cogl-attribute-private.h"
#include "cogl-pipeline.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-texture-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-indices-private.h"
#ifdef COGL_PIPELINE_PROGEND_GLSL
#include "cogl-pipeline-progend-glsl-private.h"
#endif
#include "cogl-private.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* This isn't defined in the GLES headers */
#ifndef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT 0x1405
#endif

static void _cogl_attribute_free (CoglAttribute *attribute);

COGL_OBJECT_DEFINE (Attribute, attribute);

static CoglBool
validate_cogl_attribute_name (const char *name,
                              char **real_attribute_name,
                              CoglAttributeNameID *name_id,
                              CoglBool *normalized,
                              int *texture_unit)
{
  name = name + 5; /* skip "cogl_" */

  *normalized = FALSE;
  *texture_unit = 0;

  if (strcmp (name, "position_in") == 0)
    *name_id = COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY;
  else if (strcmp (name, "color_in") == 0)
    {
      *name_id = COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY;
      *normalized = TRUE;
    }
  else if (strcmp (name, "tex_coord_in") == 0)
    {
      *real_attribute_name = "cogl_tex_coord0_in";
      *name_id = COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY;
    }
  else if (strncmp (name, "tex_coord", strlen ("tex_coord")) == 0)
    {
      char *endptr;
      *texture_unit = strtoul (name + 9, &endptr, 10);
      if (strcmp (endptr, "_in") != 0)
	{
	  g_warning ("Texture coordinate attributes should either be named "
                     "\"cogl_tex_coord_in\" or named with a texture unit index "
                     "like \"cogl_tex_coord2_in\"\n");
          return FALSE;
	}
      *name_id = COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY;
    }
  else if (strcmp (name, "normal_in") == 0)
    {
      *name_id = COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY;
      *normalized = TRUE;
    }
  else
    {
      g_warning ("Unknown cogl_* attribute name cogl_%s\n", name);
      return FALSE;
    }

  return TRUE;
}

CoglAttributeNameState *
_cogl_attribute_register_attribute_name (CoglContext *context,
                                         const char *name)
{
  CoglAttributeNameState *name_state = g_new (CoglAttributeNameState, 1);
  int name_index = context->n_attribute_names++;
  char *name_copy = g_strdup (name);

  name_state->name = NULL;
  name_state->name_index = name_index;
  if (strncmp (name, "cogl_", 5) == 0)
    {
      if (!validate_cogl_attribute_name (name,
                                         &name_state->name,
                                         &name_state->name_id,
                                         &name_state->normalized_default,
                                         &name_state->texture_unit))
        goto error;
    }
  else
    {
      name_state->name_id = COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY;
      name_state->normalized_default = FALSE;
      name_state->texture_unit = 0;
    }

  if (name_state->name == NULL)
    name_state->name = name_copy;

  g_hash_table_insert (context->attribute_name_states_hash,
                       name_copy, name_state);

  if (G_UNLIKELY (context->attribute_name_index_map == NULL))
    context->attribute_name_index_map =
      g_array_new (FALSE, FALSE, sizeof (void *));

  g_array_set_size (context->attribute_name_index_map, name_index + 1);

  g_array_index (context->attribute_name_index_map,
                 CoglAttributeNameState *, name_index) = name_state;

  return name_state;

error:
  g_free (name_state);
  return NULL;
}

CoglAttribute *
cogl_attribute_new (CoglAttributeBuffer *attribute_buffer,
                    const char *name,
                    size_t stride,
                    size_t offset,
                    int n_components,
                    CoglAttributeType type)
{
  CoglAttribute *attribute = g_slice_new (CoglAttribute);

  /* FIXME: retrieve the context from the buffer */
  _COGL_GET_CONTEXT (ctx, NULL);

  attribute->name_state =
    g_hash_table_lookup (ctx->attribute_name_states_hash, name);
  if (!attribute->name_state)
    {
      CoglAttributeNameState *name_state =
        _cogl_attribute_register_attribute_name (ctx, name);
      if (!name_state)
        goto error;
      attribute->name_state = name_state;
    }
  attribute->attribute_buffer = cogl_object_ref (attribute_buffer);
  attribute->stride = stride;
  attribute->offset = offset;
  attribute->n_components = n_components;
  attribute->type = type;
  attribute->immutable_ref = 0;

  if (attribute->name_state->name_id != COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY)
    {
      switch (attribute->name_state->name_id)
        {
        case COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY:
          if (G_UNLIKELY (n_components == 1))
            {
              g_critical ("glVertexPointer doesn't allow 1 component vertex "
                          "positions so we currently only support \"cogl_vertex\" "
                          "attributes where n_components == 2, 3 or 4");
              return FALSE;
            }
          break;
        case COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY:
          if (G_UNLIKELY (n_components != 3 && n_components != 4))
            {
              g_critical ("glColorPointer expects 3 or 4 component colors so we "
                          "currently only support \"cogl_color\" attributes where "
                          "n_components == 3 or 4");
              return FALSE;
            }
          break;
        case COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY:
          break;
        case COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY:
          if (G_UNLIKELY (n_components != 3))
            {
              g_critical ("glNormalPointer expects 3 component normals so we "
                          "currently only support \"cogl_normal\" attributes "
                          "where n_components == 3");
              return FALSE;
            }
          break;
        default:
          g_warn_if_reached ();
        }
      attribute->normalized = attribute->name_state->normalized_default;
    }
  else
    attribute->normalized = FALSE;

  return _cogl_attribute_object_new (attribute);

error:
  _cogl_attribute_free (attribute);
  return NULL;
}

CoglBool
cogl_attribute_get_normalized (CoglAttribute *attribute)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_attribute (attribute), FALSE);

  return attribute->normalized;
}

static void
warn_about_midscene_changes (void)
{
  static CoglBool seen = FALSE;
  if (!seen)
    {
      g_warning ("Mid-scene modification of attributes has "
                 "undefined results\n");
      seen = TRUE;
    }
}

void
cogl_attribute_set_normalized (CoglAttribute *attribute,
                                      CoglBool normalized)
{
  _COGL_RETURN_IF_FAIL (cogl_is_attribute (attribute));

  if (G_UNLIKELY (attribute->immutable_ref))
    warn_about_midscene_changes ();

  attribute->normalized = normalized;
}

CoglAttributeBuffer *
cogl_attribute_get_buffer (CoglAttribute *attribute)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_attribute (attribute), NULL);

  return attribute->attribute_buffer;
}

void
cogl_attribute_set_buffer (CoglAttribute *attribute,
                           CoglAttributeBuffer *attribute_buffer)
{
  _COGL_RETURN_IF_FAIL (cogl_is_attribute (attribute));

  if (G_UNLIKELY (attribute->immutable_ref))
    warn_about_midscene_changes ();

  cogl_object_ref (attribute_buffer);

  cogl_object_unref (attribute->attribute_buffer);
  attribute->attribute_buffer = attribute_buffer;
}

CoglAttribute *
_cogl_attribute_immutable_ref (CoglAttribute *attribute)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_attribute (attribute), NULL);

  attribute->immutable_ref++;
  _cogl_buffer_immutable_ref (COGL_BUFFER (attribute->attribute_buffer));
  return attribute;
}

void
_cogl_attribute_immutable_unref (CoglAttribute *attribute)
{
  _COGL_RETURN_IF_FAIL (cogl_is_attribute (attribute));
  _COGL_RETURN_IF_FAIL (attribute->immutable_ref > 0);

  attribute->immutable_ref--;
  _cogl_buffer_immutable_unref (COGL_BUFFER (attribute->attribute_buffer));
}

static void
_cogl_attribute_free (CoglAttribute *attribute)
{
  cogl_object_unref (attribute->attribute_buffer);

  g_slice_free (CoglAttribute, attribute);
}

typedef struct
{
  int unit;
  CoglPipelineFlushOptions options;
  uint32_t fallback_layers;
} ValidateLayerState;

static CoglBool
validate_layer_cb (CoglPipeline *pipeline,
                   int layer_index,
                   void *user_data)
{
  CoglTexture *texture =
    cogl_pipeline_get_layer_texture (pipeline, layer_index);
  ValidateLayerState *state = user_data;
  CoglBool status = TRUE;

  /* invalid textures will be handled correctly in
   * _cogl_pipeline_flush_layers_gl_state */
  if (texture == NULL)
    goto validated;

  _cogl_texture_flush_journal_rendering (texture);

  /* Give the texture a chance to know that we're rendering
     non-quad shaped primitives. If the texture is in an atlas it
     will be migrated */
  _cogl_texture_ensure_non_quad_rendering (texture);

  /* We need to ensure the mipmaps are ready before deciding
   * anything else about the texture because the texture storate
   * could completely change if it needs to be migrated out of the
   * atlas and will affect how we validate the layer.
   */
  _cogl_pipeline_pre_paint_for_layer (pipeline, layer_index);

  if (!_cogl_texture_can_hardware_repeat (texture))
    {
      g_warning ("Disabling layer %d of the current source material, "
                 "because texturing with the vertex buffer API is not "
                 "currently supported using sliced textures, or textures "
                 "with waste\n", layer_index);

      /* XXX: maybe we can add a mechanism for users to forcibly use
       * textures with waste where it would be their responsability to use
       * texture coords in the range [0,1] such that sampling outside isn't
       * required. We can then use a texture matrix (or a modification of
       * the users own matrix) to map 1 to the edge of the texture data.
       *
       * Potentially, given the same guarantee as above we could also
       * support a single sliced layer too. We would have to redraw the
       * vertices once for each layer, each time with a fiddled texture
       * matrix.
       */
      state->fallback_layers |= (1 << state->unit);
      state->options.flags |= COGL_PIPELINE_FLUSH_FALLBACK_MASK;
    }

validated:
  state->unit++;
  return status;
}

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
_cogl_flush_attributes_state (CoglFramebuffer *framebuffer,
                              CoglPipeline *pipeline,
                              CoglDrawFlags flags,
                              CoglAttribute **attributes,
                              int n_attributes)
{
  int i;
  CoglBool skip_gl_color = FALSE;
  CoglPipeline *copy = NULL;
  int n_tex_coord_attribs = 0;
  ValidateLayerState layers_state;
  CoglContext *ctx = framebuffer->context;

  if (!(flags & COGL_DRAW_SKIP_JOURNAL_FLUSH))
    _cogl_journal_flush (framebuffer->journal);

  layers_state.unit = 0;
  layers_state.options.flags = 0;
  layers_state.fallback_layers = 0;

  if (!(flags & COGL_DRAW_SKIP_PIPELINE_VALIDATION))
    cogl_pipeline_foreach_layer (pipeline,
                                 validate_layer_cb,
                                 &layers_state);

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the pipeline state) when flushing the clip stack, so should
   * always be done first when preparing to draw. We need to do this
   * before setting up the array pointers because setting up the clip
   * stack can cause some drawing which would change the array
   * pointers. */
  if (!(flags & COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH))
    _cogl_framebuffer_flush_state (framebuffer,
                                   framebuffer,
                                   COGL_FRAMEBUFFER_STATE_ALL);

  /* In cogl_read_pixels we have a fast-path when reading a single
   * pixel and the scene is just comprised of simple rectangles still
   * in the journal. For this optimization to work we need to track
   * when the framebuffer really does get drawn to. */
  _cogl_framebuffer_dirty (framebuffer);

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

  if (G_UNLIKELY (layers_state.options.flags))
    {
      /* If we haven't already created a derived pipeline... */
      if (!copy)
        {
          copy = cogl_pipeline_copy (pipeline);
          pipeline = copy;
        }
      _cogl_pipeline_apply_overrides (pipeline, &layers_state.options);

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

  if (G_UNLIKELY (!(flags & COGL_DRAW_SKIP_LEGACY_STATE)) &&
      G_UNLIKELY (ctx->legacy_state_set) &&
      _cogl_get_enable_legacy_state ())
    {
      /* If we haven't already created a derived pipeline... */
      if (!copy)
        {
          copy = cogl_pipeline_copy (pipeline);
          pipeline = copy;
        }
      _cogl_pipeline_apply_legacy_state (pipeline);
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
      base = _cogl_buffer_bind (buffer, COGL_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER);

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

      _cogl_buffer_unbind (buffer);
    }

  apply_attribute_enable_updates (ctx, pipeline);

  if (copy)
    cogl_object_unref (copy);
}

void
_cogl_attribute_disable_cached_arrays (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_bitmask_clear_all (&ctx->enable_builtin_attributes_tmp);
  _cogl_bitmask_clear_all (&ctx->enable_texcoord_attributes_tmp);
  _cogl_bitmask_clear_all (&ctx->enable_custom_attributes_tmp);

  /* XXX: we can pass a NULL source pipeline here because we know a
   * source pipeline only needs to be referenced when enabling
   * attributes. */
  apply_attribute_enable_updates (ctx, NULL);
}
