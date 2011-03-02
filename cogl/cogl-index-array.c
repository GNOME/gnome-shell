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

static void _cogl_index_array_free (CoglIndexArray *indices);

COGL_BUFFER_DEFINE (IndexArray, index_array);

/* XXX: Unlike the wiki design this just takes a size. A single
 * indices buffer should be able to contain multiple ranges of indices
 * which the wiki design doesn't currently consider. */
CoglIndexArray *
cogl_index_array_new (gsize bytes)
{
  CoglIndexArray *indices = g_slice_new (CoglIndexArray);
  gboolean use_malloc;

  if (!cogl_features_available (COGL_FEATURE_VBOS))
    use_malloc = TRUE;
  else
    use_malloc = FALSE;

  /* parent's constructor */
  _cogl_buffer_initialize (COGL_BUFFER (indices),
                           bytes,
                           use_malloc,
                           COGL_BUFFER_BIND_TARGET_INDEX_ARRAY,
                           COGL_BUFFER_USAGE_HINT_INDEX_ARRAY,
                           COGL_BUFFER_UPDATE_HINT_STATIC);

  return _cogl_index_array_object_new (indices);
}

static void
_cogl_index_array_free (CoglIndexArray *indices)
{
  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (indices));

  g_slice_free (CoglIndexArray, indices);
}

gboolean
cogl_index_array_allocate (CoglIndexArray *indices,
                             GError *error)
{
  /* TODO */
  return TRUE;
}

/* XXX: do we want a convenience function like this as an alternative
 * to using cogl_buffer_set_data? The advantage of this is that we can
 * track meta data such as the indices type and max_index_value for a
 * range as part of the indices array. If we just leave people to use
 * cogl_buffer_set_data then we either need a way to specify the type
 * and max index value at draw time or we'll want a separate way to
 * declare the type and max value for a range after uploading the
 * data.
 *
 * XXX: I think in the end it'll be that CoglIndices are to
 * CoglIndexArrays as CoglAttributes are to CoglVertices. I.e
 * a CoglIndexArray is a lite subclass of CoglBuffer that simply
 * implies that the buffer will later be bound as indices but doesn't
 * track more detailed meta data. CoglIndices build on a
 * CoglIndexArray and define the type and max_index_value for some
 * sub-range of a CoglIndexArray.
 *
 * XXX: The double plurel form that "Indices" "Array" implies could be
 * a bit confusing. Also to be a bit more consistent with
 * CoglAttributeBuffer vs CoglAttribute it might be best to rename so
 * we have CoglIndexArray vs CoglIndices? maybe even
 * CoglIndexRange :-/ ?
 *
 * CoglBuffer
 *   CoglAttributeBuffer (buffer sub-class)
 *     CoglAttribute (defines meta data for sub-region of buffer)
 *     CoglPrimitive (object encapsulating a set of attributes)
 *   CoglPixelBuffer (buffer sub-class)
 *   CoglIndexArray (buffer sub-class)
 *     CoglIndices (defines meta data for sub-region of array)
 *
 */
#if 0
void
cogl_index_array_set_data (CoglIndexArray *indices,
                           CoglIndicesType type,
                           int max_index_value,
                           gsize write_offset,
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

