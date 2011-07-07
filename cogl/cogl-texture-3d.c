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
#include "cogl-texture-3d-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"

#include <string.h>
#include <math.h>

/* These might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D                           0x806F
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R                       0x8072
#endif

static void _cogl_texture_3d_free (CoglTexture3D *tex_3d);

COGL_TEXTURE_DEFINE (Texture3D, texture_3d);

static const CoglTextureVtable cogl_texture_3d_vtable;

typedef struct _CoglTexture3DManualRepeatData
{
  CoglTexture3D *tex_3d;
  CoglTextureSliceCallback callback;
  void *user_data;
} CoglTexture3DManualRepeatData;

static void
_cogl_texture_3d_wrap_coords (float t_1, float t_2,
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
_cogl_texture_3d_manual_repeat_cb (const float *coords,
                                   void *user_data)
{
  CoglTexture3DManualRepeatData *data = user_data;
  float slice_coords[4];

  _cogl_texture_3d_wrap_coords (coords[0], coords[2],
                                slice_coords + 0, slice_coords + 2);
  _cogl_texture_3d_wrap_coords (coords[1], coords[3],
                                slice_coords + 1, slice_coords + 3);

  data->callback (COGL_TEXTURE (data->tex_3d),
                  slice_coords,
                  coords,
                  data->user_data);
}

static void
_cogl_texture_3d_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglTextureSliceCallback callback,
                                       void *user_data)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);
  CoglTexture3DManualRepeatData data;

  data.tex_3d = tex_3d;
  data.callback = callback;
  data.user_data = user_data;

  /* We need to implement manual repeating because if Cogl is calling
     this function then it will set the wrap mode to GL_CLAMP_TO_EDGE
     and hardware repeating can't be done */
  _cogl_texture_iterate_manual_repeats (_cogl_texture_3d_manual_repeat_cb,
                                        virtual_tx_1, virtual_ty_1,
                                        virtual_tx_2, virtual_ty_2,
                                        &data);
}

static void
_cogl_texture_3d_set_wrap_mode_parameters (CoglTexture *tex,
                                           GLenum wrap_mode_s,
                                           GLenum wrap_mode_t,
                                           GLenum wrap_mode_p)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. */
  if (tex_3d->wrap_mode_s != wrap_mode_s ||
      tex_3d->wrap_mode_t != wrap_mode_t ||
      tex_3d->wrap_mode_p != wrap_mode_p)
    {
      _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                       tex_3d->gl_texture,
                                       FALSE);
      GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                GL_TEXTURE_WRAP_S,
                                wrap_mode_s) );
      GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                GL_TEXTURE_WRAP_T,
                                wrap_mode_t) );
      GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                GL_TEXTURE_WRAP_R,
                                wrap_mode_p) );

      tex_3d->wrap_mode_s = wrap_mode_s;
      tex_3d->wrap_mode_t = wrap_mode_t;
      tex_3d->wrap_mode_p = wrap_mode_p;
    }
}

static void
_cogl_texture_3d_free (CoglTexture3D *tex_3d)
{
  _cogl_delete_gl_texture (tex_3d->gl_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_3d));
}

static CoglTexture3D *
_cogl_texture_3d_create_base (unsigned int     width,
                              unsigned int     height,
                              unsigned int     depth,
                              CoglTextureFlags flags,
                              CoglPixelFormat  internal_format)
{
  CoglTexture3D *tex_3d = g_new (CoglTexture3D, 1);
  CoglTexture *tex = COGL_TEXTURE (tex_3d);

  _cogl_texture_init (tex, &cogl_texture_3d_vtable);

  tex_3d->width = width;
  tex_3d->height = height;
  tex_3d->depth = depth;
  tex_3d->mipmaps_dirty = TRUE;
  tex_3d->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;

  /* We default to GL_LINEAR for both filters */
  tex_3d->min_filter = GL_LINEAR;
  tex_3d->mag_filter = GL_LINEAR;

  /* Wrap mode not yet set */
  tex_3d->wrap_mode_s = GL_FALSE;
  tex_3d->wrap_mode_t = GL_FALSE;
  tex_3d->wrap_mode_p = GL_FALSE;

  tex_3d->format = internal_format;

  return tex_3d;
}

static gboolean
_cogl_texture_3d_can_create (unsigned int     width,
                            unsigned int     height,
                            unsigned int     depth,
                            CoglTextureFlags flags,
                            CoglPixelFormat  internal_format,
                            GError         **error)
{
  GLenum gl_intformat;
  GLenum gl_type;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* This should only happen on GLES */
  if (!cogl_features_available (COGL_FEATURE_TEXTURE_3D))
    {
      g_set_error (error,
                   COGL_ERROR,
                   COGL_ERROR_UNSUPPORTED,
                   "3D textures are not supported by the GPU");
      return FALSE;
    }

  /* If NPOT textures aren't supported then the size must be a power
     of two */
  if (!cogl_features_available (COGL_FEATURE_TEXTURE_NPOT) &&
      (!_cogl_util_is_pot (width) ||
       !_cogl_util_is_pot (height) ||
       !_cogl_util_is_pot (depth)))
    {
      g_set_error (error,
                   COGL_ERROR,
                   COGL_ERROR_UNSUPPORTED,
                   "A non-power-of-two size was requested but this is not "
                   "supported by the GPU");
      return FALSE;
    }

  ctx->texture_driver->pixel_format_to_gl (internal_format,
                                           &gl_intformat,
                                           NULL,
                                           &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!ctx->texture_driver->size_supported_3d (GL_TEXTURE_3D,
                                               gl_intformat,
                                               gl_type,
                                               width,
                                               height,
                                               depth))
    {
      g_set_error (error,
                   COGL_ERROR,
                   COGL_ERROR_UNSUPPORTED,
                   "The requested dimensions are not supported by the GPU");
      return FALSE;
    }

  return TRUE;
}

CoglHandle
cogl_texture_3d_new_with_size (unsigned int     width,
                               unsigned int     height,
                               unsigned int     depth,
                               CoglTextureFlags flags,
                               CoglPixelFormat  internal_format,
                               GError         **error)
{
  CoglTexture3D         *tex_3d;
  GLenum                 gl_intformat;
  GLenum                 gl_format;
  GLenum                 gl_type;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  if (!_cogl_texture_3d_can_create (width, height, depth,
                                    flags, internal_format,
                                    error))
    return COGL_INVALID_HANDLE;

  internal_format = ctx->texture_driver->pixel_format_to_gl (internal_format,
                                                             &gl_intformat,
                                                             &gl_format,
                                                             &gl_type);

  tex_3d = _cogl_texture_3d_create_base (width, height, depth,
                                         flags, internal_format);

  ctx->texture_driver->gen (GL_TEXTURE_3D, 1, &tex_3d->gl_texture);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                   tex_3d->gl_texture,
                                   FALSE);
  GE( ctx, glTexImage3D (GL_TEXTURE_3D, 0, gl_intformat,
                         width, height, depth, 0, gl_format, gl_type, NULL) );

  return _cogl_texture_3d_handle_new (tex_3d);
}

CoglHandle
_cogl_texture_3d_new_from_bitmap (CoglBitmap      *bmp,
                                  unsigned int     height,
                                  unsigned int     depth,
                                  CoglTextureFlags flags,
                                  CoglPixelFormat  internal_format,
                                  GError         **error)
{
  CoglTexture3D   *tex_3d;
  CoglBitmap      *dst_bmp;
  CoglPixelFormat  bmp_format;
  unsigned int     bmp_width;
  GLenum           gl_intformat;
  GLenum           gl_format;
  GLenum           gl_type;
  guint8          *data;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  bmp_width = _cogl_bitmap_get_width (bmp);
  bmp_format = _cogl_bitmap_get_format (bmp);

  internal_format = _cogl_texture_determine_internal_format (bmp_format,
                                                             internal_format);

  if (!_cogl_texture_3d_can_create (bmp_width, height, depth,
                                    flags, internal_format,
                                    error))
    return COGL_INVALID_HANDLE;

  dst_bmp = _cogl_texture_prepare_for_upload (bmp,
                                              internal_format,
                                              &internal_format,
                                              &gl_intformat,
                                              &gl_format,
                                              &gl_type);

  if (dst_bmp == NULL)
    {
      g_set_error (error, COGL_BITMAP_ERROR, COGL_BITMAP_ERROR_FAILED,
                   "Bitmap conversion failed");
      return COGL_INVALID_HANDLE;
    }

  tex_3d = _cogl_texture_3d_create_base (bmp_width, height, depth,
                                         flags, internal_format);

  /* Keep a copy of the first pixel so that if glGenerateMipmap isn't
     supported we can fallback to using GL_GENERATE_MIPMAP */
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN) &&
      (data = _cogl_bitmap_map (dst_bmp,
                                COGL_BUFFER_ACCESS_READ, 0)))
    {
      tex_3d->first_pixel.gl_format = gl_format;
      tex_3d->first_pixel.gl_type = gl_type;
      memcpy (tex_3d->first_pixel.data, data,
              _cogl_get_format_bpp (_cogl_bitmap_get_format (dst_bmp)));

      _cogl_bitmap_unmap (dst_bmp);
    }

  ctx->texture_driver->gen (GL_TEXTURE_3D, 1, &tex_3d->gl_texture);

  ctx->texture_driver->upload_to_gl_3d (GL_TEXTURE_3D,
                                        tex_3d->gl_texture,
                                        FALSE, /* is_foreign */
                                        height,
                                        depth,
                                        dst_bmp,
                                        gl_intformat,
                                        gl_format,
                                        gl_type);

  tex_3d->gl_format = gl_intformat;

  cogl_object_unref (dst_bmp);

  return _cogl_texture_3d_handle_new (tex_3d);
}

CoglHandle
cogl_texture_3d_new_from_data (unsigned int      width,
                               unsigned int      height,
                               unsigned int      depth,
                               CoglTextureFlags  flags,
                               CoglPixelFormat   format,
                               CoglPixelFormat   internal_format,
                               unsigned int      rowstride,
                               unsigned int      image_stride,
                               const guint8     *data,
                               GError          **error)
{
  CoglBitmap *bitmap;
  CoglHandle ret;

  /* These are considered a programmer errors so we won't set a
     GError. It would be nice if this was a g_return_if_fail but the
     rest of Cogl isn't using that */
  if (format == COGL_PIXEL_FORMAT_ANY)
    return COGL_INVALID_HANDLE;

  if (data == NULL)
    return COGL_INVALID_HANDLE;

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_get_format_bpp (format);
  /* Image stride from height and rowstride if not given */
  if (image_stride == 0)
    image_stride = height * rowstride;

  if (image_stride < rowstride * height)
    return COGL_INVALID_HANDLE;

  /* GL doesn't support uploading when the image_stride isn't a
     multiple of the rowstride. If this happens we'll just pack the
     image into a new bitmap. The documentation for this function
     recommends avoiding this situation. */
  if (image_stride % rowstride != 0)
    {
      int z, y;
      int bmp_rowstride = _cogl_get_format_bpp (format) * width;
      guint8 *bmp_data = g_malloc (bmp_rowstride * height * depth);

      bitmap = _cogl_bitmap_new_from_data (bmp_data,
                                           format,
                                           width,
                                           depth * height,
                                           bmp_rowstride,
                                           (CoglBitmapDestroyNotify) g_free,
                                           NULL /* destroy_fn_data */);

      /* Copy all of the images in */
      for (z = 0; z < depth; z++)
        for (y = 0; y < height; y++)
          memcpy (bmp_data + (z * bmp_rowstride * height +
                              bmp_rowstride * y),
                  data + z * image_stride + rowstride * y,
                  bmp_rowstride);
    }
  else
    bitmap = _cogl_bitmap_new_from_data ((guint8 *) data,
                                         format,
                                         width,
                                         image_stride / rowstride * depth,
                                         rowstride,
                                         NULL, /* destroy_fn */
                                         NULL /* destroy_fn_data */);

  ret = _cogl_texture_3d_new_from_bitmap (bitmap,
                                          height,
                                          depth,
                                          flags,
                                          internal_format,
                                          error);

  cogl_object_unref (bitmap);

  return ret;
}

GQuark
cogl_texture_3d_error_quark (void)
{
  return g_quark_from_static_string ("cogl-texture-3d-error-quark");
}

static int
_cogl_texture_3d_get_max_waste (CoglTexture *tex)
{
  return -1;
}

static gboolean
_cogl_texture_3d_is_sliced (CoglTexture *tex)
{
  return FALSE;
}

static gboolean
_cogl_texture_3d_can_hardware_repeat (CoglTexture *tex)
{
  return TRUE;
}

static void
_cogl_texture_3d_transform_coords_to_gl (CoglTexture *tex,
                                         float *s,
                                         float *t)
{
  /* The texture coordinates map directly so we don't need to do
     anything */
}

static CoglTransformResult
_cogl_texture_3d_transform_quad_coords_to_gl (CoglTexture *tex,
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
_cogl_texture_3d_get_gl_texture (CoglTexture *tex,
                                 GLuint *out_gl_handle,
                                 GLenum *out_gl_target)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);

  if (out_gl_handle)
    *out_gl_handle = tex_3d->gl_texture;

  if (out_gl_target)
    *out_gl_target = GL_TEXTURE_3D;

  return TRUE;
}

static void
_cogl_texture_3d_set_filters (CoglTexture *tex,
                              GLenum       min_filter,
                              GLenum       mag_filter)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (min_filter == tex_3d->min_filter
      && mag_filter == tex_3d->mag_filter)
    return;

  /* Store new values */
  tex_3d->min_filter = min_filter;
  tex_3d->mag_filter = mag_filter;

  /* Apply new filters to the texture */
  _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                   tex_3d->gl_texture,
                                   FALSE);
  GE( ctx, glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, mag_filter) );
  GE( ctx, glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, min_filter) );
}

static void
_cogl_texture_3d_pre_paint (CoglTexture *tex, CoglTexturePrePaintFlags flags)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Only update if the mipmaps are dirty */
  if ((flags & COGL_TEXTURE_NEEDS_MIPMAP) &&
      tex_3d->auto_mipmap && tex_3d->mipmaps_dirty)
    {
      _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                       tex_3d->gl_texture,
                                       FALSE);
      /* glGenerateMipmap is defined in the FBO extension. If it's not
         available we'll fallback to temporarily enabling
         GL_GENERATE_MIPMAP and reuploading the first pixel */
      if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
        ctx->texture_driver->gl_generate_mipmaps (GL_TEXTURE_3D);
#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)
      else if (ctx->driver != COGL_DRIVER_GLES2)
        {
          GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                    GL_GENERATE_MIPMAP,
                                    GL_TRUE) );
          GE( ctx, glTexSubImage3D (GL_TEXTURE_3D,
                                    0, /* level */
                                    0, /* xoffset */
                                    0, /* yoffset */
                                    0, /* zoffset */
                                    1, /* width */
                                    1, /* height */
                                    1, /* depth */
                                    tex_3d->first_pixel.gl_format,
                                    tex_3d->first_pixel.gl_type,
                                    tex_3d->first_pixel.data) );
          GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                    GL_GENERATE_MIPMAP,
                                    GL_FALSE) );
        }
#endif

      tex_3d->mipmaps_dirty = FALSE;
    }
}

static void
_cogl_texture_3d_ensure_non_quad_rendering (CoglTexture *tex)
{
  /* Nothing needs to be done */
}

static gboolean
_cogl_texture_3d_set_region (CoglTexture    *tex,
                             int             src_x,
                             int             src_y,
                             int             dst_x,
                             int             dst_y,
                             unsigned int    dst_width,
                             unsigned int    dst_height,
                             CoglBitmap     *bmp)
{
  /* This function doesn't really make sense for 3D textures because
     it can't specify which image to upload to */
  return FALSE;
}

static int
_cogl_texture_3d_get_data (CoglTexture     *tex,
                           CoglPixelFormat  format,
                           unsigned int     rowstride,
                           guint8          *data)
{
  /* FIXME: we could probably implement this by assuming the data is
     big enough to hold all of the images and that there is no stride
     between the images. However it would be better to have an API
     that can provide an image stride and this function probably isn't
     particularly useful anyway so for now it just reports failure */
  return 0;
}

static CoglPixelFormat
_cogl_texture_3d_get_format (CoglTexture *tex)
{
  return COGL_TEXTURE_3D (tex)->format;
}

static GLenum
_cogl_texture_3d_get_gl_format (CoglTexture *tex)
{
  return COGL_TEXTURE_3D (tex)->gl_format;
}

static int
_cogl_texture_3d_get_width (CoglTexture *tex)
{
  return COGL_TEXTURE_3D (tex)->width;
}

static int
_cogl_texture_3d_get_height (CoglTexture *tex)
{
  return COGL_TEXTURE_3D (tex)->height;
}

static const CoglTextureVtable
cogl_texture_3d_vtable =
  {
    _cogl_texture_3d_set_region,
    _cogl_texture_3d_get_data,
    _cogl_texture_3d_foreach_sub_texture_in_region,
    _cogl_texture_3d_get_max_waste,
    _cogl_texture_3d_is_sliced,
    _cogl_texture_3d_can_hardware_repeat,
    _cogl_texture_3d_transform_coords_to_gl,
    _cogl_texture_3d_transform_quad_coords_to_gl,
    _cogl_texture_3d_get_gl_texture,
    _cogl_texture_3d_set_filters,
    _cogl_texture_3d_pre_paint,
    _cogl_texture_3d_ensure_non_quad_rendering,
    _cogl_texture_3d_set_wrap_mode_parameters,
    _cogl_texture_3d_get_format,
    _cogl_texture_3d_get_gl_format,
    _cogl_texture_3d_get_width,
    _cogl_texture_3d_get_height,
    NULL /* is_foreign */
  };
