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
#include "cogl-pipeline-private.h"

#define COGL_TEXTURE(tex) ((CoglTexture *)(tex))

typedef struct _CoglTexture           CoglTexture;
typedef struct _CoglTextureVtable     CoglTextureVtable;

typedef void (*CoglTextureSliceCallback) (CoglHandle handle,
                                          const float *slice_coords,
                                          const float *virtual_coords,
                                          void *user_data);

typedef void (* CoglTextureManualRepeatCallback) (const float *coords,
                                                  void *user_data);

/* Encodes three possibiloities result of transforming a quad */
typedef enum {
  /* quad doesn't cross the boundaries of a texture */
  COGL_TRANSFORM_NO_REPEAT,
  /* quad crosses boundaries, hardware wrap mode can handle */
  COGL_TRANSFORM_HARDWARE_REPEAT,
  /* quad crosses boundaries, needs software fallback;
   * for a sliced texture, this might not actually involve
   * repeating, just a quad crossing multiple slices */
  COGL_TRANSFORM_SOFTWARE_REPEAT,
} CoglTransformResult;

/* Flags given to the pre_paint method */
typedef enum {
  /* The texture is going to be used with filters that require
     mipmapping. This gives the texture the opportunity to
     automatically update the mipmap tree */
  COGL_TEXTURE_NEEDS_MIPMAP = 1
} CoglTexturePrePaintFlags;

struct _CoglTextureVtable
{
  /* Virtual functions that must be implemented for a texture
     backend */

  /* This should update the specified sub region of the texture with a
     sub region of the given bitmap. The bitmap will have first been
     converted to a suitable format for uploading if neccessary. */
  gboolean (* set_region) (CoglTexture    *tex,
                           int             src_x,
                           int             src_y,
                           int             dst_x,
                           int             dst_y,
                           unsigned int    dst_width,
                           unsigned int    dst_height,
                           CoglBitmap     *bitmap);

  /* This should copy the image data of the texture into @data. The
     requested format will have been first passed through
     ctx->texture_driver->find_best_gl_get_data_format so it should
     always be a format that is valid for GL (ie, no conversion should
     be necessary). */
  gboolean (* get_data) (CoglTexture     *tex,
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
  CoglTransformResult (* transform_quad_coords_to_gl) (CoglTexture *tex,
						       float *coords);

  gboolean (* get_gl_texture) (CoglTexture *tex,
                               GLuint *out_gl_handle,
                               GLenum *out_gl_target);

  void (* set_filters) (CoglTexture *tex,
                        GLenum min_filter,
                        GLenum mag_filter);

  void (* pre_paint) (CoglTexture *tex, CoglTexturePrePaintFlags flags);
  void (* ensure_non_quad_rendering) (CoglTexture *tex);

  void (* set_wrap_mode_parameters) (CoglTexture *tex,
                                     GLenum wrap_mode_s,
                                     GLenum wrap_mode_t,
                                     GLenum wrap_mode_p);

  CoglPixelFormat (* get_format) (CoglTexture *tex);
  GLenum (* get_gl_format) (CoglTexture *tex);
  int (* get_width) (CoglTexture *tex);
  int (* get_height) (CoglTexture *tex);

  gboolean (* is_foreign) (CoglTexture *tex);
};

struct _CoglTexture
{
  CoglHandleObject         _parent;
  GList                   *framebuffers;
  const CoglTextureVtable *vtable;
};

typedef enum _CoglTextureChangeFlags
{
  /* Whenever the internals of a texture are changed such that the
   * underlying GL textures that represent the CoglTexture change then
   * we notify cogl-material.c via
   * _cogl_pipeline_texture_pre_change_notify
   */
  COGL_TEXTURE_CHANGE_GL_TEXTURES

} CoglTextureChangeFlags;

typedef struct _CoglTexturePixel  CoglTexturePixel;

/* This is used by the texture backends to store the first pixel of
   each GL texture. This is only used when glGenerateMipmap is not
   available so that we can temporarily set GL_GENERATE_MIPMAP and
   reupload a pixel */
struct _CoglTexturePixel
{
  /* We need to store the format of the pixel because we store the
     data in the source format which might end up being different for
     each slice if a subregion is updated with a different format */
  GLenum gl_format;
  GLenum gl_type;
  guint8 data[4];
};

void
_cogl_texture_init (CoglTexture *texture,
                    const CoglTextureVtable *vtable);

void
_cogl_texture_free (CoglTexture *texture);

/* This is used to register a type to the list of handle types that
   will be considered a texture in cogl_is_texture() */
void
_cogl_texture_register_texture_type (GQuark type);

#define COGL_TEXTURE_DEFINE(TypeName, type_name)                        \
  COGL_HANDLE_DEFINE_WITH_CODE                                          \
  (TypeName, type_name,                                                 \
   _cogl_texture_register_texture_type (_cogl_handle_                   \
                                        ## type_name ## _get_type ()))

#define COGL_TEXTURE_INTERNAL_DEFINE(TypeName, type_name)               \
  COGL_HANDLE_INTERNAL_DEFINE_WITH_CODE                                 \
  (TypeName, type_name,                                                 \
   _cogl_texture_register_texture_type (_cogl_handle_                   \
                                        ## type_name ## _get_type ()))

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
CoglTransformResult
_cogl_texture_transform_quad_coords_to_gl (CoglHandle handle,
                                           float *coords);

GLenum
_cogl_texture_get_gl_format (CoglHandle handle);

void
_cogl_texture_set_wrap_mode_parameters (CoglHandle handle,
                                        GLenum wrap_mode_s,
                                        GLenum wrap_mode_t,
                                        GLenum wrap_mode_p);


void
_cogl_texture_set_filters (CoglHandle handle,
                           GLenum min_filter,
                           GLenum mag_filter);

void
_cogl_texture_pre_paint (CoglHandle handle, CoglTexturePrePaintFlags flags);

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

CoglBitmap *
_cogl_texture_prepare_for_upload (CoglBitmap      *src_bmp,
                                  CoglPixelFormat  dst_format,
                                  CoglPixelFormat *dst_format_out,
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

gboolean
_cogl_texture_is_foreign (CoglHandle handle);

gboolean
_cogl_texture_set_region_from_bitmap (CoglHandle    handle,
                                      int           src_x,
                                      int           src_y,
                                      int           dst_x,
                                      int           dst_y,
                                      unsigned int  dst_width,
                                      unsigned int  dst_height,
                                      CoglBitmap   *bmp);

void
_cogl_texture_associate_framebuffer (CoglHandle handle,
                                     CoglFramebuffer *framebuffer);

const GList *
_cogl_texture_get_associated_framebuffers (CoglHandle handle);

void
_cogl_texture_flush_journal_rendering (CoglHandle handle);

#endif /* __COGL_TEXTURE_PRIVATE_H */
