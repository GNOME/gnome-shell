/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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

#include "cogl-object-private.h"
#include "cogl-indices.h"
#include "cogl-indices-private.h"
#include "cogl-context-private.h"
#include "cogl-gtype-private.h"

static void _cogl_index_buffer_free (CoglIndexBuffer *indices);

COGL_BUFFER_DEFINE (IndexBuffer, index_buffer);
COGL_GTYPE_DEFINE_CLASS (IndexBuffer, index_buffer);

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

