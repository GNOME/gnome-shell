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

static void
_cogl_texture_draw_and_read (CoglTexture *tex,
                             CoglBitmap  *target_bmp,
                             GLint       *viewport)
{
  gint        bpp;
  float       rx1, ry1;
  float       rx2, ry2;
  float       tx1, ty1;
  float       tx2, ty2;
  int         bw,  bh;
  CoglBitmap  rect_bmp;
  CoglHandle  handle;

  handle = (CoglHandle) tex;
  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  ry1 = 0; ry2 = 0;
  ty1 = 0; ty2 = 0;

  /* Walk Y axis until whole bitmap height consumed */
  for (bh = tex->bitmap.height; bh > 0; bh -= viewport[3])
    {
      /* Rectangle Y coords */
      ry1 = ry2;
      ry2 += (bh < viewport[3]) ? bh : viewport[3];

      /* Normalized texture Y coords */
      ty1 = ty2;
      ty2 = (ry2 / (float)tex->bitmap.height);

      rx1 = 0; rx2 = 0;
      tx1 = 0; tx2 = 0;

      /* Walk X axis until whole bitmap width consumed */
      for (bw = tex->bitmap.width; bw > 0; bw-=viewport[2])
        {
          /* Rectangle X coords */
          rx1 = rx2;
          rx2 += (bw < viewport[2]) ? bw : viewport[2];

          /* Normalized texture X coords */
          tx1 = tx2;
          tx2 = (rx2 / (float)tex->bitmap.width);

          /* Draw a portion of texture */
          cogl_rectangle_with_texture_coords (0, 0,
                                              rx2 - rx1,
                                              ry2 - ry1,
                                              tx1, ty1,
                                              tx2, ty2);

          /* Read into a temporary bitmap */
          rect_bmp.format = COGL_PIXEL_FORMAT_RGBA_8888;
          rect_bmp.width = rx2 - rx1;
          rect_bmp.height = ry2 - ry1;
          rect_bmp.rowstride = bpp * rect_bmp.width;
          rect_bmp.data = (guchar*) g_malloc (rect_bmp.rowstride *
                                              rect_bmp.height);

          _cogl_texture_driver_prep_gl_for_pixels_download (rect_bmp.rowstride,
                                                            bpp);
          GE( glReadPixels (viewport[0], viewport[1],
                            rect_bmp.width,
                            rect_bmp.height,
                            GL_RGBA, GL_UNSIGNED_BYTE,
                            rect_bmp.data) );

          /* Copy to target bitmap */
          _cogl_bitmap_copy_subregion (&rect_bmp,
                                       target_bmp,
                                       0,0,
                                       rx1,ry1,
                                       rect_bmp.width,
                                       rect_bmp.height);

          /* Free temp bitmap */
          g_free (rect_bmp.data);
        }
    }
}

gboolean
_cogl_texture_driver_download_from_gl (CoglTexture *tex,
                                       CoglBitmap  *target_bmp,
                                       GLuint       target_gl_format,
                                       GLuint       target_gl_type)
{
  gint       bpp;
  GLint      viewport[4];
  CoglBitmap alpha_bmp;
  CoglHandle prev_source;

  _COGL_GET_CONTEXT (ctx, FALSE);


  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  /* Viewport needs to have some size and be inside the window for this */
  GE( glGetIntegerv (GL_VIEWPORT, viewport) );

  if (viewport[0] <  0 || viewport[1] <  0 ||
      viewport[2] <= 0 || viewport[3] <= 0)
    return FALSE;

  /* Setup orthographic projection into current viewport
     (0,0 in bottom-left corner to draw the texture
     upside-down so we match the way glReadPixels works) */

  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_push ();
  _cogl_current_matrix_identity ();

  _cogl_current_matrix_ortho (0, (float)(viewport[2]),
                              0, (float)(viewport[3]),
                              (float)(0),
                              (float)(100));

  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_push ();
  _cogl_current_matrix_identity ();

  /* Direct copy operation */

  if (ctx->drv.texture_download_material == COGL_INVALID_HANDLE)
    {
      ctx->drv.texture_download_material = cogl_material_new ();
      cogl_material_set_blend (ctx->drv.texture_download_material,
                               "RGBA = ADD (SRC_COLOR, 0)",
                               NULL);
    }

  prev_source = cogl_handle_ref (ctx->source_material);
  cogl_set_source (ctx->drv.texture_download_material);

  cogl_material_set_layer (ctx->drv.texture_download_material, 0, tex);

  cogl_material_set_layer_combine (ctx->drv.texture_download_material,
                                   0, /* layer */
                                   "RGBA = REPLACE (TEXTURE)",
                                   NULL);

  _cogl_texture_draw_and_read (tex, target_bmp, viewport);

  /* Check whether texture has alpha and framebuffer not */
  /* FIXME: For some reason even if ALPHA_BITS is 8, the framebuffer
     still doesn't seem to have an alpha buffer. This might be just
     a PowerVR issue.
  GLint r_bits, g_bits, b_bits, a_bits;
  GE( glGetIntegerv (GL_ALPHA_BITS, &a_bits) );
  GE( glGetIntegerv (GL_RED_BITS, &r_bits) );
  GE( glGetIntegerv (GL_GREEN_BITS, &g_bits) );
  GE( glGetIntegerv (GL_BLUE_BITS, &b_bits) );
  printf ("R bits: %d\n", r_bits);
  printf ("G bits: %d\n", g_bits);
  printf ("B bits: %d\n", b_bits);
  printf ("A bits: %d\n", a_bits); */
  if ((tex->bitmap.format & COGL_A_BIT)/* && a_bits == 0*/)
    {
      guchar *srcdata;
      guchar *dstdata;
      guchar *srcpixel;
      guchar *dstpixel;
      gint    x,y;

      /* Create temp bitmap for alpha values */
      alpha_bmp.format = COGL_PIXEL_FORMAT_RGBA_8888;
      alpha_bmp.width = target_bmp->width;
      alpha_bmp.height = target_bmp->height;
      alpha_bmp.rowstride = bpp * alpha_bmp.width;
      alpha_bmp.data = (guchar*) g_malloc (alpha_bmp.rowstride *
                                           alpha_bmp.height);

      /* Draw alpha values into RGB channels */
      cogl_material_set_layer_combine (ctx->drv.texture_download_material,
                                       0, /* layer */
                                       "RGBA = REPLACE (TEXTURE[A])",
                                       NULL);

      _cogl_texture_draw_and_read (tex, &alpha_bmp, viewport);

      /* Copy temp R to target A */
      srcdata = alpha_bmp.data;
      dstdata = target_bmp->data;

      for (y=0; y<target_bmp->height; ++y)
        {
          for (x=0; x<target_bmp->width; ++x)
            {
              srcpixel = srcdata + x*bpp;
              dstpixel = dstdata + x*bpp;
              dstpixel[3] = srcpixel[0];
            }
          srcdata += alpha_bmp.rowstride;
          dstdata += target_bmp->rowstride;
        }

      g_free (alpha_bmp.data);
    }

  /* Restore old state */
  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_pop ();
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_pop ();

  /* restore the original material */
  cogl_set_source (prev_source);
  cogl_handle_unref (prev_source);

  return TRUE;
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

