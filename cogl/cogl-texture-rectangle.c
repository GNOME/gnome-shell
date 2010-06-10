/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-journal-private.h"

#include <string.h>
#include <math.h>

/* These aren't defined under GLES */
#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif
#ifndef GL_CLAMP
#define GL_CLAMP                 0x2900
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER       0x812D
#endif

static void _cogl_texture_rectangle_free (CoglTextureRectangle *tex_rect);

COGL_TEXTURE_DEFINE (TextureRectangle, texture_rectangle);

static const CoglTextureVtable cogl_texture_rectangle_vtable;

typedef struct _CoglTextureRectangleManualRepeatData
{
  CoglTextureRectangle *tex_rect;
  CoglTextureSliceCallback callback;
  void *user_data;
} CoglTextureRectangleManualRepeatData;

static void
_cogl_texture_rectangle_wrap_coords (float t_1, float t_2,
                                     float *out_t_1, float *out_t_2)
{
  float int_part;

  /* Wrap t_1 and t_2 to the range [0,1] */

  modff (t_1 < t_2 ? t_1 : t_2, &int_part);
  t_1 -= int_part;
  t_2 -= int_part;
  if (cogl_util_float_signbit (int_part))
    {
      *out_t_1 = 1.0f + t_1;
      *out_t_2 = 1.0f + t_2;
    }
  else
    {
      *out_t_1 = t_1;
      *out_t_2 = t_2;
    }
}

static void
_cogl_texture_rectangle_manual_repeat_cb (const float *coords,
                                          void *user_data)
{
  CoglTextureRectangleManualRepeatData *data = user_data;
  float slice_coords[4];

  _cogl_texture_rectangle_wrap_coords (coords[0], coords[2],
                                       slice_coords + 0, slice_coords + 2);
  _cogl_texture_rectangle_wrap_coords (coords[1], coords[3],
                                       slice_coords + 1, slice_coords + 3);

  slice_coords[0] *= data->tex_rect->width;
  slice_coords[1] *= data->tex_rect->height;
  slice_coords[2] *= data->tex_rect->width;
  slice_coords[3] *= data->tex_rect->height;

  data->callback (COGL_TEXTURE (data->tex_rect),
                  data->tex_rect->gl_texture,
                  GL_TEXTURE_RECTANGLE_ARB,
                  slice_coords,
                  coords,
                  data->user_data);
}

static void
_cogl_texture_rectangle_foreach_sub_texture_in_region (
                                           CoglTexture *tex,
                                           float virtual_tx_1,
                                           float virtual_ty_1,
                                           float virtual_tx_2,
                                           float virtual_ty_2,
                                           CoglTextureSliceCallback callback,
                                           void *user_data)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  CoglTextureRectangleManualRepeatData data;

  data.tex_rect = tex_rect;
  data.callback = callback;
  data.user_data = user_data;

  /* We need to implement manual repeating because if Cogl is calling
     this function then it will set the wrap mode to GL_CLAMP_TO_EDGE
     and hardware repeating can't be done */
  _cogl_texture_iterate_manual_repeats
    (_cogl_texture_rectangle_manual_repeat_cb,
     virtual_tx_1, virtual_ty_1, virtual_tx_2, virtual_ty_2,
     &data);
}

static gboolean
can_use_wrap_mode (GLenum wrap_mode)
{
  return (wrap_mode == GL_CLAMP ||
          wrap_mode == GL_CLAMP_TO_EDGE ||
          wrap_mode == GL_CLAMP_TO_BORDER);
}

static void
_cogl_texture_rectangle_set_wrap_mode_parameters (CoglTexture *tex,
                                                  GLenum wrap_mode_s,
                                                  GLenum wrap_mode_t,
                                                  GLenum wrap_mode_r)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. Texture rectangle doesn't make use of
     the r coordinate so we can ignore its wrap mode */
  if (tex_rect->wrap_mode_s != wrap_mode_s ||
      tex_rect->wrap_mode_t != wrap_mode_t)
    {
      g_assert (can_use_wrap_mode (wrap_mode_s));
      g_assert (can_use_wrap_mode (wrap_mode_t));

      GE( _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                           tex_rect->gl_texture,
                                           FALSE) );
      GE( glTexParameteri (GL_TEXTURE_RECTANGLE_ARB,
                           GL_TEXTURE_WRAP_S, wrap_mode_s) );
      GE( glTexParameteri (GL_TEXTURE_RECTANGLE_ARB,
                           GL_TEXTURE_WRAP_T, wrap_mode_t) );

      tex_rect->wrap_mode_s = wrap_mode_s;
      tex_rect->wrap_mode_t = wrap_mode_t;
    }
}

static void
_cogl_texture_rectangle_free (CoglTextureRectangle *tex_rect)
{
  _cogl_delete_gl_texture (tex_rect->gl_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_rect));
}

static gboolean
_cogl_texture_rectangle_can_create (unsigned int width,
                                    unsigned int height,
                                    CoglPixelFormat internal_format)
{
  GLenum gl_intformat;
  GLenum gl_type;

  if (!cogl_features_available (COGL_FEATURE_TEXTURE_RECTANGLE))
    return FALSE;

  _cogl_pixel_format_to_gl (internal_format,
                            &gl_intformat,
                            NULL,
                            &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!_cogl_texture_driver_size_supported (GL_TEXTURE_RECTANGLE_ARB,
                                            gl_intformat,
                                            gl_type,
                                            width,
                                            height))
    return FALSE;

  return TRUE;
}

static CoglTextureRectangle *
_cogl_texture_rectangle_create_base (unsigned int     width,
                                     unsigned int     height,
                                     CoglTextureFlags flags,
                                     CoglPixelFormat  internal_format)
{
  CoglTextureRectangle *tex_rect = g_new (CoglTextureRectangle, 1);
  CoglTexture *tex = COGL_TEXTURE (tex_rect);

  tex->vtable = &cogl_texture_rectangle_vtable;

  tex_rect->width = width;
  tex_rect->height = height;

  /* We default to GL_LINEAR for both filters */
  tex_rect->min_filter = GL_LINEAR;
  tex_rect->mag_filter = GL_LINEAR;

  /* Wrap mode not yet set */
  tex_rect->wrap_mode_s = GL_FALSE;
  tex_rect->wrap_mode_t = GL_FALSE;

  tex_rect->format = internal_format;

  return tex_rect;
}

CoglHandle
_cogl_texture_rectangle_new_with_size (unsigned int     width,
                                       unsigned int     height,
                                       CoglTextureFlags flags,
                                       CoglPixelFormat  internal_format)
{
  CoglTextureRectangle *tex_rect;
  GLenum                gl_intformat;
  GLenum                gl_format;
  GLenum                gl_type;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  if (!_cogl_texture_rectangle_can_create (width, height, internal_format))
    return COGL_INVALID_HANDLE;

  internal_format = _cogl_pixel_format_to_gl (internal_format,
                                              &gl_intformat,
                                              &gl_format,
                                              &gl_type);

  tex_rect = _cogl_texture_rectangle_create_base (width, height, flags,
                                                  internal_format);

  _cogl_texture_driver_gen (GL_TEXTURE_RECTANGLE_ARB, 1, &tex_rect->gl_texture);
  GE( _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                       tex_rect->gl_texture,
                                       FALSE) );
  GE( glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, gl_intformat,
                    width, height, 0, gl_format, gl_type, NULL) );

  return _cogl_texture_rectangle_handle_new (tex_rect);
}

CoglHandle
_cogl_texture_rectangle_new_from_bitmap (CoglHandle       bmp_handle,
                                         CoglTextureFlags flags,
                                         CoglPixelFormat  internal_format)
{
  CoglTextureRectangle *tex_rect;
  CoglBitmap           *bmp = (CoglBitmap *)bmp_handle;
  CoglBitmap            dst_bmp;
  gboolean              dst_bmp_owner;
  GLenum                gl_intformat;
  GLenum                gl_format;
  GLenum                gl_type;

  g_return_val_if_fail (bmp_handle != COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  internal_format = _cogl_texture_determine_internal_format (bmp->format,
                                                             internal_format);

  if (!_cogl_texture_rectangle_can_create (bmp->width, bmp->height,
                                           internal_format))
    return COGL_INVALID_HANDLE;

  if (!_cogl_texture_prepare_for_upload (bmp,
                                         internal_format,
                                         &internal_format,
                                         &dst_bmp,
                                         &dst_bmp_owner,
                                         &gl_intformat,
                                         &gl_format,
                                         &gl_type))
    return COGL_INVALID_HANDLE;

  tex_rect = _cogl_texture_rectangle_create_base (bmp->width,
                                                  bmp->height,
                                                  flags,
                                                  internal_format);

  _cogl_texture_driver_gen (GL_TEXTURE_RECTANGLE_ARB, 1, &tex_rect->gl_texture);
  _cogl_texture_driver_upload_to_gl (GL_TEXTURE_RECTANGLE_ARB,
                                     tex_rect->gl_texture,
                                     FALSE,
                                     &dst_bmp,
                                     gl_intformat,
                                     gl_format,
                                     gl_type);

  tex_rect->gl_format = gl_intformat;

  if (dst_bmp_owner)
    g_free (dst_bmp.data);

  return _cogl_texture_rectangle_handle_new (tex_rect);
}

static int
_cogl_texture_rectangle_get_max_waste (CoglTexture *tex)
{
  return -1;
}

static gboolean
_cogl_texture_rectangle_is_sliced (CoglTexture *tex)
{
  return FALSE;
}

static gboolean
_cogl_texture_rectangle_can_hardware_repeat (CoglTexture *tex)
{
  return FALSE;
}

static void
_cogl_texture_rectangle_transform_coords_to_gl (CoglTexture *tex,
                                                float *s,
                                                float *t)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);

  *s *= tex_rect->width;
  *t *= tex_rect->height;
}

static CoglTransformResult
_cogl_texture_rectangle_transform_quad_coords_to_gl (CoglTexture *tex,
                                                     float *coords)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  gboolean need_repeat = FALSE;
  int i;

  for (i = 0; i < 4; i++)
    {
      if (coords[i] < 0.0f || coords[i] > 1.0f)
        need_repeat = TRUE;
      coords[i] *= (i & 1) ? tex_rect->height : tex_rect->width;
    }

  return (need_repeat ? COGL_TRANSFORM_SOFTWARE_REPEAT
          : COGL_TRANSFORM_NO_REPEAT);
}

static gboolean
_cogl_texture_rectangle_get_gl_texture (CoglTexture *tex,
                                        GLuint *out_gl_handle,
                                        GLenum *out_gl_target)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);

  if (out_gl_handle)
    *out_gl_handle = tex_rect->gl_texture;

  if (out_gl_target)
    *out_gl_target = GL_TEXTURE_RECTANGLE_ARB;

  return TRUE;
}

static void
_cogl_texture_rectangle_set_filters (CoglTexture *tex,
                                     GLenum       min_filter,
                                     GLenum       mag_filter)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);

  if (min_filter == tex_rect->min_filter
      && mag_filter == tex_rect->mag_filter)
    return;

  /* Rectangle textures don't support mipmapping */
  g_assert (min_filter == GL_LINEAR || min_filter == GL_NEAREST);

  /* Store new values */
  tex_rect->min_filter = min_filter;
  tex_rect->mag_filter = mag_filter;

  /* Apply new filters to the texture */
  GE( _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                       tex_rect->gl_texture,
                                       FALSE) );
  GE( glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
                       mag_filter) );
  GE( glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
                       min_filter) );
}

static void
_cogl_texture_rectangle_pre_paint (CoglTexture *tex,
                                   CoglTexturePrePaintFlags flags)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Rectangle textures don't support mipmaps */
  g_assert ((flags & COGL_TEXTURE_NEEDS_MIPMAP) == 0);
}

static void
_cogl_texture_rectangle_ensure_non_quad_rendering (CoglTexture *tex)
{
  /* Nothing needs to be done */
}

static gboolean
_cogl_texture_rectangle_set_region (CoglTexture    *tex,
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
                                    const guint8   *data)
{
  CoglTextureRectangle   *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  int              bpp;
  CoglBitmap       source_bmp;
  CoglBitmap       tmp_bmp;
  gboolean         tmp_bmp_owner = FALSE;
  GLenum           closest_gl_format;
  GLenum           closest_gl_type;

  /* Check for valid format */
  if (format == COGL_PIXEL_FORMAT_ANY)
    return FALSE;

  /* Shortcut out early if the image is empty */
  if (width == 0 || height == 0)
    return TRUE;

  /* Init source bitmap */
  source_bmp.width = width;
  source_bmp.height = height;
  source_bmp.format = format;
  source_bmp.data = (guint8 *)data;

  /* Rowstride from width if none specified */
  bpp = _cogl_get_format_bpp (format);
  source_bmp.rowstride = (rowstride == 0) ? width * bpp : rowstride;

  /* Prepare the bitmap so that it will do the premultiplication
     conversion */
  _cogl_texture_prepare_for_upload (&source_bmp,
                                    tex_rect->format,
                                    NULL,
                                    &tmp_bmp,
                                    &tmp_bmp_owner,
                                    NULL,
                                    &closest_gl_format,
                                    &closest_gl_type);

  /* Send data to GL */
  _cogl_texture_driver_upload_subregion_to_gl (GL_TEXTURE_RECTANGLE_ARB,
                                               tex_rect->gl_texture,
                                               FALSE,
                                               src_x, src_y,
                                               dst_x, dst_y,
                                               dst_width, dst_height,
                                               &tmp_bmp,
                                               closest_gl_format,
                                               closest_gl_type);

  /* Free data if owner */
  if (tmp_bmp_owner)
    g_free (tmp_bmp.data);

  return TRUE;
}

static int
_cogl_texture_rectangle_get_data (CoglTexture     *tex,
                                  CoglPixelFormat  format,
                                  unsigned int     rowstride,
                                  guint8          *data)
{
  CoglTextureRectangle   *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  int              bpp;
  int              byte_size;
  CoglPixelFormat  closest_format;
  int              closest_bpp;
  GLenum           closest_gl_format;
  GLenum           closest_gl_type;
  CoglBitmap       target_bmp;
  CoglBitmap       new_bmp;
  gboolean         success;
  guint8          *src;
  guint8          *dst;
  int              y;

  /* Default to internal format if none specified */
  if (format == COGL_PIXEL_FORMAT_ANY)
    format = tex_rect->format;

  /* Rowstride from texture width if none specified */
  bpp = _cogl_get_format_bpp (format);
  if (rowstride == 0)
    rowstride = tex_rect->width * bpp;

  /* Return byte size if only that requested */
  byte_size =  tex_rect->height * rowstride;
  if (data == NULL)
    return byte_size;

  closest_format =
    _cogl_texture_driver_find_best_gl_get_data_format (format,
                                                       &closest_gl_format,
                                                       &closest_gl_type);
  closest_bpp = _cogl_get_format_bpp (closest_format);

  target_bmp.width = tex_rect->width;
  target_bmp.height = tex_rect->height;

  /* Is the requested format supported? */
  if (closest_format == format)
    {
      /* Target user data directly */
      target_bmp.format = format;
      target_bmp.rowstride = rowstride;
      target_bmp.data = data;
    }
  else
    {
      /* Target intermediate buffer */
      target_bmp.format = closest_format;
      target_bmp.rowstride = target_bmp.width * closest_bpp;
      target_bmp.data = g_malloc (target_bmp.height * target_bmp.rowstride);
    }

  _cogl_texture_driver_prep_gl_for_pixels_download (target_bmp.rowstride,
                                                    closest_bpp);

  GE( _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                       tex_rect->gl_texture,
                                       FALSE) );
  if (!_cogl_texture_driver_gl_get_tex_image (GL_TEXTURE_RECTANGLE_ARB,
                                              closest_gl_format,
                                              closest_gl_type,
                                              target_bmp.data))
    {
      /* XXX: In some cases _cogl_texture_rectangle_download_from_gl may
       * fail to read back the texture data; such as for GLES which doesn't
       * support glGetTexImage, so here we fallback to drawing the texture
       * and reading the pixels from the framebuffer. */
      _cogl_texture_draw_and_read (tex, &target_bmp,
                                   closest_gl_format,
                                   closest_gl_type);
    }

  /* Was intermediate used? */
  if (closest_format != format)
    {
      /* Convert to requested format */
      success = _cogl_bitmap_convert_format_and_premult (&target_bmp,
                                                         &new_bmp,
                                                         format);

      /* Free intermediate data and return if failed */
      g_free (target_bmp.data);
      if (!success)
        return 0;

      /* Copy to user buffer */
      for (y = 0; y < new_bmp.height; ++y)
        {
          src = new_bmp.data + y * new_bmp.rowstride;
          dst = data + y * rowstride;
          memcpy (dst, src, new_bmp.width);
        }

      /* Free converted data */
      g_free (new_bmp.data);
    }

  return byte_size;
}

static CoglPixelFormat
_cogl_texture_rectangle_get_format (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->format;
}

static GLenum
_cogl_texture_rectangle_get_gl_format (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->gl_format;
}

static int
_cogl_texture_rectangle_get_width (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->width;
}

static int
_cogl_texture_rectangle_get_height (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->height;
}

static const CoglTextureVtable
cogl_texture_rectangle_vtable =
  {
    _cogl_texture_rectangle_set_region,
    _cogl_texture_rectangle_get_data,
    _cogl_texture_rectangle_foreach_sub_texture_in_region,
    _cogl_texture_rectangle_get_max_waste,
    _cogl_texture_rectangle_is_sliced,
    _cogl_texture_rectangle_can_hardware_repeat,
    _cogl_texture_rectangle_transform_coords_to_gl,
    _cogl_texture_rectangle_transform_quad_coords_to_gl,
    _cogl_texture_rectangle_get_gl_texture,
    _cogl_texture_rectangle_set_filters,
    _cogl_texture_rectangle_pre_paint,
    _cogl_texture_rectangle_ensure_non_quad_rendering,
    _cogl_texture_rectangle_set_wrap_mode_parameters,
    _cogl_texture_rectangle_get_format,
    _cogl_texture_rectangle_get_gl_format,
    _cogl_texture_rectangle_get_width,
    _cogl_texture_rectangle_get_height,
    NULL /* is_foreign */
  };
