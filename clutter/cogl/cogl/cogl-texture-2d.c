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
#include "cogl-texture-2d-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-journal-private.h"
#include "cogl-material-private.h"

#include <string.h>
#include <math.h>

static void _cogl_texture_2d_free (CoglTexture2D *tex_2d);

COGL_TEXTURE_INTERNAL_DEFINE (Texture2D, texture_2d);

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
_cogl_texture_2d_set_wrap_mode_parameters (CoglTexture *tex,
                                           GLenum wrap_mode_s,
                                           GLenum wrap_mode_t,
                                           GLenum wrap_mode_r)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. Texture 2D doesn't make use of the r
     coordinate so we can ignore its wrap mode */
  if (tex_2d->wrap_mode_s != wrap_mode_s ||
      tex_2d->wrap_mode_t != wrap_mode_t)
    {
      _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                       tex_2d->gl_texture,
                                       FALSE);
      GE( glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode_s) );
      GE( glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode_t) );

      tex_2d->wrap_mode_s = wrap_mode_s;
      tex_2d->wrap_mode_t = wrap_mode_t;
    }
}

static void
_cogl_texture_2d_free (CoglTexture2D *tex_2d)
{
  _cogl_delete_gl_texture (tex_2d->gl_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_2d));
}

static gboolean
_cogl_texture_2d_is_pot (unsigned int num)
{
  /* Make sure there is only one bit set */
  return (num & (num - 1)) == 0;
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
  tex_2d->wrap_mode_s = GL_FALSE;
  tex_2d->wrap_mode_t = GL_FALSE;

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
  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   FALSE);
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

  internal_format = _cogl_texture_determine_internal_format (bmp->format,
                                                             internal_format);

  if (!_cogl_texture_2d_can_create (bmp->width, bmp->height, internal_format))
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

  tex_2d = _cogl_texture_2d_create_base (bmp->width,
                                         bmp->height,
                                         flags,
                                         internal_format);

  _cogl_texture_driver_gen (GL_TEXTURE_2D, 1, &tex_2d->gl_texture);
  _cogl_texture_driver_upload_to_gl (GL_TEXTURE_2D,
                                     tex_2d->gl_texture,
                                     FALSE,
                                     &dst_bmp,
                                     gl_intformat,
                                     gl_format,
                                     gl_type);

  tex_2d->gl_format = gl_intformat;

  if (dst_bmp_owner)
    g_free (dst_bmp.data);

  return _cogl_texture_2d_handle_new (tex_2d);
}

void
_cogl_texture_2d_externally_modified (CoglHandle handle)
{
  if (!_cogl_is_texture_2d (handle))
    return;

  COGL_TEXTURE_2D (handle)->mipmaps_dirty = TRUE;
}

static int
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

static CoglTransformResult
_cogl_texture_2d_transform_quad_coords_to_gl (CoglTexture *tex,
                                              float *coords)
{
  /* The texture coordinates map directly so we don't need to do
     anything other than check for repeats */

  gboolean need_repeat = FALSE;
  int i;

  for (i = 0; i < 4; i++)
    if (coords[i] < 0.0f || coords[i] > 1.0f)
      need_repeat = TRUE;

  return (need_repeat ? COGL_TRANSFORM_HARDWARE_REPEAT
          : COGL_TRANSFORM_NO_REPEAT);
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
  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   FALSE);
  GE( glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter) );
  GE( glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter) );
}

static void
_cogl_texture_2d_pre_paint (CoglTexture *tex, CoglTexturePrePaintFlags flags)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Only update if the mipmaps are dirty */
  if ((flags & COGL_TEXTURE_NEEDS_MIPMAP) &&
      tex_2d->auto_mipmap && tex_2d->mipmaps_dirty)
    {
      _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                       tex_2d->gl_texture,
                                       FALSE);
      /* glGenerateMipmap is defined in the FBO extension. We only allow
         CoglTexture2D instances to be created if this feature is
         available so we don't need to check for the extension */
      _cogl_texture_driver_gl_generate_mipmaps (GL_TEXTURE_2D);

      tex_2d->mipmaps_dirty = FALSE;
    }
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
                             CoglBitmap     *bmp)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);
  GLenum         gl_format;
  GLenum         gl_type;

  _cogl_pixel_format_to_gl (bmp->format,
                            NULL, /* internal format */
                            &gl_format,
                            &gl_type);

  /* Send data to GL */
  _cogl_texture_driver_upload_subregion_to_gl (GL_TEXTURE_2D,
                                               tex_2d->gl_texture,
                                               FALSE,
                                               src_x, src_y,
                                               dst_x, dst_y,
                                               dst_width, dst_height,
                                               bmp,
                                               gl_format,
                                               gl_type);

  tex_2d->mipmaps_dirty = TRUE;

  return TRUE;
}

static gboolean
_cogl_texture_2d_get_data (CoglTexture     *tex,
                           CoglPixelFormat  format,
                           unsigned int     rowstride,
                           guint8          *data)
{
  CoglTexture2D   *tex_2d = COGL_TEXTURE_2D (tex);
  int              bpp;
  GLenum           gl_format;
  GLenum           gl_type;

  bpp = _cogl_get_format_bpp (format);

  _cogl_pixel_format_to_gl (format,
                            NULL, /* internal format */
                            &gl_format,
                            &gl_type);

  _cogl_texture_driver_prep_gl_for_pixels_download (rowstride, bpp);

  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   FALSE);
  return _cogl_texture_driver_gl_get_tex_image (GL_TEXTURE_2D,
                                                gl_format,
                                                gl_type,
                                                data);
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

static int
_cogl_texture_2d_get_width (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->width;
}

static int
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
    _cogl_texture_2d_pre_paint,
    _cogl_texture_2d_ensure_non_quad_rendering,
    _cogl_texture_2d_set_wrap_mode_parameters,
    _cogl_texture_2d_get_format,
    _cogl_texture_2d_get_gl_format,
    _cogl_texture_2d_get_width,
    _cogl_texture_2d_get_height,
    NULL /* is_foreign */
  };
