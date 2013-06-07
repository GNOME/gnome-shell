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

#include "cogl-private.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"
#include "cogl-util-gl-private.h"

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

static CoglBool
can_use_wrap_mode (GLenum wrap_mode)
{
  return (wrap_mode == GL_CLAMP ||
          wrap_mode == GL_CLAMP_TO_EDGE ||
          wrap_mode == GL_CLAMP_TO_BORDER);
}

static void
_cogl_texture_rectangle_gl_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                           GLenum wrap_mode_s,
                                                           GLenum wrap_mode_t,
                                                           GLenum wrap_mode_p)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  CoglContext *ctx = tex->context;

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. Texture rectangle doesn't make use of
     the r coordinate so we can ignore its wrap mode */
  if (tex_rect->gl_legacy_texobj_wrap_mode_s != wrap_mode_s ||
      tex_rect->gl_legacy_texobj_wrap_mode_t != wrap_mode_t)
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

      tex_rect->gl_legacy_texobj_wrap_mode_s = wrap_mode_s;
      tex_rect->gl_legacy_texobj_wrap_mode_t = wrap_mode_t;
    }
}

static void
_cogl_texture_rectangle_free (CoglTextureRectangle *tex_rect)
{
  if (!tex_rect->is_foreign && tex_rect->gl_texture)
    _cogl_delete_gl_texture (tex_rect->gl_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_rect));
}

static CoglBool
_cogl_texture_rectangle_can_create (CoglContext *ctx,
                                    unsigned int width,
                                    unsigned int height,
                                    CoglPixelFormat internal_format,
                                    CoglError **error)
{
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_RECTANGLE))
    {
      _cogl_set_error (error,
                       COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_TYPE,
                       "The CoglTextureRectangle feature isn't available");
      return FALSE;
    }

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!ctx->texture_driver->size_supported (ctx,
                                            GL_TEXTURE_RECTANGLE_ARB,
                                            gl_intformat,
                                            gl_format,
                                            gl_type,
                                            width,
                                            height))
    {
      _cogl_set_error (error,
                       COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_SIZE,
                       "The requested texture size + format is unsupported");
      return FALSE;
    }

  return TRUE;
}

static void
_cogl_texture_rectangle_set_auto_mipmap (CoglTexture *tex,
                                         CoglBool value)
{
  /* Rectangle textures currently never support mipmapping so there's
     no point in doing anything here */
}

static CoglTextureRectangle *
_cogl_texture_rectangle_create_base (CoglContext *ctx,
                                     int width,
                                     int height,
                                     CoglPixelFormat internal_format)
{
  CoglTextureRectangle *tex_rect = g_new (CoglTextureRectangle, 1);
  CoglTexture *tex = COGL_TEXTURE (tex_rect);

  _cogl_texture_init (tex, ctx, width, height, &cogl_texture_rectangle_vtable);

  tex_rect->gl_texture = 0;

  /* We default to GL_LINEAR for both filters */
  tex_rect->gl_legacy_texobj_min_filter = GL_LINEAR;
  tex_rect->gl_legacy_texobj_mag_filter = GL_LINEAR;

  /* Wrap mode not yet set */
  tex_rect->gl_legacy_texobj_wrap_mode_s = GL_FALSE;
  tex_rect->gl_legacy_texobj_wrap_mode_t = GL_FALSE;

  tex_rect->internal_format = internal_format;

  return _cogl_texture_rectangle_object_new (tex_rect);
}

CoglTextureRectangle *
cogl_texture_rectangle_new_with_size (CoglContext *ctx,
                                      int width,
                                      int height,
                                      CoglPixelFormat internal_format,
                                      CoglError **error)
{
  CoglTextureRectangle *tex_rect;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  tex_rect =_cogl_texture_rectangle_create_base (ctx,
                                                 width, height,
                                                 internal_format);

  /* XXX: This api has been changed for Cogl 2.0 on the master branch
   * to not take a CoglError to allow the storage to be allocated
   * lazily but since Mutter uses this api we are currently
   * maintaining the semantics of immediately allocating the storage
   */
  if (!cogl_texture_allocate (COGL_TEXTURE (tex_rect), error))
    {
      cogl_object_unref (tex_rect);
      return NULL;
    }
  return tex_rect;
}

static CoglBool
_cogl_texture_rectangle_allocate (CoglTexture *tex,
                                  CoglError **error)
{
  CoglContext *ctx = tex->context;
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;
  GLenum gl_error;
  GLenum gl_texture;

  if (!_cogl_texture_rectangle_can_create (ctx,
                                           tex->width,
                                           tex->height,
                                           tex_rect->internal_format,
                                           error))
    return FALSE;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          tex_rect->internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  gl_texture =
    ctx->texture_driver->gen (ctx,
                              GL_TEXTURE_RECTANGLE_ARB,
                              tex_rect->internal_format);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   gl_texture,
                                   tex_rect->is_foreign);

  /* Clear any GL errors */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  ctx->glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, gl_intformat,
                     tex->width, tex->height, 0, gl_format, gl_type, NULL);

  if (_cogl_gl_util_catch_out_of_memory (ctx, error))
    {
      GE( ctx, glDeleteTextures (1, &gl_texture) );
      return FALSE;
    }

  tex_rect->gl_texture = gl_texture;
  tex_rect->gl_format = gl_intformat;

  return TRUE;
}

CoglTextureRectangle *
cogl_texture_rectangle_new_from_bitmap (CoglBitmap *bmp,
                                        CoglPixelFormat internal_format,
                                        CoglError **error)
{
  CoglTextureRectangle *tex_rect;
  CoglBitmap *upload_bmp;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;
  CoglContext *ctx;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_bitmap (bmp), NULL);

  ctx = _cogl_bitmap_get_context (bmp);

  internal_format =
    _cogl_texture_determine_internal_format (cogl_bitmap_get_format (bmp),
                                             internal_format);

  if (!_cogl_texture_rectangle_can_create (ctx,
                                           cogl_bitmap_get_width (bmp),
                                           cogl_bitmap_get_height (bmp),
                                           internal_format,
                                           error))
    return NULL;

  upload_bmp =
    _cogl_bitmap_convert_for_upload (bmp,
                                     internal_format,
                                     FALSE, /* can't convert in place */
                                     error);
  if (upload_bmp == NULL)
    return NULL;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          cogl_bitmap_get_format (upload_bmp),
                                          NULL, /* internal format */
                                          &gl_format,
                                          &gl_type);
  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          NULL,
                                          NULL);

  tex_rect = _cogl_texture_rectangle_create_base (ctx,
                                                  cogl_bitmap_get_width (bmp),
                                                  cogl_bitmap_get_height (bmp),
                                                  internal_format);

  tex_rect->gl_texture =
    ctx->texture_driver->gen (ctx,
                              GL_TEXTURE_RECTANGLE_ARB,
                              internal_format);
  if (!ctx->texture_driver->upload_to_gl (ctx,
                                          GL_TEXTURE_RECTANGLE_ARB,
                                          tex_rect->gl_texture,
                                          FALSE,
                                          upload_bmp,
                                          gl_intformat,
                                          gl_format,
                                          gl_type,
                                          error))
    {
      cogl_object_unref (upload_bmp);
      cogl_object_unref (tex_rect);
      return NULL;
    }

  tex_rect->gl_format = gl_intformat;

  cogl_object_unref (upload_bmp);

  _cogl_texture_set_allocated (COGL_TEXTURE (tex_rect), TRUE);

  return tex_rect;
}

CoglTextureRectangle *
cogl_texture_rectangle_new_from_foreign (CoglContext *ctx,
                                         unsigned int gl_handle,
                                         int width,
                                         int height,
                                         CoglPixelFormat format,
                                         CoglError **error)
{
  /* NOTE: width, height and internal format are not queriable
   * in GLES, hence such a function prototype.
   */

  GLenum gl_error = 0;
  GLint gl_compressed = GL_FALSE;
  GLenum gl_int_format = 0;
  CoglTextureRectangle *tex_rect;

  /* Assert that it is a valid GL texture object */
  g_return_val_if_fail (ctx->glIsTexture (gl_handle), NULL);

  if (!ctx->texture_driver->allows_foreign_gl_target (ctx,
                                                      GL_TEXTURE_RECTANGLE_ARB))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Foreign GL_TEXTURE_RECTANGLE textures are not "
                       "supported by your system");
      return NULL;
    }

  /* Make sure binding succeeds */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB, gl_handle, TRUE);
  if (ctx->glGetError () != GL_NO_ERROR)
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Failed to bind foreign GL_TEXTURE_RECTANGLE texture");
      return NULL;
    }

  /* Obtain texture parameters */

#ifdef HAVE_COGL_GL
  if ((ctx->private_feature_flags &
       COGL_PRIVATE_FEATURE_QUERY_TEXTURE_PARAMETERS))
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
      if (!ctx->driver_vtable->pixel_format_from_gl_internal (ctx,
                                                              gl_int_format,
                                                              &format))
        {
          _cogl_set_error (error,
                           COGL_SYSTEM_ERROR,
                           COGL_SYSTEM_ERROR_UNSUPPORTED,
                           "Unsupported internal format for foreign texture");
          return NULL;
        }
    }
  else
#endif
    {
      /* Otherwise we'll assume we can derive the GL format from the
         passed in format */
      ctx->driver_vtable->pixel_format_to_gl (ctx,
                                              format,
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
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  /* Compressed texture images not supported */
  if (gl_compressed == GL_TRUE)
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Compressed foreign textures aren't currently supported");
      return NULL;
    }

  /* Create new texture */
  tex_rect = _cogl_texture_rectangle_create_base (ctx, width, height, format);

  /* Setup bitmap info */
  tex_rect->is_foreign = TRUE;

  tex_rect->internal_format = format;

  tex_rect->gl_texture = gl_handle;
  tex_rect->gl_format = gl_int_format;

  /* Unknown filter */
  tex_rect->gl_legacy_texobj_min_filter = GL_FALSE;
  tex_rect->gl_legacy_texobj_mag_filter = GL_FALSE;

  _cogl_texture_set_allocated (COGL_TEXTURE (tex_rect), TRUE);

  return tex_rect;
}

static int
_cogl_texture_rectangle_get_max_waste (CoglTexture *tex)
{
  return -1;
}

static CoglBool
_cogl_texture_rectangle_is_sliced (CoglTexture *tex)
{
  return FALSE;
}

static CoglBool
_cogl_texture_rectangle_can_hardware_repeat (CoglTexture *tex)
{
  return FALSE;
}

static void
_cogl_texture_rectangle_transform_coords_to_gl (CoglTexture *tex,
                                                float *s,
                                                float *t)
{
  *s *= tex->width;
  *t *= tex->height;
}

static CoglTransformResult
_cogl_texture_rectangle_transform_quad_coords_to_gl (CoglTexture *tex,
                                                     float *coords)
{
  CoglBool need_repeat = FALSE;
  int i;

  for (i = 0; i < 4; i++)
    {
      if (coords[i] < 0.0f || coords[i] > 1.0f)
        need_repeat = TRUE;
      coords[i] *= (i & 1) ? tex->height : tex->width;
    }

  return (need_repeat ? COGL_TRANSFORM_SOFTWARE_REPEAT
          : COGL_TRANSFORM_NO_REPEAT);
}

static CoglBool
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
_cogl_texture_rectangle_gl_flush_legacy_texobj_filters (CoglTexture *tex,
                                                        GLenum min_filter,
                                                        GLenum mag_filter)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  CoglContext *ctx = tex->context;

  if (min_filter == tex_rect->gl_legacy_texobj_min_filter
      && mag_filter == tex_rect->gl_legacy_texobj_mag_filter)
    return;

  /* Rectangle textures don't support mipmapping */
  g_assert (min_filter == GL_LINEAR || min_filter == GL_NEAREST);

  /* Store new values */
  tex_rect->gl_legacy_texobj_min_filter = min_filter;
  tex_rect->gl_legacy_texobj_mag_filter = mag_filter;

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
  /* Rectangle textures don't support mipmaps */
  g_assert ((flags & COGL_TEXTURE_NEEDS_MIPMAP) == 0);
}

static void
_cogl_texture_rectangle_ensure_non_quad_rendering (CoglTexture *tex)
{
  /* Nothing needs to be done */
}

static CoglBool
_cogl_texture_rectangle_set_region (CoglTexture *tex,
                                    int src_x,
                                    int src_y,
                                    int dst_x,
                                    int dst_y,
                                    int dst_width,
                                    int dst_height,
                                    int level,
                                    CoglBitmap *bmp,
                                    CoglError **error)
{
  CoglBitmap *upload_bmp;
  GLenum gl_format;
  GLenum gl_type;
  CoglContext *ctx = tex->context;
  CoglBool status;

  upload_bmp =
    _cogl_bitmap_convert_for_upload (bmp,
                                     cogl_texture_get_format (tex),
                                     FALSE, /* can't convert in place */
                                     error);
  if (upload_bmp == NULL)
    return FALSE;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          cogl_bitmap_get_format (upload_bmp),
                                          NULL, /* internal format */
                                          &gl_format,
                                          &gl_type);

  /* Send data to GL */
  status =
    ctx->texture_driver->upload_subregion_to_gl (ctx,
                                                 tex,
                                                 FALSE,
                                                 src_x, src_y,
                                                 dst_x, dst_y,
                                                 dst_width, dst_height,
                                                 level,
                                                 upload_bmp,
                                                 gl_format,
                                                 gl_type,
                                                 error);

  cogl_object_unref (upload_bmp);

  return status;
}

static CoglBool
_cogl_texture_rectangle_get_data (CoglTexture *tex,
                                  CoglPixelFormat format,
                                  int rowstride,
                                  uint8_t *data)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  CoglContext *ctx = tex->context;
  int bpp;
  GLenum gl_format;
  GLenum gl_type;

  bpp = _cogl_pixel_format_get_bytes_per_pixel (format);

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          format,
                                          NULL, /* internal format */
                                          &gl_format,
                                          &gl_type);

  ctx->texture_driver->prep_gl_for_pixels_download (ctx,
                                                    rowstride,
                                                    tex->width,
                                                    bpp);

  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   tex_rect->gl_texture,
                                   tex_rect->is_foreign);
  return ctx->texture_driver->gl_get_tex_image (ctx,
                                                GL_TEXTURE_RECTANGLE_ARB,
                                                gl_format,
                                                gl_type,
                                                data);
}

static CoglPixelFormat
_cogl_texture_rectangle_get_format (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->internal_format;
}

static GLenum
_cogl_texture_rectangle_get_gl_format (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->gl_format;
}

static CoglBool
_cogl_texture_rectangle_is_foreign (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->is_foreign;
}

static CoglTextureType
_cogl_texture_rectangle_get_type (CoglTexture *tex)
{
  return COGL_TEXTURE_TYPE_RECTANGLE;
}

static const CoglTextureVtable
cogl_texture_rectangle_vtable =
  {
    TRUE, /* primitive */
    _cogl_texture_rectangle_allocate,
    _cogl_texture_rectangle_set_region,
    _cogl_texture_rectangle_get_data,
    NULL, /* foreach_sub_texture_in_region */
    _cogl_texture_rectangle_get_max_waste,
    _cogl_texture_rectangle_is_sliced,
    _cogl_texture_rectangle_can_hardware_repeat,
    _cogl_texture_rectangle_transform_coords_to_gl,
    _cogl_texture_rectangle_transform_quad_coords_to_gl,
    _cogl_texture_rectangle_get_gl_texture,
    _cogl_texture_rectangle_gl_flush_legacy_texobj_filters,
    _cogl_texture_rectangle_pre_paint,
    _cogl_texture_rectangle_ensure_non_quad_rendering,
    _cogl_texture_rectangle_gl_flush_legacy_texobj_wrap_modes,
    _cogl_texture_rectangle_get_format,
    _cogl_texture_rectangle_get_gl_format,
    _cogl_texture_rectangle_get_type,
    _cogl_texture_rectangle_is_foreign,
    _cogl_texture_rectangle_set_auto_mipmap
  };
