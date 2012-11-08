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
#include "cogl-framebuffer.h"
#include "cogl-pipeline-private.h"
#include "cogl-boxed-value.h"

typedef enum
{
  COGL_ATTRIBUTE_NAME_ID_POSITION_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_POINT_SIZE_ARRAY,
  COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY
} CoglAttributeNameID;

typedef struct _CoglAttributeNameState
{
  char *name;
  CoglAttributeNameID name_id;
  int name_index;
  CoglBool normalized_default;
  int layer_number;
} CoglAttributeNameState;

struct _CoglAttribute
{
  CoglObject _parent;

  const CoglAttributeNameState *name_state;
  CoglBool normalized;

  CoglBool is_buffered;

  union {
    struct {
      CoglAttributeBuffer *attribute_buffer;
      size_t stride;
      size_t offset;
      int n_components;
      CoglAttributeType type;
    } buffered;
    struct {
      CoglContext *context;
      CoglBoxedValue boxed;
    } constant;
  } d;

  int immutable_ref;
};

typedef enum
{
  COGL_DRAW_SKIP_JOURNAL_FLUSH = 1 << 0,
  COGL_DRAW_SKIP_PIPELINE_VALIDATION = 1 << 1,
  COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH = 1 << 2,
  COGL_DRAW_SKIP_LEGACY_STATE = 1 << 3,
  /* By default the vertex attribute drawing code will assume that if
     there is a color attribute array enabled then we can't determine
     if the colors will be opaque so we need to enabling
     blending. However when drawing from the journal we know what the
     contents of the color array is so we can override this by passing
     this flag. */
  COGL_DRAW_COLOR_ATTRIBUTE_IS_OPAQUE = 1 << 4,
  /* This forcibly disables the debug option to divert all drawing to
   * wireframes */
  COGL_DRAW_SKIP_DEBUG_WIREFRAME = 1 << 5
} CoglDrawFlags;

/* During CoglContext initialization we register the "cogl_color_in"
 * attribute name so it gets a global name_index of 0. We need to know
 * the name_index for "cogl_color_in" in
 * _cogl_pipeline_flush_gl_state() */
#define COGL_ATTRIBUTE_COLOR_NAME_INDEX 0

CoglAttributeNameState *
_cogl_attribute_register_attribute_name (CoglContext *context,
                                         const char *name);

CoglAttribute *
_cogl_attribute_immutable_ref (CoglAttribute *attribute);

void
_cogl_attribute_immutable_unref (CoglAttribute *attribute);

typedef struct
{
  int unit;
  CoglPipelineFlushOptions options;
  uint32_t fallback_layers;
} CoglFlushLayerState;

void
_cogl_flush_attributes_state (CoglFramebuffer *framebuffer,
                              CoglPipeline *pipeline,
                              CoglDrawFlags flags,
                              CoglAttribute **attributes,
                              int n_attributes);

int
_cogl_attribute_get_n_components (CoglAttribute *attribute);

#endif /* __COGL_ATTRIBUTE_PRIVATE_H */

