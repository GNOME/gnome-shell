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
 *
 * Authors:
 *  Matthew Allum  <mallum@openedhand.com>
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-bitmap.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-material.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-primitives.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "cogl-gles2-wrapper.h"


void
_cogl_texture_driver_bind (GLenum gl_target,
                           GLuint gl_handle,
                           GLenum gl_intformat)
{
  GE (cogl_gles2_wrapper_bind_texture (gl_target, gl_handle, gl_intformat));
}

void
_cogl_texture_driver_prep_gl_for_pixels_upload (int pixels_rowstride,
                                                int pixels_bpp)
{
  _cogl_texture_prep_gl_alignment_for_pixels_upload (pixels_rowstride);
}

void
_cogl_texture_driver_prep_gl_for_pixels_download (int pixels_rowstride,
                                                  int pixels_bpp)
{
  _cogl_texture_prep_gl_alignment_for_pixels_download (pixels_rowstride);
}

void
_cogl_texture_driver_upload_subregion_to_gl (CoglTexture *tex,
                                             int          src_x,
                                             int          src_y,
                                             int          dst_x,
                                             int          dst_y,
                                             int          width,
                                             int          height,
                                             CoglBitmap  *source_bmp,
				             GLuint       source_gl_format,
				             GLuint       source_gl_type,
                                             GLuint       gl_handle)
{
  int bpp = _cogl_get_format_bpp (source_bmp->format);
  CoglBitmap slice_bmp;

  /* NB: GLES doesn't support the GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS
   * or GL_UNPACK_SKIP_ROWS pixel store options so we can't directly source a
   * sub-region from source_bmp, we need to use a transient bitmap instead. */

  /* FIXME: optimize by not copying to intermediate slice bitmap when source
   * rowstride = bpp * width and the texture image is not sliced */

  /* Setup temp bitmap for slice subregion */
  slice_bmp.format = tex->bitmap.format;
  slice_bmp.width  = width;
  slice_bmp.height = height;
  slice_bmp.rowstride = bpp * slice_bmp.width;
  slice_bmp.data = (guchar*) g_malloc (slice_bmp.rowstride * slice_bmp.height);

  /* Setup gl alignment to match rowstride and top-left corner */
  _cogl_texture_driver_prep_gl_for_pixels_upload (slice_bmp.rowstride,
                                                  bpp);

  /* Copy subregion data */
  _cogl_bitmap_copy_subregion (source_bmp,
                               &slice_bmp,
                               src_x,
                               src_y,
                               0, 0,
                               slice_bmp.width,
                               slice_bmp.height);

  /* Upload new image data */
  GE( _cogl_texture_driver_bind (tex->gl_target,
                                 gl_handle, tex->gl_intformat) );

  GE( glTexSubImage2D (tex->gl_target, 0,
                       dst_x, dst_y,
                       width, height,
                       source_gl_format,
                       source_gl_type,
                       slice_bmp.data) );

  /* Free temp bitmap */
  g_free (slice_bmp.data);
}

/* NB: GLES doesn't support glGetTexImage2D, so cogl-texture will instead
 * fallback to a generic render + readpixels approach to downloading
 * texture data. (See _cogl_texture_draw_and_read() ) */
gboolean
_cogl_texture_driver_gl_get_tex_image (GLenum  gl_target,
                                       GLenum  dest_gl_format,
                                       GLenum  dest_gl_type,
                                       guint8 *dest)
{
  return FALSE;
}

gboolean
_cogl_texture_driver_size_supported (GLenum gl_target,
                                     GLenum gl_format,
                                     GLenum gl_type,
                                     int    width,
                                     int    height)
{
  return TRUE;
}

void
_cogl_texture_driver_try_setting_gl_border_color (
                                              GLuint   gl_target,
                                              const GLfloat *transparent_color)
{
  /* FAIL! */
}

gboolean
_cogl_pixel_format_from_gl_internal (GLenum            gl_int_format,
				     CoglPixelFormat  *out_format)
{
  return TRUE;
}

CoglPixelFormat
_cogl_pixel_format_to_gl (CoglPixelFormat  format,
			  GLenum          *out_glintformat,
			  GLenum          *out_glformat,
			  GLenum          *out_gltype)
{
  CoglPixelFormat required_format;
  GLenum          glintformat = 0;
  GLenum          glformat = 0;
  GLenum          gltype = 0;

  /* FIXME: check YUV support */

  required_format = format;

  /* Find GL equivalents */
  switch (format & COGL_UNPREMULT_MASK)
    {
    case COGL_PIXEL_FORMAT_A_8:
      glintformat = GL_ALPHA;
      glformat = GL_ALPHA;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_G_8:
      glintformat = GL_LUMINANCE;
      glformat = GL_LUMINANCE;
      gltype = GL_UNSIGNED_BYTE;
      break;

      /* Just one 24-bit ordering supported */
    case COGL_PIXEL_FORMAT_RGB_888:
    case COGL_PIXEL_FORMAT_BGR_888:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_BYTE;
      required_format = COGL_PIXEL_FORMAT_RGB_888;
      break;

      /* Just one 32-bit ordering supported */
    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ABGR_8888:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_BYTE;
      required_format = COGL_PIXEL_FORMAT_RGBA_8888;
      required_format |= (format & COGL_PREMULT_BIT);
      break;

      /* The following three types of channel ordering
       * are always defined using system word byte
       * ordering (even according to GLES spec) */
    case COGL_PIXEL_FORMAT_RGB_565:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_SHORT_5_6_5;
      break;
    case COGL_PIXEL_FORMAT_RGBA_4444:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_SHORT_4_4_4_4;
      break;
    case COGL_PIXEL_FORMAT_RGBA_5551:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_SHORT_5_5_5_1;
      break;

      /* FIXME: check extensions for YUV support */
    default:
      break;
    }

  if (out_glintformat != NULL)
    *out_glintformat = glintformat;
  if (out_glformat != NULL)
    *out_glformat = glformat;
  if (out_gltype != NULL)
    *out_gltype = gltype;

  return required_format;
}

gboolean
_cogl_texture_driver_allows_foreign_gl_target (GLenum gl_target)
{
  /* Allow 2-dimensional textures only */
  if (gl_target != GL_TEXTURE_2D)
    return FALSE;
  return TRUE;
}

void
_cogl_texture_driver_gl_generate_mipmaps (GLenum gl_target)
{
  GE( cogl_wrap_glGenerateMipmap (gl_target) );
}

CoglPixelFormat
_cogl_texture_driver_find_best_gl_get_data_format (
                                             CoglPixelFormat  format,
                                             GLenum          *closest_gl_format,
                                             GLenum          *closest_gl_type)
{
  /* Find closest format that's supported by GL
     (Can't use _cogl_pixel_format_to_gl since available formats
      when reading pixels on GLES are severely limited) */
  *closest_gl_format = GL_RGBA;
  *closest_gl_type = GL_UNSIGNED_BYTE;
  return COGL_PIXEL_FORMAT_RGBA_8888;
}

