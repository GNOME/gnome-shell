/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#include <string.h>
#include <math.h>

static void _cogl_texture_2d_free (CoglTexture2D *tex_2d);

COGL_HANDLE_DEFINE (Texture2D, texture_2d);

static const CoglTextureVtable cogl_texture_2d_vtable;

typedef struct _CoglTexture2DManualRepeatData
{
  CoglTexture2D *tex_2d;
  CoglTextureSliceCallback callback;
  void *user_data;
} CoglTexture2DManualRepeatData;

static void
_cogl_texture_2d_wrap_coords (float t_1, float t_2,
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
_cogl_texture_2d_manual_repeat_cb (const float *coords,
                                   void *user_data)
{
  CoglTexture2DManualRepeatData *data = user_data;
  float slice_coords[4];

  _cogl_texture_2d_wrap_coords (coords[0], coords[2],
                                slice_coords + 0, slice_coords + 2);
  _cogl_texture_2d_wrap_coords (coords[1], coords[3],
                                slice_coords + 1, slice_coords + 3);

  data->callback (COGL_TEXTURE (data->tex_2d),
                  data->tex_2d->gl_texture,
                  GL_TEXTURE_2D,
                  slice_coords,
                  coords,
                  data->user_data);
}

static void
_cogl_texture_2d_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglTextureSliceCallback callback,
                                       void *user_data)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);
  CoglTexture2DManualRepeatData data;

  data.tex_2d = tex_2d;
  data.callback = callback;
  data.user_data = user_data;

  /* We need to implement manual repeating because if Cogl is calling
     this function then it will set the wrap mode to GL_CLAMP_TO_EDGE
     and hardware repeating can't be done */
  _cogl_texture_iterate_manual_repeats (_cogl_texture_2d_manual_repeat_cb,
                                        virtual_tx_1, virtual_ty_1,
                                        virtual_tx_2, virtual_ty_2,
                                        &data);
}

static void
_cogl_texture_2d_set_wrap_mode_parameter (CoglTexture *tex,
                                          GLenum wrap_mode)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  /* Only set the wrap mode if it's different from the current
     value to avoid too many GL calls */
  if (tex_2d->wrap_mode != wrap_mode)
    {
      /* Any queued texture rectangles may be depending on the
       * previous wrap mode... */
      _cogl_journal_flush ();

      GE( glBindTexture (GL_TEXTURE_2D, tex_2d->gl_texture) );
      GE( glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode) );
      GE( glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode) );

      tex_2d->wrap_mode = wrap_mode;
    }
}

static void
_cogl_texture_2d_free (CoglTexture2D *tex_2d)
{
  GE( glDeleteTextures (1, &tex_2d->gl_texture) );
  g_free (tex_2d);
}

static gboolean
_cogl_texture_2d_is_pot (unsigned int num)
{
  gboolean have_bit = FALSE;

  /* Make sure there is only one bit set */
  while (num)
    {
      if (num & 1)
        {
          if (have_bit)
            return FALSE;
          have_bit = TRUE;
        }
      num >>= 1;
    }

  return TRUE;
}

static gboolean
_cogl_texture_2d_can_create (unsigned int width,
                             unsigned int height,
                             CoglPixelFormat internal_format)
{
  GLenum gl_intformat;
  GLenum gl_type;

  /* If the driver doesn't support glGenerateMipmap then we need to
     store a copy of the first pixels to cause an update. Instead of
     duplicating the code here we'll just make it fallback to
     CoglTexture2DSliced */
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return FALSE;

  /* If NPOT textures aren't supported then the size must be a power
     of two */
  if (!cogl_features_available (COGL_FEATURE_TEXTURE_NPOT) &&
      (!_cogl_texture_2d_is_pot (width) ||
       !_cogl_texture_2d_is_pot (height)))
    return FALSE;

  _cogl_pixel_format_to_gl (internal_format,
                            &gl_intformat,
                            NULL,
                            &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!_cogl_texture_driver_size_supported (GL_TEXTURE_2D,
                                            gl_intformat,
                                            gl_type,
                                            width,
                                            height))
    return FALSE;

  return TRUE;
}

static CoglTexture2D *
_cogl_texture_2d_create_base (unsigned int     width,
                              unsigned int     height,
                              CoglTextureFlags flags,
                              CoglPixelFormat  internal_format)
{
  CoglTexture2D *tex_2d = g_new (CoglTexture2D, 1);
  CoglTexture *tex = COGL_TEXTURE (tex_2d);

  tex->vtable = &cogl_texture_2d_vtable;

  tex_2d->width = width;
  tex_2d->height = height;
  tex_2d->mipmaps_dirty = TRUE;
  tex_2d->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;

  /* We default to GL_LINEAR for both filters */
  tex_2d->min_filter = GL_LINEAR;
  tex_2d->mag_filter = GL_LINEAR;

  /* Wrap mode not yet set */
  tex_2d->wrap_mode = GL_FALSE;

  tex_2d->format = internal_format;

  return tex_2d;
}

CoglHandle
_cogl_texture_2d_new_with_size (unsigned int     width,
                                unsigned int     height,
                                CoglTextureFlags flags,
                                CoglPixelFormat  internal_format)
{
  CoglTexture2D         *tex_2d;
  GLenum                 gl_intformat;
  GLenum                 gl_format;
  GLenum                 gl_type;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  if (!_cogl_texture_2d_can_create (width, height, internal_format))
    return COGL_INVALID_HANDLE;

  internal_format = _cogl_pixel_format_to_gl (internal_format,
                                              &gl_intformat,
                                              &gl_format,
                                              &gl_type);

  tex_2d = _cogl_texture_2d_create_base (width, height, flags, internal_format);

  _cogl_texture_driver_gen (GL_TEXTURE_2D, 1, &tex_2d->gl_texture);
  GE( glBindTexture (GL_TEXTURE_2D, tex_2d->gl_texture) );
  GE( glTexImage2D (GL_TEXTURE_2D, 0, gl_intformat,
                    width, height, 0, gl_format, gl_type, NULL) );

  return _cogl_texture_2d_handle_new (tex_2d);
}

CoglHandle
_cogl_texture_2d_new_from_bitmap (CoglHandle       bmp_handle,
                                  CoglTextureFlags flags,
                                  CoglPixelFormat  internal_format)
{
  CoglTexture2D *tex_2d;
  CoglBitmap    *bmp = (CoglBitmap *)bmp_handle;
  CoglBitmap     dst_bmp;
  gboolean       dst_bmp_owner;
  GLenum         gl_intformat;
  GLenum         gl_format;
  GLenum         gl_type;

  g_return_val_if_fail (bmp_handle != COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  if (!_cogl_texture_prepare_for_upload (bmp,
                                         internal_format,
                                         &internal_format,
                                         &dst_bmp,
                                         &dst_bmp_owner,
                                         &gl_intformat,
                                         &gl_format,
                                         &gl_type))
    return COGL_INVALID_HANDLE;

  tex_2d = _cogl_texture_2d_create_base (bmp->width,
                                         bmp->height,
                                         flags,
                                         internal_format);

  _cogl_texture_driver_gen (GL_TEXTURE_2D, 1, &tex_2d->gl_texture);
  _cogl_texture_driver_upload_to_gl (GL_TEXTURE_2D,
                                     tex_2d->gl_texture,
                                     &dst_bmp,
                                     gl_intformat,
                                     gl_format,
                                     gl_type);

  tex_2d->gl_format = gl_intformat;

  if (dst_bmp_owner)
    g_free (dst_bmp.data);

  return _cogl_texture_2d_handle_new (tex_2d);
}

static gint
_cogl_texture_2d_get_max_waste (CoglTexture *tex)
{
  return -1;
}

static gboolean
_cogl_texture_2d_is_sliced (CoglTexture *tex)
{
  return FALSE;
}

static gboolean
_cogl_texture_2d_can_hardware_repeat (CoglTexture *tex)
{
  return TRUE;
}

static void
_cogl_texture_2d_transform_coords_to_gl (CoglTexture *tex,
                                         float *s,
                                         float *t)
{
  /* The texture coordinates map directly so we don't need to do
     anything */
}

static gboolean
_cogl_texture_2d_transform_quad_coords_to_gl (CoglTexture *tex,
                                              float *coords)
{
  /* The texture coordinates map directly so we don't need to do
     anything */
  return TRUE;
}

static gboolean
_cogl_texture_2d_get_gl_texture (CoglTexture *tex,
                                 GLuint *out_gl_handle,
                                 GLenum *out_gl_target)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  if (out_gl_handle)
    *out_gl_handle = tex_2d->gl_texture;

  if (out_gl_target)
    *out_gl_target = GL_TEXTURE_2D;

  return TRUE;
}

static void
_cogl_texture_2d_set_filters (CoglTexture *tex,
                              GLenum       min_filter,
                              GLenum       mag_filter)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  if (min_filter == tex_2d->min_filter
      && mag_filter == tex_2d->mag_filter)
    return;

  /* Store new values */
  tex_2d->min_filter = min_filter;
  tex_2d->mag_filter = mag_filter;

  /* Apply new filters to the texture */
  GE( glBindTexture (GL_TEXTURE_2D, tex_2d->gl_texture) );
  GE( glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter) );
  GE( glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter) );
}

static void
_cogl_texture_2d_ensure_mipmaps (CoglTexture *tex)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Only update if the mipmaps are dirty */
  if (!tex_2d->auto_mipmap || !tex_2d->mipmaps_dirty)
    return;

  GE( glBindTexture (GL_TEXTURE_2D, tex_2d->gl_texture) );
  /* glGenerateMipmap is defined in the FBO extension. We only allow
     CoglTexture2D instances to be created if this feature is
     available so we don't need to check for the extension */
  _cogl_texture_driver_gl_generate_mipmaps (GL_TEXTURE_2D);

  tex_2d->mipmaps_dirty = FALSE;
}

static void
_cogl_texture_2d_ensure_non_quad_rendering (CoglTexture *tex)
{
  /* Nothing needs to be done */
}

static gboolean
_cogl_texture_2d_set_region (CoglTexture    *tex,
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
  CoglTexture2D   *tex_2d = COGL_TEXTURE_2D (tex);
  gint             bpp;
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
  source_bmp.data = (guchar*) data;

  /* Rowstride from width if none specified */
  bpp = _cogl_get_format_bpp (format);
  source_bmp.rowstride = (rowstride == 0) ? width * bpp : rowstride;

  /* Prepare the bitmap so that it will do the premultiplication
     conversion */
  _cogl_texture_prepare_for_upload (&source_bmp,
                                    tex_2d->format,
                                    NULL,
                                    &tmp_bmp,
                                    &tmp_bmp_owner,
                                    NULL,
                                    &closest_gl_format,
                                    &closest_gl_type);

  /* Send data to GL */
  _cogl_texture_driver_upload_subregion_to_gl (GL_TEXTURE_2D,
                                               tex_2d->gl_texture,
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
_cogl_texture_2d_get_data (CoglTexture     *tex,
                           CoglPixelFormat  format,
                           unsigned int     rowstride,
                           guint8          *data)
{
  CoglTexture2D   *tex_2d = COGL_TEXTURE_2D (tex);
  gint             bpp;
  gint             byte_size;
  CoglPixelFormat  closest_format;
  gint             closest_bpp;
  GLenum           closest_gl_format;
  GLenum           closest_gl_type;
  CoglBitmap       target_bmp;
  CoglBitmap       new_bmp;
  gboolean         success;
  guchar          *src;
  guchar          *dst;
  gint             y;

  /* Default to internal format if none specified */
  if (format == COGL_PIXEL_FORMAT_ANY)
    format = tex_2d->format;

  /* Rowstride from texture width if none specified */
  bpp = _cogl_get_format_bpp (format);
  if (rowstride == 0) rowstride = tex_2d->width * bpp;

  /* Return byte size if only that requested */
  byte_size =  tex_2d->height * rowstride;
  if (data == NULL) return byte_size;

  closest_format =
    _cogl_texture_driver_find_best_gl_get_data_format (format,
                                                       &closest_gl_format,
                                                       &closest_gl_type);
  closest_bpp = _cogl_get_format_bpp (closest_format);

  target_bmp.width = tex_2d->width;
  target_bmp.height = tex_2d->height;

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
      target_bmp.data = (guchar*) g_malloc (target_bmp.height
                                            * target_bmp.rowstride);
    }

  GE( glBindTexture (GL_TEXTURE_2D, tex_2d->gl_texture) );
  if (!_cogl_texture_driver_gl_get_tex_image (GL_TEXTURE_2D,
                                              closest_gl_format,
                                              closest_gl_type,
                                              target_bmp.data))
    {
      /* XXX: In some cases _cogl_texture_2d_download_from_gl may
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
      if (!success) return 0;

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
_cogl_texture_2d_get_format (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->format;
}

static GLenum
_cogl_texture_2d_get_gl_format (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->gl_format;
}

static gint
_cogl_texture_2d_get_width (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->width;
}

static gint
_cogl_texture_2d_get_height (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->height;
}

static const CoglTextureVtable
cogl_texture_2d_vtable =
  {
    _cogl_texture_2d_set_region,
    _cogl_texture_2d_get_data,
    _cogl_texture_2d_foreach_sub_texture_in_region,
    _cogl_texture_2d_get_max_waste,
    _cogl_texture_2d_is_sliced,
    _cogl_texture_2d_can_hardware_repeat,
    _cogl_texture_2d_transform_coords_to_gl,
    _cogl_texture_2d_transform_quad_coords_to_gl,
    _cogl_texture_2d_get_gl_texture,
    _cogl_texture_2d_set_filters,
    _cogl_texture_2d_ensure_mipmaps,
    _cogl_texture_2d_ensure_non_quad_rendering,
    _cogl_texture_2d_set_wrap_mode_parameter,
    _cogl_texture_2d_get_format,
    _cogl_texture_2d_get_gl_format,
    _cogl_texture_2d_get_width,
    _cogl_texture_2d_get_height
  };
