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

#include "cogl-private.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-texture-3d-private.h"
#include "cogl-texture-3d.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"

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

static void
_cogl_texture_3d_gl_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                    GLenum wrap_mode_s,
                                                    GLenum wrap_mode_t,
                                                    GLenum wrap_mode_p)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);
  CoglContext *ctx = tex->context;

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. */
  if (tex_3d->gl_legacy_texobj_wrap_mode_s != wrap_mode_s ||
      tex_3d->gl_legacy_texobj_wrap_mode_t != wrap_mode_t ||
      tex_3d->gl_legacy_texobj_wrap_mode_p != wrap_mode_p)
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

      tex_3d->gl_legacy_texobj_wrap_mode_s = wrap_mode_s;
      tex_3d->gl_legacy_texobj_wrap_mode_t = wrap_mode_t;
      tex_3d->gl_legacy_texobj_wrap_mode_p = wrap_mode_p;
    }
}

static void
_cogl_texture_3d_free (CoglTexture3D *tex_3d)
{
  _cogl_delete_gl_texture (tex_3d->gl_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_3d));
}

static void
_cogl_texture_3d_set_auto_mipmap (CoglTexture *tex,
                                  CoglBool value)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);

  tex_3d->auto_mipmap = value;
}

static CoglTexture3D *
_cogl_texture_3d_create_base (CoglContext *ctx,
                              int width,
                              int height,
                              int depth,
                              CoglPixelFormat internal_format)
{
  CoglTexture3D *tex_3d = g_new (CoglTexture3D, 1);
  CoglTexture *tex = COGL_TEXTURE (tex_3d);

  _cogl_texture_init (tex, ctx, &cogl_texture_3d_vtable);

  tex_3d->width = width;
  tex_3d->height = height;
  tex_3d->depth = depth;
  tex_3d->mipmaps_dirty = TRUE;
  tex_3d->auto_mipmap = TRUE;

  /* We default to GL_LINEAR for both filters */
  tex_3d->gl_legacy_texobj_min_filter = GL_LINEAR;
  tex_3d->gl_legacy_texobj_mag_filter = GL_LINEAR;

  /* Wrap mode not yet set */
  tex_3d->gl_legacy_texobj_wrap_mode_s = GL_FALSE;
  tex_3d->gl_legacy_texobj_wrap_mode_t = GL_FALSE;
  tex_3d->gl_legacy_texobj_wrap_mode_p = GL_FALSE;

  tex_3d->format = internal_format;

  return tex_3d;
}

static CoglBool
_cogl_texture_3d_can_create (CoglContext *ctx,
                             int width,
                             int height,
                             int depth,
                             CoglPixelFormat internal_format,
                             CoglError **error)
{
  GLenum gl_intformat;
  GLenum gl_type;

  /* This should only happen on GLES */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "3D textures are not supported by the GPU");
      return FALSE;
    }

  /* If NPOT textures aren't supported then the size must be a power
     of two */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT) &&
      (!_cogl_util_is_pot (width) ||
       !_cogl_util_is_pot (height) ||
       !_cogl_util_is_pot (depth)))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "A non-power-of-two size was requested but this is not "
                       "supported by the GPU");
      return FALSE;
    }

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          NULL,
                                          &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!ctx->texture_driver->size_supported_3d (ctx,
                                               GL_TEXTURE_3D,
                                               gl_intformat,
                                               gl_type,
                                               width,
                                               height,
                                               depth))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "The requested dimensions are not supported by the GPU");
      return FALSE;
    }

  return TRUE;
}

CoglTexture3D *
cogl_texture_3d_new_with_size (CoglContext *ctx,
                               int width,
                               int height,
                               int depth,
                               CoglPixelFormat internal_format,
                               CoglError **error)
{
  CoglTexture3D         *tex_3d;
  GLenum                 gl_intformat;
  GLenum                 gl_format;
  GLenum                 gl_type;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  if (!_cogl_texture_3d_can_create (ctx,
                                    width, height, depth,
                                    internal_format,
                                    error))
    return NULL;

  internal_format = ctx->driver_vtable->pixel_format_to_gl (ctx,
                                                            internal_format,
                                                            &gl_intformat,
                                                            &gl_format,
                                                            &gl_type);

  tex_3d = _cogl_texture_3d_create_base (ctx,
                                         width, height, depth,
                                         internal_format);

  ctx->texture_driver->gen (ctx, GL_TEXTURE_3D, 1, &tex_3d->gl_texture);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                   tex_3d->gl_texture,
                                   FALSE);
  GE( ctx, glTexImage3D (GL_TEXTURE_3D, 0, gl_intformat,
                         width, height, depth, 0, gl_format, gl_type, NULL) );

  return _cogl_texture_3d_object_new (tex_3d);
}

CoglTexture3D *
cogl_texture_3d_new_from_bitmap (CoglBitmap *bmp,
                                 unsigned int height,
                                 unsigned int depth,
                                 CoglPixelFormat internal_format,
                                 CoglError **error)
{
  CoglTexture3D *tex_3d;
  CoglBitmap *dst_bmp;
  CoglPixelFormat bmp_format;
  unsigned int bmp_width;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;
  uint8_t *data;
  CoglContext *ctx;

  ctx = _cogl_bitmap_get_context (bmp);

  bmp_width = cogl_bitmap_get_width (bmp);
  bmp_format = cogl_bitmap_get_format (bmp);

  internal_format = _cogl_texture_determine_internal_format (bmp_format,
                                                             internal_format);

  if (!_cogl_texture_3d_can_create (ctx,
                                    bmp_width, height, depth,
                                    internal_format,
                                    error))
    return NULL;

  dst_bmp = _cogl_texture_prepare_for_upload (bmp,
                                              internal_format,
                                              &internal_format,
                                              &gl_intformat,
                                              &gl_format,
                                              &gl_type);

  if (dst_bmp == NULL)
    {
      _cogl_set_error (error, COGL_BITMAP_ERROR, COGL_BITMAP_ERROR_FAILED,
                       "Bitmap conversion failed");
      return NULL;
    }

  tex_3d = _cogl_texture_3d_create_base (ctx,
                                         bmp_width, height, depth,
                                         internal_format);

  /* Keep a copy of the first pixel so that if glGenerateMipmap isn't
     supported we can fallback to using GL_GENERATE_MIPMAP */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN) &&
      (data = _cogl_bitmap_map (dst_bmp,
                                COGL_BUFFER_ACCESS_READ, 0)))
    {
      CoglPixelFormat format = cogl_bitmap_get_format (dst_bmp);
      tex_3d->first_pixel.gl_format = gl_format;
      tex_3d->first_pixel.gl_type = gl_type;
      memcpy (tex_3d->first_pixel.data, data,
              _cogl_pixel_format_get_bytes_per_pixel (format));

      _cogl_bitmap_unmap (dst_bmp);
    }

  ctx->texture_driver->gen (ctx, GL_TEXTURE_3D, 1, &tex_3d->gl_texture);

  ctx->texture_driver->upload_to_gl_3d (ctx,
                                        GL_TEXTURE_3D,
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

  return _cogl_texture_3d_object_new (tex_3d);
}

CoglTexture3D *
cogl_texture_3d_new_from_data (CoglContext *context,
                               int width,
                               int height,
                               int depth,
                               CoglPixelFormat format,
                               CoglPixelFormat internal_format,
                               int rowstride,
                               int image_stride,
                               const uint8_t *data,
                               CoglError **error)
{
  CoglBitmap *bitmap;
  CoglTexture3D *ret;

  /* These are considered a programmer errors so we won't set a
     CoglError. It would be nice if this was a _COGL_RETURN_IF_FAIL but the
     rest of Cogl isn't using that */
  if (format == COGL_PIXEL_FORMAT_ANY)
    return NULL;

  if (data == NULL)
    return NULL;

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_pixel_format_get_bytes_per_pixel (format);
  /* Image stride from height and rowstride if not given */
  if (image_stride == 0)
    image_stride = height * rowstride;

  if (image_stride < rowstride * height)
    return NULL;

  /* GL doesn't support uploading when the image_stride isn't a
     multiple of the rowstride. If this happens we'll just pack the
     image into a new bitmap. The documentation for this function
     recommends avoiding this situation. */
  if (image_stride % rowstride != 0)
    {
      uint8_t *bmp_data;
      int bmp_rowstride;
      int z, y;

      bitmap = _cogl_bitmap_new_with_malloc_buffer (context,
                                                    width,
                                                    depth * height,
                                                    format);

      bmp_data = _cogl_bitmap_map (bitmap,
                                   COGL_BUFFER_ACCESS_WRITE,
                                   COGL_BUFFER_MAP_HINT_DISCARD);

      if (bmp_data == NULL)
        {
          cogl_object_unref (bitmap);
          return NULL;
        }

      bmp_rowstride = cogl_bitmap_get_rowstride (bitmap);

      /* Copy all of the images in */
      for (z = 0; z < depth; z++)
        for (y = 0; y < height; y++)
          memcpy (bmp_data + (z * bmp_rowstride * height +
                              bmp_rowstride * y),
                  data + z * image_stride + rowstride * y,
                  bmp_rowstride);

      _cogl_bitmap_unmap (bitmap);
    }
  else
    bitmap = cogl_bitmap_new_for_data (context,
                                       width,
                                       image_stride / rowstride * depth,
                                       format,
                                       rowstride,
                                       (uint8_t *) data);

  ret = cogl_texture_3d_new_from_bitmap (bitmap,
                                         height,
                                         depth,
                                         internal_format,
                                         error);

  cogl_object_unref (bitmap);

  return ret;
}

static int
_cogl_texture_3d_get_max_waste (CoglTexture *tex)
{
  return -1;
}

static CoglBool
_cogl_texture_3d_is_sliced (CoglTexture *tex)
{
  return FALSE;
}

static CoglBool
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

  CoglBool need_repeat = FALSE;
  int i;

  for (i = 0; i < 4; i++)
    if (coords[i] < 0.0f || coords[i] > 1.0f)
      need_repeat = TRUE;

  return (need_repeat ? COGL_TRANSFORM_HARDWARE_REPEAT
          : COGL_TRANSFORM_NO_REPEAT);
}

static CoglBool
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
_cogl_texture_3d_gl_flush_legacy_texobj_filters (CoglTexture *tex,
                                                 GLenum min_filter,
                                                 GLenum mag_filter)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);
  CoglContext *ctx = tex->context;

  if (min_filter == tex_3d->gl_legacy_texobj_min_filter
      && mag_filter == tex_3d->gl_legacy_texobj_mag_filter)
    return;

  /* Store new values */
  tex_3d->gl_legacy_texobj_min_filter = min_filter;
  tex_3d->gl_legacy_texobj_mag_filter = mag_filter;

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
  CoglContext *ctx = tex->context;

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
      if (cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
        ctx->texture_driver->gl_generate_mipmaps (ctx, GL_TEXTURE_3D);
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

static CoglBool
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
_cogl_texture_3d_get_data (CoglTexture *tex,
                           CoglPixelFormat format,
                           int rowstride,
                           uint8_t *data)
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

static CoglTextureType
_cogl_texture_3d_get_type (CoglTexture *tex)
{
  return COGL_TEXTURE_TYPE_3D;
}

static const CoglTextureVtable
cogl_texture_3d_vtable =
  {
    TRUE, /* primitive */
    _cogl_texture_3d_set_region,
    _cogl_texture_3d_get_data,
    NULL, /* foreach_sub_texture_in_region */
    _cogl_texture_3d_get_max_waste,
    _cogl_texture_3d_is_sliced,
    _cogl_texture_3d_can_hardware_repeat,
    _cogl_texture_3d_transform_coords_to_gl,
    _cogl_texture_3d_transform_quad_coords_to_gl,
    _cogl_texture_3d_get_gl_texture,
    _cogl_texture_3d_gl_flush_legacy_texobj_filters,
    _cogl_texture_3d_pre_paint,
    _cogl_texture_3d_ensure_non_quad_rendering,
    _cogl_texture_3d_gl_flush_legacy_texobj_wrap_modes,
    _cogl_texture_3d_get_format,
    _cogl_texture_3d_get_gl_format,
    _cogl_texture_3d_get_width,
    _cogl_texture_3d_get_height,
    _cogl_texture_3d_get_type,
    NULL, /* is_foreign */
    _cogl_texture_3d_set_auto_mipmap
  };
