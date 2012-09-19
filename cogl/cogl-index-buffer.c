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

#include "cogl-object-private.h"
#include "cogl-indices.h"
#include "cogl-indices-private.h"
#include "cogl-context-private.h"

static void _cogl_index_buffer_free (CoglIndexBuffer *indices);

COGL_BUFFER_DEFINE (IndexBuffer, index_buffer);

/* XXX: Unlike the wiki design this just takes a size. A single
 * indices buffer should be able to contain multiple ranges of indices
 * which the wiki design doesn't currently consider. */
CoglIndexBuffer *
cogl_index_buffer_new (CoglContext *context, size_t bytes)
{
  CoglIndexBuffer *indices = g_slice_new (CoglIndexBuffer);

  /* parent's constructor */
  _cogl_buffer_initialize (COGL_BUFFER (indices),
                           context,
                           bytes,
                           COGL_BUFFER_BIND_TARGET_INDEX_BUFFER,
                           COGL_BUFFER_USAGE_HINT_INDEX_BUFFER,
                           COGL_BUFFER_UPDATE_HINT_STATIC);

  return _cogl_index_buffer_object_new (indices);
}

static void
_cogl_index_buffer_free (CoglIndexBuffer *indices)
{
  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (indices));

  g_slice_free (CoglIndexBuffer, indices);
}

/* XXX: do we want a convenience function like this as an alternative
 * to using cogl_buffer_set_data? The advantage of this is that we can
 * track meta data such as the indices type and max_index_value for a
 * range as part of the indices buffer. If we just leave people to use
 * cogl_buffer_set_data then we either need a way to specify the type
 * and max index value at draw time or we'll want a separate way to
 * declare the type and max value for a range after uploading the
 * data.
 *
 * XXX: I think in the end it'll be that CoglIndices are to
 * CoglIndexBuffers as CoglAttributes are to CoglAttributeBuffers. I.e
 * a CoglIndexBuffer is a lite subclass of CoglBuffer that simply
 * implies that the buffer will later be bound as indices but doesn't
 * track more detailed meta data. CoglIndices build on a
 * CoglIndexBuffer and define the type and max_index_value for some
 * sub-range of a CoglIndexBuffer.
 */
#if 0
void
cogl_index_buffer_set_data (CoglIndexBuffer *indices,
                            CoglIndicesType type,
                            int max_index_value,
                            size_t write_offset,
                            void *user_indices,
                            int n_indices)
{
  GList *l;

  for (l = indices->ranges; l; l = l->next)
    {

    }
  cogl_buffer_set
}
#endif

