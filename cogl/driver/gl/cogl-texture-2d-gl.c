/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010,2011,2012 Intel Corporation.
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
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-private.h"
#include "cogl-texture-2d-gl.h"
#include "cogl-texture-2d-gl-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-gl-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"
#include "cogl-util-gl-private.h"

void
_cogl_texture_2d_gl_free (CoglTexture2D *tex_2d)
{
  if (!tex_2d->is_foreign && tex_2d->gl_texture)
    _cogl_delete_gl_texture (tex_2d->gl_texture);
}

CoglBool
_cogl_texture_2d_gl_can_create (CoglContext *ctx,
                                int width,
                                int height,
                                CoglPixelFormat internal_format)
{
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!ctx->texture_driver->size_supported (ctx,
                                            GL_TEXTURE_2D,
                                            gl_intformat,
                                            gl_format,
                                            gl_type,
                                            width,
                                            height))
    return FALSE;

  return TRUE;
}

void
_cogl_texture_2d_gl_init (CoglTexture2D *tex_2d)
{
  tex_2d->gl_texture = 0;

  /* We default to GL_LINEAR for both filters */
  tex_2d->gl_legacy_texobj_min_filter = GL_LINEAR;
  tex_2d->gl_legacy_texobj_mag_filter = GL_LINEAR;

  /* Wrap mode not yet set */
  tex_2d->gl_legacy_texobj_wrap_mode_s = GL_FALSE;
  tex_2d->gl_legacy_texobj_wrap_mode_t = GL_FALSE;
}

CoglBool
_cogl_texture_2d_gl_allocate (CoglTexture *tex,
                              CoglError **error)
{
  CoglContext *ctx = tex->context;
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;
  GLenum gl_error;
  GLenum gl_texture;

  if (!_cogl_texture_2d_gl_can_create (ctx,
                                       tex->width,
                                       tex->height,
                                       tex_2d->internal_format))
    {
      _cogl_set_error (error, COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_SIZE,
                       "Failed to create texture 2d due to size/format"
                       " constraints");
      return FALSE;
    }

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          tex_2d->internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  gl_texture =
    ctx->texture_driver->gen (ctx, GL_TEXTURE_2D, tex_2d->internal_format);

  tex_2d->gl_internal_format = gl_intformat;

  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   gl_texture,
                                   tex_2d->is_foreign);

  /* Clear any GL errors */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  ctx->glTexImage2D (GL_TEXTURE_2D, 0, gl_intformat,
                     tex->width, tex->height, 0, gl_format, gl_type, NULL);

  if (_cogl_gl_util_catch_out_of_memory (ctx, error))
    {
      GE( ctx, glDeleteTextures (1, &gl_texture) );
      return FALSE;
    }

  tex_2d->gl_texture = gl_texture;
  tex_2d->gl_internal_format = gl_intformat;

  return TRUE;
}

CoglTexture2D *
_cogl_texture_2d_gl_new_from_bitmap (CoglBitmap *bmp,
                                     CoglPixelFormat internal_format,
                                     CoglBool can_convert_in_place,
                                     CoglError **error)
{
  CoglContext *ctx = _cogl_bitmap_get_context (bmp);
  CoglTexture2D *tex_2d;
  CoglBitmap *upload_bmp;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  upload_bmp = _cogl_bitmap_convert_for_upload (bmp,
                                                internal_format,
                                                can_convert_in_place,
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

  tex_2d = _cogl_texture_2d_create_base (ctx,
                                         cogl_bitmap_get_width (bmp),
                                         cogl_bitmap_get_height (bmp),
                                         internal_format);

  /* Keep a copy of the first pixel so that if glGenerateMipmap isn't
     supported we can fallback to using GL_GENERATE_MIPMAP */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    {
      CoglError *ignore = NULL;
      uint8_t *data = _cogl_bitmap_map (upload_bmp,
                                        COGL_BUFFER_ACCESS_READ, 0,
                                        &ignore);
      CoglPixelFormat format = cogl_bitmap_get_format (upload_bmp);

      tex_2d->first_pixel.gl_format = gl_format;
      tex_2d->first_pixel.gl_type = gl_type;

      if (data)
        {
          memcpy (tex_2d->first_pixel.data, data,
                  _cogl_pixel_format_get_bytes_per_pixel (format));
          _cogl_bitmap_unmap (upload_bmp);
        }
      else
        {
          g_warning ("Failed to read first pixel of bitmap for "
                     "glGenerateMipmap fallback");
          cogl_error_free (ignore);
          memset (tex_2d->first_pixel.data, 0,
                  _cogl_pixel_format_get_bytes_per_pixel (format));
        }
    }

  tex_2d->gl_texture =
    ctx->texture_driver->gen (ctx, GL_TEXTURE_2D, internal_format);
  if (!ctx->texture_driver->upload_to_gl (ctx,
                                          GL_TEXTURE_2D,
                                          tex_2d->gl_texture,
                                          FALSE,
                                          upload_bmp,
                                          gl_intformat,
                                          gl_format,
                                          gl_type,
                                          error))
    {
      cogl_object_unref (upload_bmp);
      cogl_object_unref (tex_2d);
      return NULL;
    }

  tex_2d->gl_internal_format = gl_intformat;

  cogl_object_unref (upload_bmp);

  _cogl_texture_set_allocated (COGL_TEXTURE (tex_2d), TRUE);

  return tex_2d;
}

#if defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base)
CoglTexture2D *
_cogl_egl_texture_2d_gl_new_from_image (CoglContext *ctx,
                                        int width,
                                        int height,
                                        CoglPixelFormat format,
                                        EGLImageKHR image,
                                        CoglError **error)
{
  CoglTexture2D *tex_2d;
  GLenum gl_error;

  tex_2d = _cogl_texture_2d_create_base (ctx,
                                         width, height,
                                         format);

  tex_2d->gl_texture =
    ctx->texture_driver->gen (ctx, GL_TEXTURE_2D, format);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   FALSE);

  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;
  ctx->glEGLImageTargetTexture2D (GL_TEXTURE_2D, image);
  if (ctx->glGetError () != GL_NO_ERROR)
    {
      _cogl_set_error (error,
                       COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_BAD_PARAMETER,
                       "Could not create a CoglTexture2D from a given "
                       "EGLImage");
      cogl_object_unref (tex_2d);
      return NULL;
    }

  _cogl_texture_set_allocated (COGL_TEXTURE (tex_2d), TRUE);

  return tex_2d;
}
#endif

void
_cogl_texture_2d_gl_flush_legacy_texobj_filters (CoglTexture *tex,
                                                 GLenum min_filter,
                                                 GLenum mag_filter)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);
  CoglContext *ctx = tex->context;

  if (min_filter == tex_2d->gl_legacy_texobj_min_filter
      && mag_filter == tex_2d->gl_legacy_texobj_mag_filter)
    return;

  /* Store new values */
  tex_2d->gl_legacy_texobj_min_filter = min_filter;
  tex_2d->gl_legacy_texobj_mag_filter = mag_filter;

  /* Apply new filters to the texture */
  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   tex_2d->is_foreign);
  GE( ctx, glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter) );
  GE( ctx, glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter) );
}

void
_cogl_texture_2d_gl_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                    GLenum wrap_mode_s,
                                                    GLenum wrap_mode_t,
                                                    GLenum wrap_mode_p)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);
  CoglContext *ctx = tex->context;

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. Texture 2D doesn't make use of the r
     coordinate so we can ignore its wrap mode */
  if (tex_2d->gl_legacy_texobj_wrap_mode_s != wrap_mode_s ||
      tex_2d->gl_legacy_texobj_wrap_mode_t != wrap_mode_t)
    {
      _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                       tex_2d->gl_texture,
                                       tex_2d->is_foreign);
      GE( ctx, glTexParameteri (GL_TEXTURE_2D,
                                GL_TEXTURE_WRAP_S,
                                wrap_mode_s) );
      GE( ctx, glTexParameteri (GL_TEXTURE_2D,
                                GL_TEXTURE_WRAP_T,
                                wrap_mode_t) );

      tex_2d->gl_legacy_texobj_wrap_mode_s = wrap_mode_s;
      tex_2d->gl_legacy_texobj_wrap_mode_t = wrap_mode_t;
    }
}

CoglTexture2D *
cogl_texture_2d_new_from_foreign (CoglContext *ctx,
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
  CoglTexture2D *tex_2d;

  /* Assert it is a valid GL texture object */
  g_return_val_if_fail (ctx->glIsTexture (gl_handle), NULL);

  if (!ctx->texture_driver->allows_foreign_gl_target (ctx, GL_TEXTURE_2D))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Foreign GL_TEXTURE_2D textures are not "
                       "supported by your system");
      return NULL;
    }


  /* Make sure binding succeeds */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D, gl_handle, TRUE);
  if (ctx->glGetError () != GL_NO_ERROR)
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Failed to bind foreign GL_TEXTURE_2D texture");
      return NULL;
    }

  /* Obtain texture parameters
     (only level 0 we are interested in) */

#ifdef HAVE_COGL_GL
  if (_cogl_has_private_feature
      (ctx, COGL_PRIVATE_FEATURE_QUERY_TEXTURE_PARAMETERS))
    {
      GE( ctx, glGetTexLevelParameteriv (GL_TEXTURE_2D, 0,
                                         GL_TEXTURE_COMPRESSED,
                                         &gl_compressed) );

      {
        GLint val;

        GE( ctx, glGetTexLevelParameteriv (GL_TEXTURE_2D, 0,
                                           GL_TEXTURE_INTERNAL_FORMAT,
                                           &val) );

        gl_int_format = val;
      }

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

  /* Note: previously this code would query the texture object for
     whether it has GL_GENERATE_MIPMAP enabled to determine whether to
     auto-generate the mipmap. This doesn't make much sense any more
     since Cogl switch to using glGenerateMipmap. Ideally I think
     cogl_texture_2d_new_from_foreign should take a flags parameter so
     that the application can decide whether it wants
     auto-mipmapping. To be compatible with existing code, Cogl now
     disables its own auto-mipmapping but leaves the value of
     GL_GENERATE_MIPMAP alone so that it would still work but without
     the dirtiness tracking that Cogl would do. */

  /* Create new texture */
  tex_2d = _cogl_texture_2d_create_base (ctx,
                                         width, height,
                                         format);
  _cogl_texture_2d_set_auto_mipmap (COGL_TEXTURE (tex_2d), FALSE);

  /* Setup bitmap info */
  tex_2d->is_foreign = TRUE;
  tex_2d->mipmaps_dirty = TRUE;

  tex_2d->gl_texture = gl_handle;
  tex_2d->gl_internal_format = gl_int_format;

  /* Unknown filter */
  tex_2d->gl_legacy_texobj_min_filter = GL_FALSE;
  tex_2d->gl_legacy_texobj_mag_filter = GL_FALSE;

  _cogl_texture_set_allocated (COGL_TEXTURE (tex_2d), TRUE);

  return tex_2d;
}

void
_cogl_texture_2d_gl_copy_from_framebuffer (CoglTexture2D *tex_2d,
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

  /* Make sure the current framebuffers are bound, though we don't need to
   * flush the clip state here since we aren't going to draw to the
   * framebuffer. */
  _cogl_framebuffer_flush_state (ctx->current_draw_buffer,
                                 src_fb,
                                 COGL_FRAMEBUFFER_STATE_ALL &
                                 ~COGL_FRAMEBUFFER_STATE_CLIP);

  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   tex_2d->is_foreign);

  ctx->glCopyTexSubImage2D (GL_TEXTURE_2D,
                            0, /* level */
                            dst_x, dst_y,
                            src_x, src_y,
                            width, height);
}

unsigned int
_cogl_texture_2d_gl_get_gl_handle (CoglTexture2D *tex_2d)
{
    return tex_2d->gl_texture;
}

void
_cogl_texture_2d_gl_generate_mipmap (CoglTexture2D *tex_2d)
{
  CoglContext *ctx = COGL_TEXTURE (tex_2d)->context;

  /* glGenerateMipmap is defined in the FBO extension. If it's not
     available we'll fallback to temporarily enabling
     GL_GENERATE_MIPMAP and reuploading the first pixel */
  if (cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    _cogl_texture_gl_generate_mipmaps (COGL_TEXTURE (tex_2d));
#if defined(HAVE_COGL_GLES) || defined(HAVE_COGL_GL)
  else
    {
      _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                       tex_2d->gl_texture,
                                       tex_2d->is_foreign);

      GE( ctx, glTexParameteri (GL_TEXTURE_2D,
                                GL_GENERATE_MIPMAP,
                                GL_TRUE) );
      GE( ctx, glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 1, 1,
                                tex_2d->first_pixel.gl_format,
                                tex_2d->first_pixel.gl_type,
                                tex_2d->first_pixel.data) );
      GE( ctx, glTexParameteri (GL_TEXTURE_2D,
                                GL_GENERATE_MIPMAP,
                                GL_FALSE) );
    }
#endif
}

CoglBool
_cogl_texture_2d_gl_copy_from_bitmap (CoglTexture2D *tex_2d,
                                      int src_x,
                                      int src_y,
                                      int width,
                                      int height,
                                      CoglBitmap *bmp,
                                      int dst_x,
                                      int dst_y,
                                      int level,
                                      CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglContext *ctx = tex->context;
  CoglBitmap *upload_bmp;
  CoglPixelFormat upload_format;
  GLenum gl_format;
  GLenum gl_type;
  CoglBool status = TRUE;

  upload_bmp =
    _cogl_bitmap_convert_for_upload (bmp,
                                     cogl_texture_get_format (tex),
                                     FALSE, /* can't convert in place */
                                     error);
  if (upload_bmp == NULL)
    return FALSE;

  upload_format = cogl_bitmap_get_format (upload_bmp);

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          upload_format,
                                          NULL, /* internal format */
                                          &gl_format,
                                          &gl_type);

  /* If this touches the first pixel then we'll update our copy */
  if (dst_x == 0 && dst_y == 0 &&
      !cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    {
      CoglError *ignore = NULL;
      uint8_t *data =
        _cogl_bitmap_map (upload_bmp, COGL_BUFFER_ACCESS_READ, 0, &ignore);
      CoglPixelFormat bpp =
        _cogl_pixel_format_get_bytes_per_pixel (upload_format);

      tex_2d->first_pixel.gl_format = gl_format;
      tex_2d->first_pixel.gl_type = gl_type;

      if (data)
        {
          memcpy (tex_2d->first_pixel.data,
                  (data +
                   cogl_bitmap_get_rowstride (upload_bmp) * src_y +
                   bpp * src_x),
                  bpp);
          _cogl_bitmap_unmap (bmp);
        }
      else
        {
          g_warning ("Failed to read first bitmap pixel for "
                     "glGenerateMipmap fallback");
          cogl_error_free (ignore);
          memset (tex_2d->first_pixel.data, 0, bpp);
        }
    }

  status = ctx->texture_driver->upload_subregion_to_gl (ctx,
                                                        tex,
                                                        FALSE,
                                                        src_x, src_y,
                                                        dst_x, dst_y,
                                                        width, height,
                                                        level,
                                                        upload_bmp,
                                                        gl_format,
                                                        gl_type,
                                                        error);

  cogl_object_unref (upload_bmp);

  _cogl_texture_gl_maybe_update_max_level (tex, level);

  return status;
}

void
_cogl_texture_2d_gl_get_data (CoglTexture2D *tex_2d,
                              CoglPixelFormat format,
                              int rowstride,
                              uint8_t *data)
{
  CoglContext *ctx = COGL_TEXTURE (tex_2d)->context;
  int bpp;
  int width = COGL_TEXTURE (tex_2d)->width;
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
                                                    width,
                                                    bpp);

  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   tex_2d->is_foreign);

  ctx->texture_driver->gl_get_tex_image (ctx,
                                         GL_TEXTURE_2D,
                                         gl_format,
                                         gl_type,
                                         data);
}
