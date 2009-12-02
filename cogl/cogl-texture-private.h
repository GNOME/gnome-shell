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

typedef struct _CoglTexture           CoglTexture;
typedef struct _CoglTextureVtable     CoglTextureVtable;
typedef struct _CoglTextureUploadData CoglTextureUploadData;

typedef void (*CoglTextureSliceCallback) (CoglHandle handle,
                                          GLuint gl_handle,
                                          GLenum gl_target,
                                          float *slice_coords,
                                          float *virtual_coords,
                                          void *user_data);

typedef void (* CoglTextureManualRepeatCallback) (const float *coords,
                                                  void *user_data);

struct _CoglTextureVtable
{
  /* Virtual functions that must be implemented for a texture
     backend */

  gboolean (* set_region) (CoglTexture    *tex,
                           int             src_x,
                           int             src_y,
                           int             dst_x,
                           int             dst_y,
                           unsigned int    dst_width,
                           unsigned int    dst_height,
                           int             width,
                           int             height,
                           CoglPixelFormat format,
                           unsigned int    rowstride,
                           const guint8   *data);

  int (* get_data) (CoglTexture     *tex,
                    CoglPixelFormat  format,
                    unsigned int     rowstride,
                    guint8          *data);

  void (* foreach_sub_texture_in_region) (CoglTexture *tex,
                                          float virtual_tx_1,
                                          float virtual_ty_1,
                                          float virtual_tx_2,
                                          float virtual_ty_2,
                                          CoglTextureSliceCallback callback,
                                          void *user_data);

  gint (* get_max_waste) (CoglTexture *tex);

  gboolean (* is_sliced) (CoglTexture *tex);

  gboolean (* can_hardware_repeat) (CoglTexture *tex);

  void (* transform_coords_to_gl) (CoglTexture *tex,
                                   float *s,
                                   float *t);

  gboolean (* get_gl_texture) (CoglTexture *tex,
                               GLuint *out_gl_handle,
                               GLenum *out_gl_target);

  void (* set_filters) (CoglTexture *tex,
                        GLenum min_filter,
                        GLenum mag_filter);

  void (* ensure_mipmaps) (CoglTexture *tex);

  void (* set_wrap_mode_parameter) (CoglTexture *tex,
                                    GLenum wrap_mode);

  CoglPixelFormat (* get_format) (CoglTexture *tex);
  GLenum (* get_gl_format) (CoglTexture *tex);
  gint (* get_width) (CoglTexture *tex);
  gint (* get_height) (CoglTexture *tex);
};

/* This represents the state needed to upload texture data. There are
   utility functions in cogl-texture which use this state */
struct _CoglTextureUploadData
{
  CoglBitmap bitmap;
  gboolean   bitmap_owner;
  GLenum     gl_intformat;
  GLenum     gl_format;
  GLenum     gl_type;
};

struct _CoglTexture
{
  CoglHandleObject         _parent;
  const CoglTextureVtable *vtable;
};

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
_cogl_texture_get_gl_format (CoglHandle handle);

void
_cogl_texture_set_wrap_mode_parameter (CoglHandle handle,
                                       GLenum wrap_mode);

void
_cogl_texture_set_filters (CoglHandle handle,
                           GLenum min_filter,
                           GLenum mag_filter);

void
_cogl_texture_ensure_mipmaps (CoglHandle handle);

/* Utility functions to help uploading a bitmap. These are intended to
 * be used by CoglTexture implementations or drivers... */

void
_cogl_texture_upload_data_free (CoglTextureUploadData *data);

void
_cogl_texture_upload_data_swap_bitmap (CoglTextureUploadData *data,
                                       CoglBitmap            *new_bitmap);

gboolean
_cogl_texture_upload_data_prepare (CoglTextureUploadData *data,
                                   CoglPixelFormat        internal_format);

void
_cogl_texture_prep_gl_alignment_for_pixels_upload (int pixels_rowstride);

void
_cogl_texture_prep_gl_alignment_for_pixels_download (int pixels_rowstride);

/* Utility function for implementing manual repeating. Even texture
   backends that always support hardware repeating need this because
   when foreach_sub_texture_in_region is invoked Cogl will set the
   wrap mode to GL_CLAMP_TO_EDGE so hardware repeating can't be
   done */
void
_cogl_texture_iterate_manual_repeats (CoglTextureManualRepeatCallback callback,
                                      float tx_1, float ty_1,
                                      float tx_2, float ty_2,
                                      void *user_data);

/* Utility function to use as a fallback for getting the data of any
   texture via the framebuffer */

gboolean
_cogl_texture_draw_and_read (CoglHandle   handle,
                             CoglBitmap  *target_bmp,
                             GLuint       target_gl_format,
                             GLuint       target_gl_type);

#endif /* __COGL_TEXTURE_PRIVATE_H */
