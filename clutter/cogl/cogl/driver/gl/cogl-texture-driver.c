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

#define glGenerateMipmap ctx->drv.pf_glGenerateMipmap

void
_cogl_texture_driver_bind (GLenum gl_target,
                           GLuint gl_handle,
                           GLenum gl_intformat)
{
  GE (glBindTexture (gl_target, gl_handle));
}

/* OpenGL - unlike GLES - can upload a sub region of pixel data from a larger
 * source buffer */
static void
prep_gl_for_pixels_upload_full (int pixels_rowstride,
                                int pixels_src_x,
                                int pixels_src_y,
                                int pixels_bpp)
{
  GE( glPixelStorei (GL_UNPACK_ROW_LENGTH, pixels_rowstride / pixels_bpp) );

  GE( glPixelStorei (GL_UNPACK_SKIP_PIXELS, pixels_src_x) );
  GE( glPixelStorei (GL_UNPACK_SKIP_ROWS, pixels_src_y) );

  _cogl_texture_prep_gl_alignment_for_pixels_upload (pixels_rowstride);
}

void
_cogl_texture_driver_prep_gl_for_pixels_upload (int pixels_rowstride,
                                                int pixels_bpp)
{
  prep_gl_for_pixels_upload_full (pixels_rowstride, 0, 0, pixels_bpp);
}

/* OpenGL - unlike GLES - can download pixel data into a sub region of
 * a larger destination buffer */
static void
prep_gl_for_pixels_download_full (int pixels_rowstride,
                                  int pixels_src_x,
                                  int pixels_src_y,
                                  int pixels_bpp)
{
  GE( glPixelStorei (GL_PACK_ROW_LENGTH, pixels_rowstride / pixels_bpp) );

  GE( glPixelStorei (GL_PACK_SKIP_PIXELS, pixels_src_x) );
  GE( glPixelStorei (GL_PACK_SKIP_ROWS, pixels_src_y) );

  _cogl_texture_prep_gl_alignment_for_pixels_download (pixels_rowstride);
}

void
_cogl_texture_driver_prep_gl_for_pixels_download (int pixels_rowstride,
                                                  int pixels_bpp)
{
  prep_gl_for_pixels_download_full (pixels_rowstride, 0, 0, pixels_bpp);
}

void
_cogl_texture_driver_upload_subregion_to_gl (GLenum       gl_target,
                                             GLuint       gl_handle,
                                             int          src_x,
                                             int          src_y,
                                             int          dst_x,
                                             int          dst_y,
                                             int          width,
                                             int          height,
                                             CoglBitmap  *source_bmp,
				             GLuint       source_gl_format,
				             GLuint       source_gl_type)
{
  int bpp = _cogl_get_format_bpp (source_bmp->format);

  /* Setup gl alignment to match rowstride and top-left corner */
  prep_gl_for_pixels_upload_full (source_bmp->rowstride,
                                  src_x,
                                  src_y,
                                  bpp);

  /* We don't need to use _cogl_texture_driver_bind here because we're
     not using the bound texture to render yet */
  GE( glBindTexture (gl_target, gl_handle) );

  GE( glTexSubImage2D (gl_target, 0,
                       dst_x, dst_y,
                       width, height,
                       source_gl_format,
                       source_gl_type,
                       source_bmp->data) );
}

void
_cogl_texture_driver_upload_to_gl (GLenum       gl_target,
                                   GLuint       gl_handle,
                                   CoglBitmap  *source_bmp,
                                   GLint        internal_gl_format,
                                   GLuint       source_gl_format,
                                   GLuint       source_gl_type)
{
  int bpp = _cogl_get_format_bpp (source_bmp->format);

  /* Setup gl alignment to match rowstride and top-left corner */
  prep_gl_for_pixels_upload_full (source_bmp->rowstride, 0, 0, bpp);

  /* We don't need to use _cogl_texture_driver_bind here because we're
     not using the bound texture to render yet */
  GE( glBindTexture (gl_target, gl_handle) );

  GE( glTexImage2D (gl_target, 0,
                    internal_gl_format,
                    source_bmp->width, source_bmp->height,
                    0,
                    source_gl_format,
                    source_gl_type,
                    source_bmp->data) );
}

gboolean
_cogl_texture_driver_gl_get_tex_image (GLenum  gl_target,
                                       GLenum  dest_gl_format,
                                       GLenum  dest_gl_type,
                                       guint8 *dest)
{
  GE (glGetTexImage (gl_target,
                     0, /* level */
                     dest_gl_format,
                     dest_gl_type,
                     (GLvoid *)dest));
  return TRUE;
}

gboolean
_cogl_texture_driver_size_supported (GLenum gl_target,
                                     GLenum gl_format,
                                     GLenum gl_type,
                                     int    width,
                                     int    height)
{
  if (gl_target ==  GL_TEXTURE_2D)
    {
      /* Proxy texture allows for a quick check for supported size */

      GLint new_width = 0;

      GE( glTexImage2D (GL_PROXY_TEXTURE_2D, 0, GL_RGBA,
			width, height, 0 /* border */,
			gl_format, gl_type, NULL) );

      GE( glGetTexLevelParameteriv (GL_PROXY_TEXTURE_2D, 0,
				    GL_TEXTURE_WIDTH, &new_width) );

      return new_width != 0;
    }
  else
    {
      /* not used */
      return 0;
    }
}

void
_cogl_texture_driver_try_setting_gl_border_color (
                                              GLuint   gl_target,
                                              const GLfloat *transparent_color)
{
  /* Use a transparent border color so that we can leave the
     color buffer alone when using texture co-ordinates
     outside of the texture */
  GE( glTexParameterfv (gl_target, GL_TEXTURE_BORDER_COLOR,
                        transparent_color) );
}

gboolean
_cogl_pixel_format_from_gl_internal (GLenum            gl_int_format,
				     CoglPixelFormat  *out_format)
{
  /* It doesn't really matter we convert to exact same
     format (some have no cogl match anyway) since format
     is re-matched against cogl when getting or setting
     texture image data.
  */

  switch (gl_int_format)
    {
    case GL_ALPHA: case GL_ALPHA4: case GL_ALPHA8:
    case GL_ALPHA12: case GL_ALPHA16:

      *out_format = COGL_PIXEL_FORMAT_A_8;
      return TRUE;

    case GL_LUMINANCE: case GL_LUMINANCE4: case GL_LUMINANCE8:
    case GL_LUMINANCE12: case GL_LUMINANCE16:

      *out_format = COGL_PIXEL_FORMAT_G_8;
      return TRUE;

    case GL_RGB: case GL_RGB4: case GL_RGB5: case GL_RGB8:
    case GL_RGB10: case GL_RGB12: case GL_RGB16: case GL_R3_G3_B2:

      *out_format = COGL_PIXEL_FORMAT_RGB_888;
      return TRUE;

    case GL_RGBA: case GL_RGBA2: case GL_RGBA4: case GL_RGB5_A1:
    case GL_RGBA8: case GL_RGB10_A2: case GL_RGBA12: case GL_RGBA16:

      *out_format = COGL_PIXEL_FORMAT_RGBA_8888;
      return TRUE;
    }

  return FALSE;
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

    case COGL_PIXEL_FORMAT_RGB_888:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_BGR_888:
      glintformat = GL_RGB;
      glformat = GL_BGR;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_RGBA_8888:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_BGRA_8888:
      glintformat = GL_RGBA;
      glformat = GL_BGRA;
      gltype = GL_UNSIGNED_BYTE;
      break;

      /* The following two types of channel ordering
       * have no GL equivalent unless defined using
       * system word byte ordering */
    case COGL_PIXEL_FORMAT_ARGB_8888:
      glintformat = GL_RGBA;
      glformat = GL_BGRA;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
      gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
      break;

    case COGL_PIXEL_FORMAT_ABGR_8888:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
      gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
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
  /* GL_ARB_texture_rectangle textures are supported if they are
     created from foreign because some chipsets have trouble with
     GL_ARB_texture_non_power_of_two. There is no Cogl call to create
     them directly to emphasize the fact that they don't work fully
     (for example, no mipmapping and complicated shader support) */

  /* Allow 2-dimensional or rectangle textures only */
  if (gl_target != GL_TEXTURE_2D && gl_target != CGL_TEXTURE_RECTANGLE_ARB)
    return FALSE;

  return TRUE;
}

void
_cogl_texture_driver_gl_generate_mipmaps (GLenum gl_target)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( glGenerateMipmap (gl_target) );
}

CoglPixelFormat
_cogl_texture_driver_find_best_gl_get_data_format (
                                             CoglPixelFormat  format,
                                             GLenum          *closest_gl_format,
                                             GLenum          *closest_gl_type)
{
  /* Find closest format that's supported by GL */
  return _cogl_pixel_format_to_gl (format,
                                   NULL, /* don't need */
                                   closest_gl_format,
                                   closest_gl_type);
}

