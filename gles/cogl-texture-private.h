/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __COGL_TEXTURE_H
#define __COGL_TEXTURE_H

#include "cogl-bitmap.h"

typedef struct _CoglTexture         CoglTexture;
typedef struct _CoglTexSliceSpan    CoglTexSliceSpan;
typedef struct _CoglSpanIter        CoglSpanIter;
typedef struct _CoglMultiTexture      CoglMultiTexture;
typedef struct _CoglMultiTextureLayer CoglMultiTextureLayer;

struct _CoglTexSliceSpan
{
  gint   start;
  gint   size;
  gint   waste;
};

struct _CoglTexture
{
  guint              ref_count;
  CoglBitmap         bitmap;
  gboolean           bitmap_owner;
  GLenum             gl_target;
  GLenum             gl_intformat;
  GLenum             gl_format;
  GLenum             gl_type;
  GArray            *slice_x_spans;
  GArray            *slice_y_spans;
  GArray            *slice_gl_handles;
  gint               max_waste;
  COGLenum           min_filter;
  COGLenum           mag_filter;
  gboolean           is_foreign;
  GLint              wrap_mode;
  gboolean           auto_mipmap;
};

struct _CoglMultiTextureLayer
{
  guint	       ref_count;
  guint	       index;	/*!< lowest index is blended first then others
			     on top */
  CoglTexture *tex;	/*!< The texture for this layer, or NULL
			     for an empty layer */

  /* TODO: Add more control over the texture environment for each texture
   * unit. For example we should support dot3 normal mapping. */
};

struct _CoglMultiTexture
{
  guint	   ref_count;
  GList	  *layers;
};

CoglTexture*
_cogl_texture_pointer_from_handle (CoglHandle handle);

#endif /* __COGL_TEXTURE_H */

