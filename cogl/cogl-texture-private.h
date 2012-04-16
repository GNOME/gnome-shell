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
#include "cogl-object-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-spans.h"
#include "cogl-meta-texture.h"
#include "cogl-framebuffer.h"

typedef struct _CoglTextureVtable     CoglTextureVtable;

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

  gboolean is_primitive;

  /* This should update the specified sub region of the texture with a
     sub region of the given bitmap. The bitmap is not converted
     before being passed so the implementation is expected to call
     _cogl_texture_prepare_for_upload with a suitable destination
     format before uploading */
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
                                          CoglMetaTextureCallback callback,
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

  CoglTextureType (* get_type) (CoglTexture *tex);

  gboolean (* is_foreign) (CoglTexture *tex);

  /* Only needs to be implemented if is_primitive == TRUE */
  void (* set_auto_mipmap) (CoglTexture *texture,
                            gboolean value);
};

struct _CoglTexture
{
  CoglObject               _parent;
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
_cogl_texture_register_texture_type (const CoglObjectClass *klass);

#define COGL_TEXTURE_DEFINE(TypeName, type_name)                        \
  COGL_OBJECT_DEFINE_WITH_CODE                                          \
  (TypeName, type_name,                                                 \
   _cogl_texture_register_texture_type (&_cogl_##type_name##_class))

#define COGL_TEXTURE_INTERNAL_DEFINE(TypeName, type_name)               \
  COGL_OBJECT_INTERNAL_DEFINE_WITH_CODE                                 \
  (TypeName, type_name,                                                 \
   _cogl_texture_register_texture_type (&_cogl_##type_name##_class))

gboolean
_cogl_texture_can_hardware_repeat (CoglTexture *texture);

void
_cogl_texture_transform_coords_to_gl (CoglTexture *texture,
                                      float *s,
                                      float *t);
CoglTransformResult
_cogl_texture_transform_quad_coords_to_gl (CoglTexture *texture,
                                           float *coords);

GLenum
_cogl_texture_get_gl_format (CoglTexture *texture);

void
_cogl_texture_set_wrap_mode_parameters (CoglTexture *texture,
                                        GLenum wrap_mode_s,
                                        GLenum wrap_mode_t,
                                        GLenum wrap_mode_p);


void
_cogl_texture_set_filters (CoglTexture *texture,
                           GLenum min_filter,
                           GLenum mag_filter);

void
_cogl_texture_pre_paint (CoglTexture *texture, CoglTexturePrePaintFlags flags);

void
_cogl_texture_ensure_non_quad_rendering (CoglTexture *texture);

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
_cogl_texture_prep_gl_alignment_for_pixels_download (int bpp,
                                                     int width,
                                                     int rowstride);

/* Utility function to use as a fallback for getting the data of any
   texture via the framebuffer */

gboolean
_cogl_texture_draw_and_read (CoglTexture *texture,
                             CoglBitmap  *target_bmp,
                             GLuint       target_gl_format,
                             GLuint       target_gl_type);

gboolean
_cogl_texture_is_foreign (CoglTexture *texture);

void
_cogl_texture_associate_framebuffer (CoglTexture *texture,
                                     CoglFramebuffer *framebuffer);

const GList *
_cogl_texture_get_associated_framebuffers (CoglTexture *texture);

void
_cogl_texture_flush_journal_rendering (CoglTexture *texture);

void
_cogl_texture_spans_foreach_in_region (CoglSpan *x_spans,
                                       int n_x_spans,
                                       CoglSpan *y_spans,
                                       int n_y_spans,
                                       CoglTexture **textures,
                                       float *virtual_coords,
                                       float x_normalize_factor,
                                       float y_normalize_factor,
                                       CoglPipelineWrapMode wrap_x,
                                       CoglPipelineWrapMode wrap_y,
                                       CoglMetaTextureCallback callback,
                                       void *user_data);

/*
 * _cogl_texture_get_type:
 * @texture: a #CoglTexture pointer
 *
 * Retrieves the texture type of the underlying hardware texture that
 * this #CoglTexture will use.
 *
 * Return value: The type of the hardware texture.
 */
CoglTextureType
_cogl_texture_get_type (CoglTexture *texture);

#endif /* __COGL_TEXTURE_PRIVATE_H */
