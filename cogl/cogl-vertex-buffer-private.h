/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_VERTEX_BUFFER_H
#define __COGL_VERTEX_BUFFER_H

#include "cogl-object-private.h"

#include "cogl-primitive.h"

#include <glib.h>

/* Note we put quite a bit into the flags here to help keep
 * the down size of the CoglVertexBufferAttrib struct below. */
typedef enum _CoglVertexBufferAttribFlags
{
  /* Types */
  /* NB: update the _TYPE_MASK below if these are changed */
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_COLOR_ARRAY	  = 1<<0,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_NORMAL_ARRAY	  = 1<<1,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_TEXTURE_COORD_ARRAY  = 1<<2,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_VERTEX_ARRAY	  = 1<<3,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_CUSTOM_ARRAY	  = 1<<4,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_INVALID	  = 1<<5,

  COGL_VERTEX_BUFFER_ATTRIB_FLAG_NORMALIZED	  = 1<<6,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_ENABLED	  = 1<<7,

  /* Usage hints */
  /* FIXME - flatten into one flag, since its used as a boolean */
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_INFREQUENT_RESUBMIT  = 1<<8,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_FREQUENT_RESUBMIT	  = 1<<9,

  /* GL Data types */
  /* NB: Update the _GL_TYPE_MASK below if these are changed */
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_GL_TYPE_BYTE	    = 1<<10,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_GL_TYPE_UNSIGNED_BYTE  = 1<<11,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_GL_TYPE_SHORT	    = 1<<12,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_GL_TYPE_UNSIGNED_SHORT = 1<<13,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_GL_TYPE_INT	    = 1<<14,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_GL_TYPE_UNSIGNED_INT   = 1<<15,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_GL_TYPE_FLOAT	    = 1<<16,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_GL_TYPE_DOUBLE	    = 1<<17,

  COGL_VERTEX_BUFFER_ATTRIB_FLAG_SUBMITTED		    = 1<<18,
  COGL_VERTEX_BUFFER_ATTRIB_FLAG_UNUSED		    = 1<<19

  /* XXX NB: If we need > 24 bits then look at changing the layout
   * of struct _CoglVertexBufferAttrib below */
} CoglVertexBufferAttribFlags;

#define COGL_VERTEX_BUFFER_ATTRIB_FLAG_TYPE_MASK \
  (COGL_VERTEX_BUFFER_ATTRIB_FLAG_COLOR_ARRAY \
   | COGL_VERTEX_BUFFER_ATTRIB_FLAG_NORMAL_ARRAY \
   | COGL_VERTEX_BUFFER_ATTRIB_FLAG_TEXTURE_COORD_ARRAY \
   | COGL_VERTEX_BUFFER_ATTRIB_FLAG_VERTEX_ARRAY \
   | COGL_VERTEX_BUFFER_ATTRIB_FLAG_CUSTOM_ARRAY \
   | COGL_VERTEX_BUFFER_ATTRIB_FLAG_INVALID)

typedef struct _CoglVertexBufferAttrib
{
  /* TODO: look at breaking up the flags into seperate
   * bitfields and seperate enums */
  CoglVertexBufferAttribFlags   flags:24;
  uint8_t	           id;
  GQuark		   name;
  char                    *name_without_detail;
  union _u
  {
    const void		  *pointer;
    size_t		   vbo_offset;
  } u;
  CoglAttributeType        type;
  size_t		   span_bytes;
  uint16_t                 stride;
  uint8_t                  n_components;
  uint8_t                  texture_unit;

  int                      attribute_first;
  CoglAttribute           *attribute;

} CoglVertexBufferAttrib;

typedef enum _CoglVertexBufferVBOFlags
{
  COGL_VERTEX_BUFFER_VBO_FLAG_STRIDED	= 1<<0,
  COGL_VERTEX_BUFFER_VBO_FLAG_MULTIPACK	= 1<<1,

  /* FIXME - flatten into one flag, since its used as a boolean */
  COGL_VERTEX_BUFFER_VBO_FLAG_INFREQUENT_RESUBMIT  = 1<<3,
  COGL_VERTEX_BUFFER_VBO_FLAG_FREQUENT_RESUBMIT    = 1<<4,

  COGL_VERTEX_BUFFER_VBO_FLAG_SUBMITTED	= 1<<5
} CoglVertexBufferVBOFlags;

/*
 * A CoglVertexBufferVBO represents one or more attributes in a single
 * buffer object
 */
typedef struct _CoglVertexBufferVBO
{
  CoglVertexBufferVBOFlags flags;

  CoglAttributeBuffer *attribute_buffer;
  size_t buffer_bytes;

  GList *attributes;
} CoglVertexBufferVBO;

typedef struct _CoglVertexBufferIndices
{
  CoglHandleObject _parent;

  CoglIndices *indices;
} CoglVertexBufferIndices;

typedef struct _CoglVertexBuffer
{
  CoglHandleObject _parent;

  int     n_vertices; /*!< The number of vertices in the buffer */
  GList  *submitted_vbos; /* The VBOs currently submitted to the GPU */

  /* Note: new_attributes is normally NULL and only valid while
   * modifying a buffer. */
  GList  *new_attributes; /*!< attributes pending submission */

  CoglBool dirty_attributes;

  CoglPrimitive *primitive;

} CoglVertexBuffer;

#endif /* __COGL_VERTEX_BUFFER_H */

