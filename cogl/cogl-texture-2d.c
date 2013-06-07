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

#include "cogl-private.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-2d-gl-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-error-private.h"
#ifdef COGL_HAS_EGL_SUPPORT
#include "cogl-winsys-egl-private.h"
#endif

#include <string.h>
#include <math.h>

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
#include "cogl-wayland-server.h"
#endif

static void _cogl_texture_2d_free (CoglTexture2D *tex_2d);

COGL_TEXTURE_DEFINE (Texture2D, texture_2d);

static const CoglTextureVtable cogl_texture_2d_vtable;

typedef struct _CoglTexture2DManualRepeatData
{
  CoglTexture2D *tex_2d;
  CoglMetaTextureCallback callback;
  void *user_data;
} CoglTexture2DManualRepeatData;

static void
_cogl_texture_2d_free (CoglTexture2D *tex_2d)
{
  CoglContext *ctx = COGL_TEXTURE (tex_2d)->context;

  ctx->driver_vtable->texture_2d_free (tex_2d);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_2d));
}

static CoglBool
_cogl_texture_2d_can_create (CoglContext *ctx,
                             unsigned int width,
                             unsigned int height,
                             CoglPixelFormat internal_format)
{
  /* If NPOT textures aren't supported then the size must be a power
     of two */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT_BASIC) &&
      (!_cogl_util_is_pot (width) ||
       !_cogl_util_is_pot (height)))
    return FALSE;

  return ctx->driver_vtable->texture_2d_can_create (ctx,
                                                    width,
                                                    height,
                                                    internal_format);
}

void
_cogl_texture_2d_set_auto_mipmap (CoglTexture *tex,
                                  CoglBool value)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  tex_2d->auto_mipmap = value;
}

CoglTexture2D *
_cogl_texture_2d_create_base (CoglContext *ctx,
                              int width,
                              int height,
                              CoglPixelFormat internal_format)
{
  CoglTexture2D *tex_2d = g_new (CoglTexture2D, 1);
  CoglTexture *tex = COGL_TEXTURE (tex_2d);

  _cogl_texture_init (tex, ctx, width, height, &cogl_texture_2d_vtable);

  tex_2d->mipmaps_dirty = TRUE;
  tex_2d->auto_mipmap = TRUE;

  tex_2d->is_foreign = FALSE;

  tex_2d->internal_format = internal_format;

  ctx->driver_vtable->texture_2d_init (tex_2d);

  return _cogl_texture_2d_object_new (tex_2d);
}

CoglTexture2D *
cogl_texture_2d_new_with_size (CoglContext *ctx,
                               int width,
                               int height,
                               CoglPixelFormat internal_format)
{
  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  return  _cogl_texture_2d_create_base (ctx,
                                        width, height,
                                        internal_format);
}

static CoglBool
_cogl_texture_2d_allocate (CoglTexture *tex,
                           CoglError **error)
{
  CoglContext *ctx = tex->context;
  return ctx->driver_vtable->texture_2d_allocate (tex, error);
}

CoglTexture2D *
_cogl_texture_2d_new_from_bitmap (CoglBitmap *bmp,
                                  CoglPixelFormat internal_format,
                                  CoglBool can_convert_in_place,
                                  CoglError **error)
{
  CoglContext *ctx;

  _COGL_RETURN_VAL_IF_FAIL (bmp != NULL, NULL);

  ctx = _cogl_bitmap_get_context (bmp);

  internal_format =
    _cogl_texture_determine_internal_format (cogl_bitmap_get_format (bmp),
                                             internal_format);

  if (!_cogl_texture_2d_can_create (ctx,
                                    cogl_bitmap_get_width (bmp),
                                    cogl_bitmap_get_height (bmp),
                                    internal_format))
    {
      _cogl_set_error (error, COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_SIZE,
                       "Failed to create texture 2d due to size/format"
                       " constraints");
      return NULL;
    }

  return ctx->driver_vtable->texture_2d_new_from_bitmap (bmp,
                                                         internal_format,
                                                         can_convert_in_place,
                                                         error);
}

CoglTexture2D *
cogl_texture_2d_new_from_bitmap (CoglBitmap *bmp,
                                 CoglPixelFormat internal_format,
                                 CoglError **error)
{
  return _cogl_texture_2d_new_from_bitmap (bmp, internal_format, FALSE, error);
}

CoglTexture2D *
cogl_texture_2d_new_from_data (CoglContext *ctx,
                               int width,
                               int height,
                               CoglPixelFormat format,
                               CoglPixelFormat internal_format,
                               int rowstride,
                               const uint8_t *data,
                               CoglError **error)
{
  CoglBitmap *bmp;
  CoglTexture2D *tex_2d;

  _COGL_RETURN_VAL_IF_FAIL (format != COGL_PIXEL_FORMAT_ANY, NULL);
  _COGL_RETURN_VAL_IF_FAIL (data != NULL, NULL);

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_pixel_format_get_bytes_per_pixel (format);

  /* Wrap the data into a bitmap */
  bmp = cogl_bitmap_new_for_data (ctx,
                                  width, height,
                                  format,
                                  rowstride,
                                  (uint8_t *) data);

  tex_2d = cogl_texture_2d_new_from_bitmap (bmp,
                                            internal_format,
                                            error);

  cogl_object_unref (bmp);

  return tex_2d;
}

#if defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base)
/* NB: The reason we require the width, height and format to be passed
 * even though they may seem redundant is because GLES 1/2 don't
 * provide a way to query these properties. */
CoglTexture2D *
_cogl_egl_texture_2d_new_from_image (CoglContext *ctx,
                                     int width,
                                     int height,
                                     CoglPixelFormat format,
                                     EGLImageKHR image,
                                     CoglError **error)
{
  _COGL_RETURN_VAL_IF_FAIL (_cogl_context_get_winsys (ctx)->constraints &
                            COGL_RENDERER_CONSTRAINT_USES_EGL,
                            NULL);

  _COGL_RETURN_VAL_IF_FAIL (ctx->private_feature_flags &
                        COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE,
                        NULL);

  if (ctx->driver_vtable->egl_texture_2d_new_from_image)
    return ctx->driver_vtable->egl_texture_2d_new_from_image (ctx,
                                                              width,
                                                              height,
                                                              format,
                                                              image,
                                                              error);
  else
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Creating 2D textures from EGL images is not "
                       "supported by the current driver");
      return NULL;
    }
}
#endif /* defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base) */

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
CoglTexture2D *
cogl_wayland_texture_2d_new_from_buffer (CoglContext *ctx,
                                         struct wl_resource *buffer_resource,
                                         CoglError **error)
{
  struct wl_shm_buffer *shm_buffer;

  shm_buffer = wl_shm_buffer_get (buffer_resource);

  if (shm_buffer)
    {
      int stride = wl_shm_buffer_get_stride (shm_buffer);
      CoglPixelFormat format;
      CoglPixelFormat internal_format = COGL_PIXEL_FORMAT_ANY;
      int width = wl_shm_buffer_get_width (shm_buffer);
      int height = wl_shm_buffer_get_height (shm_buffer);

      switch (wl_shm_buffer_get_format (shm_buffer))
        {
#if G_BYTE_ORDER == G_BIG_ENDIAN
          case WL_SHM_FORMAT_ARGB8888:
            format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
            break;
          case WL_SHM_FORMAT_XRGB32:
            format = COGL_PIXEL_FORMAT_ARGB_8888;
            internal_format = COGL_PIXEL_FORMAT_RGB_888;
            break;
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
          case WL_SHM_FORMAT_ARGB8888:
            format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
            break;
          case WL_SHM_FORMAT_XRGB8888:
            format = COGL_PIXEL_FORMAT_BGRA_8888;
            internal_format = COGL_PIXEL_FORMAT_BGR_888;
            break;
#endif
          default:
            g_warn_if_reached ();
            format = COGL_PIXEL_FORMAT_ARGB_8888;
        }

      return cogl_texture_2d_new_from_data (ctx,
                                            width, height,
                                            format,
                                            internal_format,
                                            stride,
                                            wl_shm_buffer_get_data (shm_buffer),
                                            error);
    }
  else
    {
      struct wl_buffer *buffer = (struct wl_buffer *) buffer_resource;
      int format, width, height;

      if (_cogl_egl_query_wayland_buffer (ctx,
                                          buffer,
                                          EGL_TEXTURE_FORMAT,
                                          &format) &&
          _cogl_egl_query_wayland_buffer (ctx,
                                          buffer,
                                          EGL_WIDTH,
                                          &width) &&
          _cogl_egl_query_wayland_buffer (ctx,
                                          buffer,
                                          EGL_HEIGHT,
                                          &height))
        {
          EGLImageKHR image;
          CoglTexture2D *tex = NULL;
          CoglPixelFormat internal_format;

          _COGL_RETURN_VAL_IF_FAIL (_cogl_context_get_winsys (ctx)->constraints &
                                    COGL_RENDERER_CONSTRAINT_USES_EGL,
                                    NULL);

          switch (format)
            {
            case EGL_TEXTURE_RGB:
              internal_format = COGL_PIXEL_FORMAT_RGB_888;
              break;
            case EGL_TEXTURE_RGBA:
              internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;
              break;
            default:
              _cogl_set_error (error,
                               COGL_SYSTEM_ERROR,
                               COGL_SYSTEM_ERROR_UNSUPPORTED,
                               "Can't create texture from unknown "
                               "wayland buffer format %d\n", format);
              return NULL;
            }

          image = _cogl_egl_create_image (ctx,
                                          EGL_WAYLAND_BUFFER_WL,
                                          buffer,
                                          NULL);
          tex = _cogl_egl_texture_2d_new_from_image (ctx,
                                                     width, height,
                                                     internal_format,
                                                     image,
                                                     error);
          _cogl_egl_destroy_image (ctx, image);
          return tex;
        }
    }

  _cogl_set_error (error,
                   COGL_SYSTEM_ERROR,
                   COGL_SYSTEM_ERROR_UNSUPPORTED,
                   "Can't create texture from unknown "
                   "wayland buffer type\n");
  return NULL;
}
#endif /* COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT */

void
_cogl_texture_2d_externally_modified (CoglTexture *texture)
{
  if (!cogl_is_texture_2d (texture))
    return;

  COGL_TEXTURE_2D (texture)->mipmaps_dirty = TRUE;
}

void
_cogl_texture_2d_copy_from_framebuffer (CoglTexture2D *tex_2d,
                                        int src_x,
                                        int src_y,
                                        int width,
                                        int height,
                                        CoglFramebuffer *src_fb,
                                        int dst_x,
                                        int dst_y,
                                        int level)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglContext *ctx = tex->context;

  /* Assert that the storage for this texture has been allocated */
  cogl_texture_allocate (tex, NULL); /* (abort on error) */

  ctx->driver_vtable->texture_2d_copy_from_framebuffer (tex_2d,
                                                        src_x,
                                                        src_y,
                                                        width,
                                                        height,
                                                        src_fb,
                                                        dst_x,
                                                        dst_y,
                                                        level);

  tex_2d->mipmaps_dirty = TRUE;
}

static int
_cogl_texture_2d_get_max_waste (CoglTexture *tex)
{
  return -1;
}

static CoglBool
_cogl_texture_2d_is_sliced (CoglTexture *tex)
{
  return FALSE;
}

static CoglBool
_cogl_texture_2d_can_hardware_repeat (CoglTexture *tex)
{
  CoglContext *ctx = tex->context;

  if (cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT) ||
      (_cogl_util_is_pot (tex->width) &&
       _cogl_util_is_pot (tex->height)))
    return TRUE;
  else
    return FALSE;
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

  int i;

  for (i = 0; i < 4; i++)
    if (coords[i] < 0.0f || coords[i] > 1.0f)
      {
        /* Repeat is needed */
        return (_cogl_texture_2d_can_hardware_repeat (tex) ?
                COGL_TRANSFORM_HARDWARE_REPEAT :
                COGL_TRANSFORM_SOFTWARE_REPEAT);
      }

  /* No repeat is needed */
  return COGL_TRANSFORM_NO_REPEAT;
}

static CoglBool
_cogl_texture_2d_get_gl_texture (CoglTexture *tex,
                                 GLuint *out_gl_handle,
                                 GLenum *out_gl_target)
{
  CoglContext *ctx = tex->context;
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  if (ctx->driver_vtable->texture_2d_get_gl_handle)
    {
      GLuint handle;

      if (out_gl_target)
        *out_gl_target = GL_TEXTURE_2D;

      handle = ctx->driver_vtable->texture_2d_get_gl_handle (tex_2d);

      if (out_gl_handle)
        *out_gl_handle = handle;

      return handle ? TRUE : FALSE;
    }
  else
    return FALSE;
}

static void
_cogl_texture_2d_pre_paint (CoglTexture *tex, CoglTexturePrePaintFlags flags)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  /* Only update if the mipmaps are dirty */
  if ((flags & COGL_TEXTURE_NEEDS_MIPMAP) &&
      tex_2d->auto_mipmap && tex_2d->mipmaps_dirty)
    {
      CoglContext *ctx = tex->context;

      ctx->driver_vtable->texture_2d_generate_mipmap (tex_2d);

      tex_2d->mipmaps_dirty = FALSE;
    }
}

static void
_cogl_texture_2d_ensure_non_quad_rendering (CoglTexture *tex)
{
  /* Nothing needs to be done */
}

static CoglBool
_cogl_texture_2d_set_region (CoglTexture *tex,
                             int src_x,
                             int src_y,
                             int dst_x,
                             int dst_y,
                             int width,
                             int height,
                             int level,
                             CoglBitmap *bmp,
                             CoglError **error)
{
  CoglContext *ctx = tex->context;
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  if (!ctx->driver_vtable->texture_2d_copy_from_bitmap (tex_2d,
                                                        src_x,
                                                        src_y,
                                                        width,
                                                        height,
                                                        bmp,
                                                        dst_x,
                                                        dst_y,
                                                        level,
                                                        error))
    {
      return FALSE;
    }

  tex_2d->mipmaps_dirty = TRUE;

  return TRUE;
}

static CoglBool
_cogl_texture_2d_get_data (CoglTexture *tex,
                           CoglPixelFormat format,
                           int rowstride,
                           uint8_t *data)
{
  CoglContext *ctx = tex->context;

  if (ctx->driver_vtable->texture_2d_get_data)
    {
      CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);
      ctx->driver_vtable->texture_2d_get_data (tex_2d, format, rowstride, data);
      return TRUE;
    }
  else
    return FALSE;
}

static CoglPixelFormat
_cogl_texture_2d_get_format (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->internal_format;
}

static GLenum
_cogl_texture_2d_get_gl_format (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->gl_internal_format;
}

static CoglBool
_cogl_texture_2d_is_foreign (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->is_foreign;
}

static CoglTextureType
_cogl_texture_2d_get_type (CoglTexture *tex)
{
  return COGL_TEXTURE_TYPE_2D;
}

static const CoglTextureVtable
cogl_texture_2d_vtable =
  {
    TRUE, /* primitive */
    _cogl_texture_2d_allocate,
    _cogl_texture_2d_set_region,
    _cogl_texture_2d_get_data,
    NULL, /* foreach_sub_texture_in_region */
    _cogl_texture_2d_get_max_waste,
    _cogl_texture_2d_is_sliced,
    _cogl_texture_2d_can_hardware_repeat,
    _cogl_texture_2d_transform_coords_to_gl,
    _cogl_texture_2d_transform_quad_coords_to_gl,
    _cogl_texture_2d_get_gl_texture,
    _cogl_texture_2d_gl_flush_legacy_texobj_filters,
    _cogl_texture_2d_pre_paint,
    _cogl_texture_2d_ensure_non_quad_rendering,
    _cogl_texture_2d_gl_flush_legacy_texobj_wrap_modes,
    _cogl_texture_2d_get_format,
    _cogl_texture_2d_get_gl_format,
    _cogl_texture_2d_get_type,
    _cogl_texture_2d_is_foreign,
    _cogl_texture_2d_set_auto_mipmap
  };
