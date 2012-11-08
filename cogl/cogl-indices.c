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
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-object-private.h"
#include "cogl-context-private.h"
#include "cogl-indices.h"
#include "cogl-indices-private.h"
#include "cogl-index-buffer.h"

#include <stdarg.h>

static void _cogl_indices_free (CoglIndices *indices);

COGL_OBJECT_DEFINE (Indices, indices);

static size_t
sizeof_indices_type (CoglIndicesType type)
{
  switch (type)
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      return 1;
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      return 2;
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      return 4;
    }
  g_return_val_if_reached (0);
}

CoglIndices *
cogl_indices_new_for_buffer (CoglIndicesType type,
                             CoglIndexBuffer *buffer,
                             size_t offset)
{
  CoglIndices *indices = g_slice_new (CoglIndices);

  indices->buffer = cogl_object_ref (buffer);
  indices->offset = offset;

  indices->type = type;

  indices->immutable_ref = 0;

  return _cogl_indices_object_new (indices);
}

CoglIndices *
cogl_indices_new (CoglContext *context,
                  CoglIndicesType type,
                  const void *indices_data,
                  int n_indices)
{
  size_t buffer_bytes = sizeof_indices_type (type) * n_indices;
  CoglIndexBuffer *index_buffer = cogl_index_buffer_new (context, buffer_bytes);
  CoglBuffer *buffer = COGL_BUFFER (index_buffer);
  CoglIndices *indices;
  CoglError *ignore_error = NULL;

  _cogl_buffer_set_data (buffer,
                         0,
                         indices_data,
                         buffer_bytes,
                         &ignore_error);
  if (ignore_error)
    {
      cogl_error_free (ignore_error);
      cogl_object_unref (index_buffer);
      return NULL;
    }

  indices = cogl_indices_new_for_buffer (type, index_buffer, 0);
  cogl_object_unref (index_buffer);

  return indices;
}

CoglIndexBuffer *
cogl_indices_get_buffer (CoglIndices *indices)
{
  return indices->buffer;
}

CoglIndicesType
cogl_indices_get_type (CoglIndices *indices)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_indices (indices),
                            COGL_INDICES_TYPE_UNSIGNED_BYTE);
  return indices->type;
}

size_t
cogl_indices_get_offset (CoglIndices *indices)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_indices (indices), 0);

  return indices->offset;
}

static void
warn_about_midscene_changes (void)
{
  static CoglBool seen = FALSE;
  if (!seen)
    {
      g_warning ("Mid-scene modification of indices has "
                 "undefined results\n");
      seen = TRUE;
    }
}

void
cogl_indices_set_offset (CoglIndices *indices,
                         size_t offset)
{
  _COGL_RETURN_IF_FAIL (cogl_is_indices (indices));

  if (G_UNLIKELY (indices->immutable_ref))
    warn_about_midscene_changes ();

  indices->offset = offset;
}

static void
_cogl_indices_free (CoglIndices *indices)
{
  cogl_object_unref (indices->buffer);
  g_slice_free (CoglIndices, indices);
}

CoglIndices *
_cogl_indices_immutable_ref (CoglIndices *indices)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_indices (indices), NULL);

  indices->immutable_ref++;
  _cogl_buffer_immutable_ref (COGL_BUFFER (indices->buffer));
  return indices;
}

void
_cogl_indices_immutable_unref (CoglIndices *indices)
{
  _COGL_RETURN_IF_FAIL (cogl_is_indices (indices));
  _COGL_RETURN_IF_FAIL (indices->immutable_ref > 0);

  indices->immutable_ref--;
  _cogl_buffer_immutable_unref (COGL_BUFFER (indices->buffer));
}

CoglIndices *
cogl_get_rectangle_indices (CoglContext *ctx, int n_rectangles)
{
  int n_indices = n_rectangles * 6;

  /* Check if the largest index required will fit in a byte array... */
  if (n_indices <= 256 / 4 * 6)
    {
      /* Generate the byte array if we haven't already */
      if (ctx->rectangle_byte_indices == NULL)
        {
          uint8_t *byte_array = g_malloc (256 / 4 * 6 * sizeof (uint8_t));
          uint8_t *p = byte_array;
          int i, vert_num = 0;

          for (i = 0; i < 256 / 4; i++)
            {
              *(p++) = vert_num + 0;
              *(p++) = vert_num + 1;
              *(p++) = vert_num + 2;
              *(p++) = vert_num + 0;
              *(p++) = vert_num + 2;
              *(p++) = vert_num + 3;
              vert_num += 4;
            }

          ctx->rectangle_byte_indices
            = cogl_indices_new (ctx,
                                COGL_INDICES_TYPE_UNSIGNED_BYTE,
                                byte_array,
                                256 / 4 * 6);

          g_free (byte_array);
        }

      return ctx->rectangle_byte_indices;
    }
  else
    {
      if (ctx->rectangle_short_indices_len < n_indices)
        {
          uint16_t *short_array;
          uint16_t *p;
          int i, vert_num = 0;

          if (ctx->rectangle_short_indices != NULL)
            cogl_object_unref (ctx->rectangle_short_indices);
          /* Pick a power of two >= MAX (512, n_indices) */
          if (ctx->rectangle_short_indices_len == 0)
            ctx->rectangle_short_indices_len = 512;
          while (ctx->rectangle_short_indices_len < n_indices)
            ctx->rectangle_short_indices_len *= 2;

          /* Over-allocate to generate a whole number of quads */
          p = short_array = g_malloc ((ctx->rectangle_short_indices_len
                                       + 5) / 6 * 6
                                      * sizeof (uint16_t));

          /* Fill in the complete quads */
          for (i = 0; i < ctx->rectangle_short_indices_len; i += 6)
            {
              *(p++) = vert_num + 0;
              *(p++) = vert_num + 1;
              *(p++) = vert_num + 2;
              *(p++) = vert_num + 0;
              *(p++) = vert_num + 2;
              *(p++) = vert_num + 3;
              vert_num += 4;
            }

          ctx->rectangle_short_indices
            = cogl_indices_new (ctx,
                                COGL_INDICES_TYPE_UNSIGNED_SHORT,
                                short_array,
                                ctx->rectangle_short_indices_len);

          g_free (short_array);
        }

      return ctx->rectangle_short_indices;
    }
}

