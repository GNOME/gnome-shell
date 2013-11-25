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
 *
 * Authors:
 *  Matthew Allum  <mallum@openedhand.com>
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-private.h"
#include "cogl-util.h"
#include "cogl-bitmap.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-pipeline.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-primitives.h"
#include "cogl-util-gl-private.h"
#include "cogl-error-private.h"
#include "cogl-texture-gl-private.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_MAX_3D_TEXTURE_SIZE_OES
#define GL_MAX_3D_TEXTURE_SIZE_OES 0x8073
#endif

/* This extension isn't available for GLES 1.1 so these won't be
   defined */
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GL_UNPACK_SKIP_ROWS
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#endif
#ifndef GL_UNPACK_SKIP_PIXELS
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#endif

static GLuint
_cogl_texture_driver_gen (CoglContext *ctx,
                          GLenum gl_target,
                          CoglPixelFormat internal_format)
{
  GLuint tex;

  GE (ctx, glGenTextures (1, &tex));

  _cogl_bind_gl_texture_transient (gl_target, tex, FALSE);

  switch (gl_target)
    {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_3D:
      /* GL_TEXTURE_MAG_FILTER defaults to GL_LINEAR, no need to set it */
      GE( ctx, glTexParameteri (gl_target,
                                GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR) );
      break;

    default:
      g_assert_not_reached();
    }

  return tex;
}

static void
prep_gl_for_pixels_upload_full (CoglContext *ctx,
                                int pixels_rowstride,
                                int pixels_src_x,
                                int pixels_src_y,
                                int pixels_bpp)
{
  if (_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_UNPACK_SUBIMAGE))
    {
      GE( ctx, glPixelStorei (GL_UNPACK_ROW_LENGTH,
                              pixels_rowstride / pixels_bpp) );

      GE( ctx, glPixelStorei (GL_UNPACK_SKIP_PIXELS, pixels_src_x) );
      GE( ctx, glPixelStorei (GL_UNPACK_SKIP_ROWS, pixels_src_y) );
    }
  else
    {
      g_assert (pixels_src_x == 0);
      g_assert (pixels_src_y == 0);
    }

  _cogl_texture_gl_prep_alignment_for_pixels_upload (ctx, pixels_rowstride);
}

static void
_cogl_texture_driver_prep_gl_for_pixels_upload (CoglContext *ctx,
                                                int pixels_rowstride,
                                                int pixels_bpp)
{
  prep_gl_for_pixels_upload_full (ctx,
                                  pixels_rowstride,
                                  0, 0, /* src_x/y */
                                  pixels_bpp);
}

static void
_cogl_texture_driver_prep_gl_for_pixels_download (CoglContext *ctx,
                                                  int pixels_rowstride,
                                                  int image_width,
                                                  int pixels_bpp)
{
  _cogl_texture_gl_prep_alignment_for_pixels_download (ctx,
                                                       pixels_bpp,
                                                       image_width,
                                                       pixels_rowstride);
}

static CoglBitmap *
prepare_bitmap_alignment_for_upload (CoglContext *ctx,
                                     CoglBitmap *src_bmp,
                                     CoglError **error)
{
  CoglPixelFormat format = cogl_bitmap_get_format (src_bmp);
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (format);
  int src_rowstride = cogl_bitmap_get_rowstride (src_bmp);
  int width = cogl_bitmap_get_width (src_bmp);
  int alignment = 1;

  if (_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_UNPACK_SUBIMAGE) ||
      src_rowstride == 0)
    return cogl_object_ref (src_bmp);

  /* Work out the alignment of the source rowstride */
  alignment = 1 << (_cogl_util_ffs (src_rowstride) - 1);
  alignment = MIN (alignment, 8);

  /* If the aligned data equals the rowstride then we can upload from
     the bitmap directly using GL_UNPACK_ALIGNMENT */
  if (((width * bpp + alignment - 1) & ~(alignment - 1)) == src_rowstride)
    return cogl_object_ref (src_bmp);
  /* Otherwise we need to copy the bitmap to pack the alignment
     because GLES has no GL_ROW_LENGTH */
  else
    return _cogl_bitmap_copy (src_bmp, error);
}

static CoglBool
_cogl_texture_driver_upload_subregion_to_gl (CoglContext *ctx,
                                             CoglTexture *texture,
                                             CoglBool is_foreign,
                                             int src_x,
                                             int src_y,
                                             int dst_x,
                                             int dst_y,
                                             int width,
                                             int height,
                                             int level,
                                             CoglBitmap *source_bmp,
				             GLuint source_gl_format,
				             GLuint source_gl_type,
                                             CoglError **error)
{
  GLenum gl_target;
  GLuint gl_handle;
  uint8_t *data;
  CoglPixelFormat source_format = cogl_bitmap_get_format (source_bmp);
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (source_format);
  CoglBitmap *slice_bmp;
  int rowstride;
  GLenum gl_error;
  CoglBool status = TRUE;
  CoglError *internal_error = NULL;
  int level_width;
  int level_height;

  cogl_texture_get_gl_texture (texture, &gl_handle, &gl_target);

  /* If we have the GL_EXT_unpack_subimage extension then we can
     upload from subregions directly. Otherwise we may need to copy
     the bitmap */
  if (!_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_UNPACK_SUBIMAGE) &&
      (src_x != 0 || src_y != 0 ||
       width != cogl_bitmap_get_width (source_bmp) ||
       height != cogl_bitmap_get_height (source_bmp)))
    {
      slice_bmp =
        _cogl_bitmap_new_with_malloc_buffer (ctx,
                                             width, height,
                                             source_format,
                                             error);
      if (!slice_bmp)
        return FALSE;

      if (!_cogl_bitmap_copy_subregion (source_bmp,
                                        slice_bmp,
                                        src_x, src_y,
                                        0, 0, /* dst_x/y */
                                        width, height,
                                        error))
        {
          cogl_object_unref (slice_bmp);
          return FALSE;
        }

      src_x = src_y = 0;
    }
  else
    {
      slice_bmp = prepare_bitmap_alignment_for_upload (ctx, source_bmp, error);
      if (!slice_bmp)
        return FALSE;
    }

  rowstride = cogl_bitmap_get_rowstride (slice_bmp);

  /* Setup gl alignment to match rowstride and top-left corner */
  prep_gl_for_pixels_upload_full (ctx, rowstride, src_x, src_y, bpp);

  data = _cogl_bitmap_gl_bind (slice_bmp, COGL_BUFFER_ACCESS_READ, 0, &internal_error);

  /* NB: _cogl_bitmap_gl_bind() may return NULL when successfull so we
   * have to explicitly check the cogl error pointer to catch
   * problems... */
  if (internal_error)
    {
      _cogl_propagate_error (error, internal_error);
      cogl_object_unref (slice_bmp);
      return FALSE;
    }

  _cogl_bind_gl_texture_transient (gl_target, gl_handle, is_foreign);

  /* Clear any GL errors */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  _cogl_texture_get_level_size (texture,
                                level,
                                &level_width,
                                &level_height,
                                NULL);

  if (level_width == width && level_height == height)
    {
      /* GL gets upset if you use glTexSubImage2D to define the
       * contents of a mipmap level so we make sure to use
       * glTexImage2D if we are uploading a full mipmap level.
       */
      ctx->glTexImage2D (gl_target,
                         level,
                         _cogl_texture_gl_get_format (texture),
                         width,
                         height,
                         0,
                         source_gl_format,
                         source_gl_type,
                         data);
    }
  else
    {
      /* GL gets upset if you use glTexSubImage2D to initialize the
       * contents of a mipmap level so if this is the first time
       * we've seen a request to upload to this level we call
       * glTexImage2D first to assert that the storage for this
       * level exists.
       */
      if (texture->max_level < level)
        {
          ctx->glTexImage2D (gl_target,
                             level,
                             _cogl_texture_gl_get_format (texture),
                             level_width,
                             level_height,
                             0,
                             source_gl_format,
                             source_gl_type,
                             NULL);
        }

      ctx->glTexSubImage2D (gl_target,
                            level,
                            dst_x, dst_y,
                            width, height,
                            source_gl_format,
                            source_gl_type,
                            data);
    }

  if (_cogl_gl_util_catch_out_of_memory (ctx, error))
    status = FALSE;

  _cogl_bitmap_gl_unbind (slice_bmp);

  cogl_object_unref (slice_bmp);

  return status;
}

static CoglBool
_cogl_texture_driver_upload_to_gl (CoglContext *ctx,
                                   GLenum gl_target,
                                   GLuint gl_handle,
                                   CoglBool is_foreign,
                                   CoglBitmap *source_bmp,
                                   GLint internal_gl_format,
                                   GLuint source_gl_format,
                                   GLuint source_gl_type,
                                   CoglError **error)
{
  CoglPixelFormat source_format = cogl_bitmap_get_format (source_bmp);
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (source_format);
  int rowstride;
  int bmp_width = cogl_bitmap_get_width (source_bmp);
  int bmp_height = cogl_bitmap_get_height (source_bmp);
  CoglBitmap *bmp;
  uint8_t *data;
  GLenum gl_error;
  CoglError *internal_error = NULL;
  CoglBool status = TRUE;

  bmp = prepare_bitmap_alignment_for_upload (ctx, source_bmp, error);
  if (!bmp)
    return FALSE;

  rowstride = cogl_bitmap_get_rowstride (bmp);

  /* Setup gl alignment to match rowstride and top-left corner */
  _cogl_texture_driver_prep_gl_for_pixels_upload (ctx, rowstride, bpp);

  _cogl_bind_gl_texture_transient (gl_target, gl_handle, is_foreign);

  data = _cogl_bitmap_gl_bind (bmp,
                               COGL_BUFFER_ACCESS_READ,
                               0, /* hints */
                               &internal_error);

  /* NB: _cogl_bitmap_gl_bind() may return NULL when successful so we
   * have to explicitly check the cogl error pointer to catch
   * problems... */
  if (internal_error)
    {
      cogl_object_unref (bmp);
      _cogl_propagate_error (error, internal_error);
      return FALSE;
    }

  /* Clear any GL errors */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  ctx->glTexImage2D (gl_target, 0,
                     internal_gl_format,
                     bmp_width, bmp_height,
                     0,
                     source_gl_format,
                     source_gl_type,
                     data);

  if (_cogl_gl_util_catch_out_of_memory (ctx, error))
    status = FALSE;

  _cogl_bitmap_gl_unbind (bmp);

  cogl_object_unref (bmp);

  return status;
}

static CoglBool
_cogl_texture_driver_upload_to_gl_3d (CoglContext *ctx,
                                      GLenum gl_target,
                                      GLuint gl_handle,
                                      CoglBool is_foreign,
                                      GLint height,
                                      GLint depth,
                                      CoglBitmap *source_bmp,
                                      GLint internal_gl_format,
                                      GLuint source_gl_format,
                                      GLuint source_gl_type,
                                      CoglError **error)
{
  CoglPixelFormat source_format = cogl_bitmap_get_format (source_bmp);
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (source_format);
  int rowstride = cogl_bitmap_get_rowstride (source_bmp);
  int bmp_width = cogl_bitmap_get_width (source_bmp);
  int bmp_height = cogl_bitmap_get_height (source_bmp);
  uint8_t *data;
  GLenum gl_error;

  _cogl_bind_gl_texture_transient (gl_target, gl_handle, is_foreign);

  /* If the rowstride or image height can't be specified with just
     GL_ALIGNMENT alone then we need to copy the bitmap because there
     is no GL_ROW_LENGTH */
  if (rowstride / bpp != bmp_width ||
      height != bmp_height / depth)
    {
      CoglBitmap *bmp;
      int image_height = bmp_height / depth;
      CoglPixelFormat source_bmp_format = cogl_bitmap_get_format (source_bmp);
      int i;

      _cogl_texture_driver_prep_gl_for_pixels_upload (ctx, bmp_width * bpp, bpp);

      /* Initialize the texture with empty data and then upload each
         image with a sub-region update */

      /* Clear any GL errors */
      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;

      ctx->glTexImage3D (gl_target,
                         0, /* level */
                         internal_gl_format,
                         bmp_width,
                         height,
                         depth,
                         0,
                         source_gl_format,
                         source_gl_type,
                         NULL);

      if (_cogl_gl_util_catch_out_of_memory (ctx, error))
        return FALSE;

      bmp = _cogl_bitmap_new_with_malloc_buffer (ctx,
                                                 bmp_width,
                                                 height,
                                                 source_bmp_format,
                                                 error);
      if (!bmp)
        return FALSE;

      for (i = 0; i < depth; i++)
        {
          if (!_cogl_bitmap_copy_subregion (source_bmp,
                                            bmp,
                                            0, image_height * i,
                                            0, 0,
                                            bmp_width,
                                            height,
                                            error))
            {
              cogl_object_unref (bmp);
              return FALSE;
            }

          data = _cogl_bitmap_gl_bind (bmp,
                                       COGL_BUFFER_ACCESS_READ, 0, error);
          if (!data)
            {
              cogl_object_unref (bmp);
              return FALSE;
            }

          /* Clear any GL errors */
          while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
            ;

          ctx->glTexSubImage3D (gl_target,
                                0, /* level */
                                0, /* xoffset */
                                0, /* yoffset */
                                i, /* zoffset */
                                bmp_width, /* width */
                                height, /* height */
                                1, /* depth */
                                source_gl_format,
                                source_gl_type,
                                data);

          if (_cogl_gl_util_catch_out_of_memory (ctx, error))
            {
              cogl_object_unref (bmp);
              _cogl_bitmap_gl_unbind (bmp);
              return FALSE;
            }

          _cogl_bitmap_gl_unbind (bmp);
        }

      cogl_object_unref (bmp);
    }
  else
    {
      data = _cogl_bitmap_gl_bind (source_bmp, COGL_BUFFER_ACCESS_READ, 0, error);
      if (!data)
        return FALSE;

      _cogl_texture_driver_prep_gl_for_pixels_upload (ctx, rowstride, bpp);

      /* Clear any GL errors */
      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;

      ctx->glTexImage3D (gl_target,
                         0, /* level */
                         internal_gl_format,
                         bmp_width,
                         height,
                         depth,
                         0,
                         source_gl_format,
                         source_gl_type,
                         data);

      if (_cogl_gl_util_catch_out_of_memory (ctx, error))
        {
          _cogl_bitmap_gl_unbind (source_bmp);
          return FALSE;
        }

      _cogl_bitmap_gl_unbind (source_bmp);
    }

  return TRUE;
}

/* NB: GLES doesn't support glGetTexImage2D, so cogl-texture will instead
 * fallback to a generic render + readpixels approach to downloading
 * texture data. (See _cogl_texture_draw_and_read() ) */
static CoglBool
_cogl_texture_driver_gl_get_tex_image (CoglContext *ctx,
                                       GLenum gl_target,
                                       GLenum dest_gl_format,
                                       GLenum dest_gl_type,
                                       uint8_t *dest)
{
  return FALSE;
}

static CoglBool
_cogl_texture_driver_size_supported_3d (CoglContext *ctx,
                                        GLenum gl_target,
                                        GLenum gl_format,
                                        GLenum gl_type,
                                        int width,
                                        int height,
                                        int depth)
{
  GLint max_size;

  /* GLES doesn't support a proxy texture target so let's at least
     check whether the size is greater than
     GL_MAX_3D_TEXTURE_SIZE_OES */
  GE( ctx, glGetIntegerv (GL_MAX_3D_TEXTURE_SIZE_OES, &max_size) );

  return width <= max_size && height <= max_size && depth <= max_size;
}

static CoglBool
_cogl_texture_driver_size_supported (CoglContext *ctx,
                                     GLenum gl_target,
                                     GLenum gl_intformat,
                                     GLenum gl_format,
                                     GLenum gl_type,
                                     int width,
                                     int height)
{
  GLint max_size;

  /* GLES doesn't support a proxy texture target so let's at least
     check whether the size is greater than GL_MAX_TEXTURE_SIZE */
  GE( ctx, glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_size) );

  return width <= max_size && height <= max_size;
}

static void
_cogl_texture_driver_try_setting_gl_border_color
                                            (CoglContext *ctx,
                                             GLuint gl_target,
                                             const GLfloat *transparent_color)
{
  /* FAIL! */
}

static CoglBool
_cogl_texture_driver_allows_foreign_gl_target (CoglContext *ctx,
                                               GLenum gl_target)
{
  /* Allow 2-dimensional textures only */
  if (gl_target != GL_TEXTURE_2D)
    return FALSE;
  return TRUE;
}

static CoglPixelFormat
_cogl_texture_driver_find_best_gl_get_data_format
                                            (CoglContext *context,
                                             CoglPixelFormat format,
                                             GLenum *closest_gl_format,
                                             GLenum *closest_gl_type)
{
  /* Find closest format that's supported by GL
     (Can't use _cogl_pixel_format_to_gl since available formats
      when reading pixels on GLES are severely limited) */
  *closest_gl_format = GL_RGBA;
  *closest_gl_type = GL_UNSIGNED_BYTE;
  return COGL_PIXEL_FORMAT_RGBA_8888;
}

const CoglTextureDriver
_cogl_texture_driver_gles =
  {
    _cogl_texture_driver_gen,
    _cogl_texture_driver_prep_gl_for_pixels_upload,
    _cogl_texture_driver_upload_subregion_to_gl,
    _cogl_texture_driver_upload_to_gl,
    _cogl_texture_driver_upload_to_gl_3d,
    _cogl_texture_driver_prep_gl_for_pixels_download,
    _cogl_texture_driver_gl_get_tex_image,
    _cogl_texture_driver_size_supported,
    _cogl_texture_driver_size_supported_3d,
    _cogl_texture_driver_try_setting_gl_border_color,
    _cogl_texture_driver_allows_foreign_gl_target,
    _cogl_texture_driver_find_best_gl_get_data_format
  };
