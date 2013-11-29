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
                              int *layer_number)
{
  name = name + 5; /* skip "cogl_" */

  *normalized = FALSE;
  *layer_number = 0;

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
      *layer_number = strtoul (name + 9, &endptr, 10);
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
  else if (strcmp (name, "point_size_in") == 0)
    *name_id = COGL_ATTRIBUTE_NAME_ID_POINT_SIZE_ARRAY;
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
                                         &name_state->layer_number))
        goto error;
    }
  else
    {
      name_state->name_id = COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY;
      name_state->normalized_default = FALSE;
      name_state->layer_number = 0;
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

static CoglBool
validate_n_components (const CoglAttributeNameState *name_state,
                       int n_components)
{
  switch (name_state->name_id)
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
    case COGL_ATTRIBUTE_NAME_ID_POINT_SIZE_ARRAY:
      if (G_UNLIKELY (n_components != 1))
        {
          g_critical ("The point size attribute can only have one "
                      "component");
          return FALSE;
        }
      break;
    case COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY:
      return TRUE;
    }

  return TRUE;
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
  CoglBuffer *buffer = COGL_BUFFER (attribute_buffer);
  CoglContext *ctx = buffer->context;

  attribute->is_buffered = TRUE;

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

  attribute->d.buffered.attribute_buffer = cogl_object_ref (attribute_buffer);
  attribute->d.buffered.stride = stride;
  attribute->d.buffered.offset = offset;
  attribute->d.buffered.n_components = n_components;
  attribute->d.buffered.type = type;

  attribute->immutable_ref = 0;

  if (attribute->name_state->name_id != COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY)
    {
      if (!validate_n_components (attribute->name_state, n_components))
        return NULL;
      attribute->normalized =
        attribute->name_state->normalized_default;
    }
  else
    attribute->normalized = FALSE;

  return _cogl_attribute_object_new (attribute);

error:
  _cogl_attribute_free (attribute);
  return NULL;
}

static CoglAttribute *
_cogl_attribute_new_const (CoglContext *context,
                           const char *name,
                           int n_components,
                           int n_columns,
                           CoglBool transpose,
                           const float *value)
{
  CoglAttribute *attribute = g_slice_new (CoglAttribute);

  attribute->name_state =
    g_hash_table_lookup (context->attribute_name_states_hash, name);
  if (!attribute->name_state)
    {
      CoglAttributeNameState *name_state =
        _cogl_attribute_register_attribute_name (context, name);
      if (!name_state)
        goto error;
      attribute->name_state = name_state;
    }

  if (!validate_n_components (attribute->name_state, n_components))
    goto error;

  attribute->is_buffered = FALSE;
  attribute->normalized = FALSE;

  attribute->d.constant.context = cogl_object_ref (context);

  attribute->d.constant.boxed.v.array = NULL;

  if (n_columns == 1)
    {
      _cogl_boxed_value_set_float (&attribute->d.constant.boxed,
                                   n_components,
                                   1,
                                   value);
    }
  else
    {
      /* FIXME: Up until GL[ES] 3 only square matrices were supported
       * and we don't currently expose non-square matrices in Cogl.
       */
      _COGL_RETURN_VAL_IF_FAIL (n_columns == n_components, NULL);
      _cogl_boxed_value_set_matrix (&attribute->d.constant.boxed,
                                    n_columns,
                                    1,
                                    transpose,
                                    value);
    }

  return _cogl_attribute_object_new (attribute);

error:
  _cogl_attribute_free (attribute);
  return NULL;
}

CoglAttribute *
cogl_attribute_new_const_1f (CoglContext *context,
                             const char *name,
                             float value)
{
  return _cogl_attribute_new_const (context,
                                    name,
                                    1, /* n_components */
                                    1, /* 1 column vector */
                                    FALSE, /* no transpose */
                                    &value);
}

CoglAttribute *
cogl_attribute_new_const_2fv (CoglContext *context,
                              const char *name,
                              const float *value)
{
  return _cogl_attribute_new_const (context,
                                    name,
                                    2, /* n_components */
                                    1, /* 1 column vector */
                                    FALSE, /* no transpose */
                                    value);
}

CoglAttribute *
cogl_attribute_new_const_3fv (CoglContext *context,
                              const char *name,
                              const float *value)
{
  return _cogl_attribute_new_const (context,
                                    name,
                                    3, /* n_components */
                                    1, /* 1 column vector */
                                    FALSE, /* no transpose */
                                    value);
}

CoglAttribute *
cogl_attribute_new_const_4fv (CoglContext *context,
                              const char *name,
                              const float *value)
{
  return _cogl_attribute_new_const (context,
                                    name,
                                    4, /* n_components */
                                    1, /* 1 column vector */
                                    FALSE, /* no transpose */
                                    value);
}

CoglAttribute *
cogl_attribute_new_const_2f (CoglContext *context,
                             const char *name,
                             float component0,
                             float component1)
{
  float vec2[2] = { component0, component1 };
  return _cogl_attribute_new_const (context,
                                    name,
                                    2, /* n_components */
                                    1, /* 1 column vector */
                                    FALSE, /* no transpose */
                                    vec2);
}

CoglAttribute *
cogl_attribute_new_const_3f (CoglContext *context,
                             const char *name,
                             float component0,
                             float component1,
                             float component2)
{
  float vec3[3] = { component0, component1, component2 };
  return _cogl_attribute_new_const (context,
                                    name,
                                    3, /* n_components */
                                    1, /* 1 column vector */
                                    FALSE, /* no transpose */
                                    vec3);
}

CoglAttribute *
cogl_attribute_new_const_4f (CoglContext *context,
                             const char *name,
                             float component0,
                             float component1,
                             float component2,
                             float component3)
{
  float vec4[4] = { component0, component1, component2, component3 };
  return _cogl_attribute_new_const (context,
                                    name,
                                    4, /* n_components */
                                    1, /* 1 column vector */
                                    FALSE, /* no transpose */
                                    vec4);
}

CoglAttribute *
cogl_attribute_new_const_2x2fv (CoglContext *context,
                                const char *name,
                                const float *matrix2x2,
                                CoglBool transpose)
{
  return _cogl_attribute_new_const (context,
                                    name,
                                    2, /* n_components */
                                    2, /* 2 column vector */
                                    FALSE, /* no transpose */
                                    matrix2x2);
}

CoglAttribute *
cogl_attribute_new_const_3x3fv (CoglContext *context,
                                const char *name,
                                const float *matrix3x3,
                                CoglBool transpose)
{
  return _cogl_attribute_new_const (context,
                                    name,
                                    3, /* n_components */
                                    3, /* 3 column vector */
                                    FALSE, /* no transpose */
                                    matrix3x3);
}

CoglAttribute *
cogl_attribute_new_const_4x4fv (CoglContext *context,
                                const char *name,
                                const float *matrix4x4,
                                CoglBool transpose)
{
  return _cogl_attribute_new_const (context,
                                    name,
                                    4, /* n_components */
                                    4, /* 4 column vector */
                                    FALSE, /* no transpose */
                                    matrix4x4);
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
  _COGL_RETURN_VAL_IF_FAIL (attribute->is_buffered, NULL);

  return attribute->d.buffered.attribute_buffer;
}

void
cogl_attribute_set_buffer (CoglAttribute *attribute,
                           CoglAttributeBuffer *attribute_buffer)
{
  _COGL_RETURN_IF_FAIL (cogl_is_attribute (attribute));
  _COGL_RETURN_IF_FAIL (attribute->is_buffered);

  if (G_UNLIKELY (attribute->immutable_ref))
    warn_about_midscene_changes ();

  cogl_object_ref (attribute_buffer);

  cogl_object_unref (attribute->d.buffered.attribute_buffer);
  attribute->d.buffered.attribute_buffer = attribute_buffer;
}

CoglAttribute *
_cogl_attribute_immutable_ref (CoglAttribute *attribute)
{
  CoglBuffer *buffer = COGL_BUFFER (attribute->d.buffered.attribute_buffer);

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_attribute (attribute), NULL);

  attribute->immutable_ref++;
  _cogl_buffer_immutable_ref (buffer);
  return attribute;
}

void
_cogl_attribute_immutable_unref (CoglAttribute *attribute)
{
  CoglBuffer *buffer = COGL_BUFFER (attribute->d.buffered.attribute_buffer);

  _COGL_RETURN_IF_FAIL (cogl_is_attribute (attribute));
  _COGL_RETURN_IF_FAIL (attribute->immutable_ref > 0);

  attribute->immutable_ref--;
  _cogl_buffer_immutable_unref (buffer);
}

static void
_cogl_attribute_free (CoglAttribute *attribute)
{
  if (attribute->is_buffered)
    cogl_object_unref (attribute->d.buffered.attribute_buffer);
  else
    _cogl_boxed_value_destroy (&attribute->d.constant.boxed);

  g_slice_free (CoglAttribute, attribute);
}

static CoglBool
validate_layer_cb (CoglPipeline *pipeline,
                   int layer_index,
                   void *user_data)
{
  CoglTexture *texture =
    cogl_pipeline_get_layer_texture (pipeline, layer_index);
  CoglFlushLayerState *state = user_data;
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

void
_cogl_flush_attributes_state (CoglFramebuffer *framebuffer,
                              CoglPipeline *pipeline,
                              CoglDrawFlags flags,
                              CoglAttribute **attributes,
                              int n_attributes)
{
  CoglContext *ctx = framebuffer->context;
  CoglFlushLayerState layers_state;
  CoglPipeline *copy = NULL;

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
  _cogl_framebuffer_mark_mid_scene (framebuffer);
  _cogl_framebuffer_mark_clear_clip_dirty (framebuffer);

  if (G_UNLIKELY (!(flags & COGL_DRAW_SKIP_LEGACY_STATE)) &&
      G_UNLIKELY (ctx->legacy_state_set) &&
      _cogl_get_enable_legacy_state ())
    {
      copy = cogl_pipeline_copy (pipeline);
      pipeline = copy;
      _cogl_pipeline_apply_legacy_state (pipeline);
    }

  ctx->driver_vtable->flush_attributes_state (framebuffer,
                                              pipeline,
                                              &layers_state,
                                              flags,
                                              attributes,
                                              n_attributes);

  if (copy)
    cogl_object_unref (copy);
}

int
_cogl_attribute_get_n_components (CoglAttribute *attribute)
{
  if (attribute->is_buffered)
    return attribute->d.buffered.n_components;
  else
    return attribute->d.constant.boxed.size;
}
