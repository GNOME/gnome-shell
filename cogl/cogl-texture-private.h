/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_TEXTURE_PRIVATE_H
#define __COGL_TEXTURE_PRIVATE_H

#include "cogl-bitmap-private.h"
#include "cogl-handle.h"
#include "cogl-material-private.h"

typedef struct _CoglTexture       CoglTexture;
typedef struct _CoglTexSliceSpan  CoglTexSliceSpan;
typedef struct _CoglSpanIter      CoglSpanIter;
typedef struct _CoglTexturePixel  CoglTexturePixel;

struct _CoglTexSliceSpan
{
  gint   start;
  gint   size;
  gint   waste;
};

struct _CoglSpanIter
{
  gint              index;
  GArray           *array;
  CoglTexSliceSpan *span;
  float             pos;
  float             next_pos;
  float             origin;
  float             cover_start;
  float             cover_end;
  float             intersect_start;
  float             intersect_end;
  float             intersect_start_local;
  float             intersect_end_local;
  gboolean          intersects;
  gboolean          flipped;
};

/* This is used to store the first pixel of each slice. This is only
   used when glGenerateMipmap is not available */
struct _CoglTexturePixel
{
  /* We need to store the format of the pixel because we store the
     data in the source format which might end up being different for
     each slice if a subregion is updated with a different format */
  GLenum gl_format;
  GLenum gl_type;
  guint8 data[4];
};

struct _CoglTexture
{
  CoglHandleObject   _parent;
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
  GLenum             min_filter;
  GLenum             mag_filter;
  gboolean           is_foreign;
  GLint              wrap_mode;
  gboolean           auto_mipmap;
  gboolean           mipmaps_dirty;

  /* This holds a copy of the first pixel in each slice. It is only
     used to force an automatic update of the mipmaps when
     glGenerateMipmap is not available. */
  CoglTexturePixel  *first_pixels;
};

/* To improve batching of geometry when submitting vertices to OpenGL we
 * log the texture rectangles we want to draw to a journal, so when we
 * later flush the journal we aim to batch data, and gl draw calls. */
typedef struct _CoglJournalEntry
{
  CoglHandle               material;
  int                      n_layers;
  CoglMaterialFlushOptions flush_options;
  CoglMatrix               model_view;
  /* XXX: These entries are pretty big now considering the padding in
   * CoglMaterialFlushOptions and CoglMatrix, so we might need to optimize this
   * later. */
} CoglJournalEntry;

typedef void (*CoglTextureSliceCallback) (CoglHandle handle,
                                          GLuint gl_handle,
                                          GLenum gl_target,
                                          float *slice_coords,
                                          float *virtual_coords,
                                          void *user_data);

CoglTexture*
_cogl_texture_pointer_from_handle (CoglHandle handle);

void
_cogl_texture_foreach_sub_texture_in_region (CoglHandle handle,
                                             float virtual_tx_1,
                                             float virtual_ty_1,
                                             float virtual_tx_2,
                                             float virtual_ty_2,
                                             CoglTextureSliceCallback callback,
                                             void *user_data);

gboolean
_cogl_texture_can_hardware_repeat (CoglHandle handle);

void
_cogl_texture_transform_coords_to_gl (CoglHandle handle,
                                      float *s,
                                      float *t);
GLenum
_cogl_texture_get_internal_gl_format (CoglHandle handle);

void
_cogl_texture_set_wrap_mode_parameter (CoglHandle handle,
                                       GLenum wrap_mode);

void
_cogl_texture_set_filters (CoglHandle handle,
                           GLenum min_filter,
                           GLenum mag_filter);

void
_cogl_texture_ensure_mipmaps (CoglHandle handle);

void
_cogl_texture_prep_gl_alignment_for_pixels_upload (int pixels_rowstride);

void
_cogl_texture_prep_gl_alignment_for_pixels_download (int pixels_rowstride);

#endif /* __COGL_TEXTURE_PRIVATE_H */
