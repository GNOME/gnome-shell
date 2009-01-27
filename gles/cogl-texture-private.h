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

typedef struct _CoglTexture       CoglTexture;
typedef struct _CoglTexSliceSpan  CoglTexSliceSpan;
typedef struct _CoglSpanIter      CoglSpanIter;

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

/* To improve batching of geometry when submitting vertices to OpenGL we
 * log the texture rectangles we want to draw to a journal, so when we
 * later flush the journal we aim to batch data, and gl draw calls. */
typedef struct _CoglJournalEntry
{
  CoglHandle     material;
  gint           n_layers;
  guint32        fallback_mask;
  GLuint         layer0_override_texture;
} CoglJournalEntry;

CoglTexture*
_cogl_texture_pointer_from_handle (CoglHandle handle);

gboolean
_cogl_texture_span_has_waste (CoglTexture *tex,
                              gint x_span_index,
                              gint y_span_index);

#endif /* __COGL_TEXTURE_H */
