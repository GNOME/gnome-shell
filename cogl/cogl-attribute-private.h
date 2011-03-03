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

#ifndef __COGL_ATTRIBUTE_PRIVATE_H
#define __COGL_ATTRIBUTE_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-attribute.h"

typedef enum
{
  COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY
} CoglAttributeNameID;

struct _CoglAttribute
{
  CoglObject _parent;

  CoglAttributeBuffer *attribute_buffer;
  char *name;
  CoglAttributeNameID name_id;
  gsize stride;
  gsize offset;
  int n_components;
  CoglAttributeType type;
  gboolean normalized;
  unsigned int texture_unit;

  int immutable_ref;
};

typedef enum
{
  COGL_DRAW_SKIP_JOURNAL_FLUSH = 1 << 0,
  COGL_DRAW_SKIP_PIPELINE_VALIDATION = 1 << 1,
  COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH = 1 << 2,
  /* When flushing from the journal the logged pipeline will already
     contain the legacy state overrides so we don't want to apply them
     again when we flush the pipeline for drawing */
  COGL_DRAW_SKIP_LEGACY_STATE = 1 << 3,
  /* By default the vertex attribute drawing code will assume that if
     there is a color attribute array enabled then we can't determine
     if the colors will be opaque so we need to enabling
     blending. However when drawing from the journal we know what the
     contents of the color array is so we can override this by passing
     this flag. */
  COGL_DRAW_COLOR_ATTRIBUTE_IS_OPAQUE = 1 << 4
} CoglDrawFlags;

CoglAttribute *
_cogl_attribute_immutable_ref (CoglAttribute *attribute);

void
_cogl_attribute_immutable_unref (CoglAttribute *attribute);

void
_cogl_draw_attributes (CoglVerticesMode mode,
                       int first_vertex,
                       int n_vertices,
                       CoglAttribute **attributes,
                       int n_attributes,
                       CoglDrawFlags flags);

void
_cogl_draw_indexed_attributes (CoglVerticesMode mode,
                               int first_vertex,
                               int n_vertices,
                               CoglIndices *indices,
                               CoglAttribute **attributes,
                               int n_attributes,
                               CoglDrawFlags flags);

void
_cogl_attribute_disable_cached_arrays (void);

#endif /* __COGL_ATTRIBUTE_PRIVATE_H */

