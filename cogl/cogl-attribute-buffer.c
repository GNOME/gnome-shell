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
#include "cogl-attribute-buffer.h"
#include "cogl-attribute-buffer-private.h"
#include "cogl-context-private.h"

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

