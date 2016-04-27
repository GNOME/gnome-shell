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
#include "cogl-attribute-buffer.h"
#include "cogl-attribute-buffer-private.h"
#include "cogl-context-private.h"
#include "cogl-gtype-private.h"

static void _cogl_attribute_buffer_free (CoglAttributeBuffer *array);

COGL_BUFFER_DEFINE (AttributeBuffer, attribute_buffer);
COGL_GTYPE_DEFINE_CLASS (AttributeBuffer, attribute_buffer);

CoglAttributeBuffer *
cogl_attribute_buffer_new_with_size (CoglContext *context,
                                     size_t bytes)
{
  CoglAttributeBuffer *buffer = g_slice_new (CoglAttributeBuffer);

  /* parent's constructor */
  _cogl_buffer_initialize (COGL_BUFFER (buffer),
                           context,
                           bytes,
                           COGL_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER,
                           COGL_BUFFER_USAGE_HINT_ATTRIBUTE_BUFFER,
                           COGL_BUFFER_UPDATE_HINT_STATIC);

  return _cogl_attribute_buffer_object_new (buffer);
}

CoglAttributeBuffer *
cogl_attribute_buffer_new (CoglContext *context,
                           size_t bytes,
                           const void *data)
{
  CoglAttributeBuffer *buffer;

  buffer = cogl_attribute_buffer_new_with_size (context, bytes);

  /* Note: to keep the common cases simple this API doesn't throw
   * CoglErrors, so developers can assume this function never returns
   * NULL and we will simply abort on error.
   *
   * Developers wanting to catch errors can use
   * cogl_attribute_buffer_new_with_size() and catch errors when later
   * calling cogl_buffer_set_data() or cogl_buffer_map().
   */

  /* XXX: NB: for Cogl 2.0 we don't allow NULL data here but we can't
   * break the api for 1.x and so we keep the check for now. */
  if (data)
    _cogl_buffer_set_data (COGL_BUFFER (buffer),
                           0,
                           data,
                           bytes,
                           NULL);

  return buffer;
}

static void
_cogl_attribute_buffer_free (CoglAttributeBuffer *array)
{
  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (array));

  g_slice_free (CoglAttributeBuffer, array);
}

