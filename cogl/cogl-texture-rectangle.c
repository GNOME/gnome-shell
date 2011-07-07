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
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"

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

COGL_TEXTURE_INTERNAL_DEFINE (TextureRectangle, texture_rectangle);

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
                                                  GLenum wrap_mode_p)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. Texture rectangle doesn't make use of
     the r coordinate so we can ignore its wrap mode */
  if (tex_rect->wrap_mode_s != wrap_mode_s ||
      tex_rect->wrap_mode_t != wrap_mode_t)
    {
      g_assert (can_use_wrap_mode (wrap_mode_s));
      g_assert (can_use_wrap_mode (wrap_mode_t));

      _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                       tex_rect->gl_texture,
                                       tex_rect->is_foreign);
      GE( ctx, glTexParameteri (GL_TEXTURE_RECTANGLE_ARB,
                                GL_TEXTURE_WRAP_S, wrap_mode_s) );
      GE( ctx, glTexParameteri (GL_TEXTURE_RECTANGLE_ARB,
                                GL_TEXTURE_WRAP_T, wrap_mode_t) );

      tex_rect->wrap_mode_s = wrap_mode_s;
      tex_rect->wrap_mode_t = wrap_mode_t;
    }
}

static void
_cogl_texture_rectangle_free (CoglTextureRectangle *tex_rect)
{
  if (!tex_rect->is_foreign)
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

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_TEXTURE_RECTANGLE))
    return FALSE;

  ctx->texture_driver->pixel_format_to_gl (internal_format,
                                           &gl_intformat,
                                           NULL,
                                           &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!ctx->texture_driver->size_supported (GL_TEXTURE_RECTANGLE_ARB,
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

  _cogl_texture_init (tex, &cogl_texture_rectangle_vtable);

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

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  if (!_cogl_texture_rectangle_can_create (width, height, internal_format))
    return COGL_INVALID_HANDLE;

  internal_format = ctx->texture_driver->pixel_format_to_gl (internal_format,
                                                             &gl_intformat,
                                                             &gl_format,
                                                             &gl_type);

  tex_rect = _cogl_texture_rectangle_create_base (width, height, flags,
                                                  internal_format);

  ctx->texture_driver->gen (GL_TEXTURE_RECTANGLE_ARB, 1, &tex_rect->gl_texture);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   tex_rect->gl_texture,
                                   tex_rect->is_foreign);
  GE( ctx, glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, gl_intformat,
                         width, height, 0, gl_format, gl_type, NULL) );

  return _cogl_texture_rectangle_handle_new (tex_rect);
}

CoglHandle
_cogl_texture_rectangle_new_from_bitmap (CoglBitmap      *bmp,
                                         CoglTextureFlags flags,
                                         CoglPixelFormat  internal_format)
{
  CoglTextureRectangle *tex_rect;
  CoglBitmap           *dst_bmp;
  GLenum                gl_intformat;
  GLenum                gl_format;
  GLenum                gl_type;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  g_return_val_if_fail (cogl_is_bitmap (bmp), COGL_INVALID_HANDLE);

  internal_format =
    _cogl_texture_determine_internal_format (_cogl_bitmap_get_format (bmp),
                                             internal_format);

  if (!_cogl_texture_rectangle_can_create (_cogl_bitmap_get_width (bmp),
                                           _cogl_bitmap_get_height (bmp),
                                           internal_format))
    return COGL_INVALID_HANDLE;

  dst_bmp = _cogl_texture_prepare_for_upload (bmp,
                                              internal_format,
                                              &internal_format,
                                              &gl_intformat,
                                              &gl_format,
                                              &gl_type);

  if (dst_bmp == NULL)
    return COGL_INVALID_HANDLE;

  tex_rect = _cogl_texture_rectangle_create_base (_cogl_bitmap_get_width (bmp),
                                                  _cogl_bitmap_get_height (bmp),
                                                  flags,
                                                  internal_format);

  ctx->texture_driver->gen (GL_TEXTURE_RECTANGLE_ARB, 1, &tex_rect->gl_texture);
  ctx->texture_driver->upload_to_gl (GL_TEXTURE_RECTANGLE_ARB,
                                     tex_rect->gl_texture,
                                     FALSE,
                                     dst_bmp,
                                     gl_intformat,
                                     gl_format,
                                     gl_type);

  tex_rect->gl_format = gl_intformat;

  cogl_object_unref (dst_bmp);

  return _cogl_texture_rectangle_handle_new (tex_rect);
}

CoglHandle
_cogl_texture_rectangle_new_from_foreign (GLuint gl_handle,
                                          GLuint width,
                                          GLuint height,
                                          CoglPixelFormat format)
{
  /* NOTE: width, height and internal format are not queriable
   * in GLES, hence such a function prototype.
   */

  GLenum                gl_error      = 0;
  GLint                 gl_compressed = GL_FALSE;
  GLenum                gl_int_format = 0;
  CoglTextureRectangle *tex_rect;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  if (!ctx->texture_driver->allows_foreign_gl_target (GL_TEXTURE_RECTANGLE_ARB))
    return COGL_INVALID_HANDLE;

  /* Make sure it is a valid GL texture object */
  if (!ctx->glIsTexture (gl_handle))
    return COGL_INVALID_HANDLE;

  /* Make sure binding succeeds */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB, gl_handle, TRUE);
  if (ctx->glGetError () != GL_NO_ERROR)
    return COGL_INVALID_HANDLE;

  /* Obtain texture parameters */

#if HAVE_COGL_GL
  if (ctx->driver == COGL_DRIVER_GL)
    {
      GLint val;

      GE( ctx, glGetTexLevelParameteriv (GL_TEXTURE_RECTANGLE_ARB, 0,
                                         GL_TEXTURE_COMPRESSED,
                                         &gl_compressed) );

      GE( ctx, glGetTexLevelParameteriv (GL_TEXTURE_RECTANGLE_ARB, 0,
                                         GL_TEXTURE_INTERNAL_FORMAT,
                                         &val) );

      gl_int_format = val;

      /* If we can query GL for the actual pixel format then we'll ignore
         the passed in format and use that. */
      if (!ctx->texture_driver->pixel_format_from_gl_internal (gl_int_format,
                                                               &format))
        return COGL_INVALID_HANDLE;
    }
  else
#endif
    {
      /* Otherwise we'll assume we can derive the GL format from the
         passed in format */
      ctx->texture_driver->pixel_format_to_gl (format,
                                               &gl_int_format,
                                               NULL,
                                               NULL);
    }

  /* Note: We always trust the given width and height without querying
   * the texture object because the user may be creating a Cogl
   * texture for a texture_from_pixmap object where glTexImage2D may
   * not have been called and the texture_from_pixmap spec doesn't
   * clarify that it is reliable to query back the size from OpenGL.
   */

  /* Validate width and height */
  if (width <= 0 || height <= 0)
    return COGL_INVALID_HANDLE;

  /* Compressed texture images not supported */
  if (gl_compressed == GL_TRUE)
    return COGL_INVALID_HANDLE;

  /* Create new texture */
  tex_rect = _cogl_texture_rectangle_create_base (width, height,
                                                  COGL_TEXTURE_NO_AUTO_MIPMAP,
                                                  format);

  /* Setup bitmap info */
  tex_rect->is_foreign = TRUE;

  tex_rect->format = format;

  tex_rect->gl_texture = gl_handle;
  tex_rect->gl_format = gl_int_format;

  /* Unknown filter */
  tex_rect->min_filter = GL_FALSE;
  tex_rect->mag_filter = GL_FALSE;

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

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (min_filter == tex_rect->min_filter
      && mag_filter == tex_rect->mag_filter)
    return;

  /* Rectangle textures don't support mipmapping */
  g_assert (min_filter == GL_LINEAR || min_filter == GL_NEAREST);

  /* Store new values */
  tex_rect->min_filter = min_filter;
  tex_rect->mag_filter = mag_filter;

  /* Apply new filters to the texture */
  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   tex_rect->gl_texture,
                                   tex_rect->is_foreign);
  GE( ctx, glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
                            mag_filter) );
  GE( ctx, glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
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
                                    CoglBitmap     *bmp)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  GLenum                gl_format;
  GLenum                gl_type;

  _COGL_GET_CONTEXT (ctx, FALSE);

  ctx->texture_driver->pixel_format_to_gl (_cogl_bitmap_get_format (bmp),
                                           NULL, /* internal format */
                                           &gl_format,
                                           &gl_type);

  /* Send data to GL */
  ctx->texture_driver->upload_subregion_to_gl (GL_TEXTURE_RECTANGLE_ARB,
                                               tex_rect->gl_texture,
                                               FALSE,
                                               src_x, src_y,
                                               dst_x, dst_y,
                                               dst_width, dst_height,
                                               bmp,
                                               gl_format,
                                               gl_type);

  return TRUE;
}

static gboolean
_cogl_texture_rectangle_get_data (CoglTexture     *tex,
                                  CoglPixelFormat  format,
                                  unsigned int     rowstride,
                                  guint8          *data)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  int                   bpp;
  GLenum                gl_format;
  GLenum                gl_type;

  _COGL_GET_CONTEXT (ctx, FALSE);

  bpp = _cogl_get_format_bpp (format);

  ctx->texture_driver->pixel_format_to_gl (format,
                                           NULL, /* internal format */
                                           &gl_format,
                                           &gl_type);

  ctx->texture_driver->prep_gl_for_pixels_download (rowstride, bpp);

  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   tex_rect->gl_texture,
                                   tex_rect->is_foreign);
  return ctx->texture_driver->gl_get_tex_image (GL_TEXTURE_RECTANGLE_ARB,
                                                gl_format,
                                                gl_type,
                                                data);
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

static gboolean
_cogl_texture_rectangle_is_foreign (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->is_foreign;
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
    _cogl_texture_rectangle_is_foreign
  };
