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

#define COGL_TEXTURE(tex) ((CoglTexture *)(tex))

typedef struct _CoglTexture       CoglTexture;

typedef enum _CoglTextureType
{
  COGL_TEXTURE_TYPE_2D_SLICED
} CoglTextureType;

struct _CoglTexture
{
  CoglHandleObject   _parent;
  CoglTextureType    type;
  CoglBitmap         bitmap;
  gboolean           bitmap_owner;
  GLenum             gl_target;
  GLenum             gl_intformat;
  GLenum             gl_format;
  GLenum             gl_type;
  GLenum             min_filter;
  GLenum             mag_filter;
  gboolean           is_foreign;
  GLint              wrap_mode;
  gboolean           auto_mipmap;
  gboolean           mipmaps_dirty;
};

typedef void (*CoglTextureSliceCallback) (CoglHandle handle,
                                          GLuint gl_handle,
                                          GLenum gl_target,
                                          float *slice_coords,
                                          float *virtual_coords,
                                          void *user_data);

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


/* Functions currently only used by CoglTexture implementations or
 * drivers... */

void
_cogl_texture_free (CoglTexture *tex);

void
_cogl_texture_bitmap_free (CoglTexture *tex);

void
_cogl_texture_bitmap_swap (CoglTexture  *tex,
			   CoglBitmap   *new_bitmap);

gboolean
_cogl_texture_bitmap_prepare (CoglTexture     *tex,
			      CoglPixelFormat  internal_format);

void
_cogl_texture_prep_gl_alignment_for_pixels_upload (int pixels_rowstride);

void
_cogl_texture_prep_gl_alignment_for_pixels_download (int pixels_rowstride);

gboolean
_cogl_texture_draw_and_read (CoglTexture *tex,
                             CoglBitmap  *target_bmp,
                             GLuint       target_gl_format,
                             GLuint       target_gl_type);

#endif /* __COGL_TEXTURE_PRIVATE_H */
