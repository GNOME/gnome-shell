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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_TEXTURE_PRIVATE_H
#define __COGL_TEXTURE_PRIVATE_H

#include "cogl-bitmap-private.h"
#include "cogl-handle.h"
#include "cogl-material-private.h"

#define COGL_TEXTURE(tex) ((CoglTexture *)(tex))

typedef struct _CoglTexture           CoglTexture;
typedef struct _CoglTextureVtable     CoglTextureVtable;

typedef void (*CoglTextureSliceCallback) (CoglHandle handle,
                                          GLuint gl_handle,
                                          GLenum gl_target,
                                          const float *slice_coords,
                                          const float *virtual_coords,
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

  int (* get_max_waste) (CoglTexture *tex);

  gboolean (* is_sliced) (CoglTexture *tex);

  gboolean (* can_hardware_repeat) (CoglTexture *tex);

  void (* transform_coords_to_gl) (CoglTexture *tex,
                                   float *s,
                                   float *t);
  gboolean (* transform_quad_coords_to_gl) (CoglTexture *tex,
                                            float *coords);

  gboolean (* get_gl_texture) (CoglTexture *tex,
                               GLuint *out_gl_handle,
                               GLenum *out_gl_target);

  void (* set_filters) (CoglTexture *tex,
                        GLenum min_filter,
                        GLenum mag_filter);

  void (* ensure_mipmaps) (CoglTexture *tex);
  void (* ensure_non_quad_rendering) (CoglTexture *tex);

  void (* set_wrap_mode_parameter) (CoglTexture *tex,
                                    GLenum wrap_mode);

  CoglPixelFormat (* get_format) (CoglTexture *tex);
  GLenum (* get_gl_format) (CoglTexture *tex);
  int (* get_width) (CoglTexture *tex);
  int (* get_height) (CoglTexture *tex);
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
gboolean
_cogl_texture_transform_quad_coords_to_gl (CoglHandle handle,
                                           float *coords);

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

void
_cogl_texture_ensure_non_quad_rendering (CoglHandle handle);

/* Utility function to determine which pixel format to use when
   dst_format is COGL_PIXEL_FORMAT_ANY. If dst_format is not ANY then
   it will just be returned directly */
CoglPixelFormat
_cogl_texture_determine_internal_format (CoglPixelFormat src_format,
                                         CoglPixelFormat dst_format);

/* Utility function to help uploading a bitmap. If the bitmap needs
   premult conversion then it will be copied and *copied_bitmap will
   be set to TRUE. Otherwise dst_bmp will be set to a shallow copy of
   src_bmp. The GLenums needed for uploading are returned */

gboolean
_cogl_texture_prepare_for_upload (CoglBitmap      *src_bmp,
                                  CoglPixelFormat  dst_format,
                                  CoglPixelFormat *dst_format_out,
                                  CoglBitmap      *dst_bmp,
                                  gboolean        *copied_bitmap,
                                  GLenum          *out_glintformat,
                                  GLenum          *out_glformat,
                                  GLenum          *out_gltype);

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
