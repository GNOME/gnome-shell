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
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-winsys-egl-private.h"

#include <string.h>
#include <math.h>

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
#include <wayland-server.h>
#endif

static void _cogl_texture_2d_free (CoglTexture2D *tex_2d);

COGL_TEXTURE_DEFINE (Texture2D, texture_2d);

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
                                           GLenum wrap_mode_p)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. Texture 2D doesn't make use of the r
     coordinate so we can ignore its wrap mode */
  if (tex_2d->wrap_mode_s != wrap_mode_s ||
      tex_2d->wrap_mode_t != wrap_mode_t)
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

      tex_2d->wrap_mode_s = wrap_mode_s;
      tex_2d->wrap_mode_t = wrap_mode_t;
    }
}

static void
_cogl_texture_2d_free (CoglTexture2D *tex_2d)
{
  if (!tex_2d->is_foreign)
    _cogl_delete_gl_texture (tex_2d->gl_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_2d));
}

static gboolean
_cogl_texture_2d_can_create (unsigned int width,
                             unsigned int height,
                             CoglPixelFormat internal_format)
{
  GLenum gl_intformat;
  GLenum gl_type;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* If NPOT textures aren't supported then the size must be a power
     of two */
  if (!cogl_features_available (COGL_FEATURE_TEXTURE_NPOT) &&
      (!_cogl_util_is_pot (width) ||
       !_cogl_util_is_pot (height)))
    return FALSE;

  ctx->texture_driver->pixel_format_to_gl (internal_format,
                                           &gl_intformat,
                                           NULL,
                                           &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!ctx->texture_driver->size_supported (GL_TEXTURE_2D,
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

  _cogl_texture_init (tex, &cogl_texture_2d_vtable);

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

  tex_2d->is_foreign = FALSE;

  tex_2d->format = internal_format;

  return tex_2d;
}

CoglTexture2D *
cogl_texture_2d_new_with_size (CoglContext *ctx,
                               int width,
                               int height,
                               CoglPixelFormat internal_format,
                               GError **error)
{
  CoglTexture2D         *tex_2d;
  GLenum                 gl_intformat;
  GLenum                 gl_format;
  GLenum                 gl_type;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  if (!_cogl_texture_2d_can_create (width, height, internal_format))
    {
      g_set_error (error, COGL_TEXTURE_ERROR,
                   COGL_TEXTURE_ERROR_SIZE,
                   "Failed to create texture 2d due to size/format"
                   " constraints");
      return NULL;
    }

  internal_format = ctx->texture_driver->pixel_format_to_gl (internal_format,
                                                             &gl_intformat,
                                                             &gl_format,
                                                             &gl_type);

  tex_2d = _cogl_texture_2d_create_base (width, height, COGL_TEXTURE_NONE,
                                         internal_format);

  ctx->texture_driver->gen (GL_TEXTURE_2D, 1, &tex_2d->gl_texture);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   tex_2d->is_foreign);
  GE( ctx, glTexImage2D (GL_TEXTURE_2D, 0, gl_intformat,
                         width, height, 0, gl_format, gl_type, NULL) );

  return _cogl_texture_2d_handle_new (tex_2d);
}

CoglHandle
_cogl_texture_2d_new_from_bitmap (CoglBitmap      *bmp,
                                  CoglTextureFlags flags,
                                  CoglPixelFormat  internal_format,
                                  GError         **error)
{
  CoglTexture2D *tex_2d;
  CoglBitmap    *dst_bmp;
  GLenum         gl_intformat;
  GLenum         gl_format;
  GLenum         gl_type;
  guint8        *data;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  g_return_val_if_fail (bmp != NULL, COGL_INVALID_HANDLE);

  internal_format =
    _cogl_texture_determine_internal_format (_cogl_bitmap_get_format (bmp),
                                             internal_format);

  if (!_cogl_texture_2d_can_create (_cogl_bitmap_get_width (bmp),
                                    _cogl_bitmap_get_height (bmp),
                                    internal_format))
    {
      g_set_error (error, COGL_TEXTURE_ERROR,
                   COGL_TEXTURE_ERROR_SIZE,
                   "Failed to create texture 2d due to size/format"
                   " constraints");
      return NULL;

    }

  if ((dst_bmp = _cogl_texture_prepare_for_upload (bmp,
                                                   internal_format,
                                                   &internal_format,
                                                   &gl_intformat,
                                                   &gl_format,
                                                   &gl_type)) == NULL)
    {
      g_set_error (error, COGL_TEXTURE_ERROR,
                   COGL_TEXTURE_ERROR_FORMAT,
                   "Failed to prepare texture upload due to format");
      return NULL;
    }

  tex_2d = _cogl_texture_2d_create_base (_cogl_bitmap_get_width (bmp),
                                         _cogl_bitmap_get_height (bmp),
                                         flags,
                                         internal_format);

  /* Keep a copy of the first pixel so that if glGenerateMipmap isn't
     supported we can fallback to using GL_GENERATE_MIPMAP */
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN) &&
      (data = _cogl_bitmap_map (dst_bmp,
                                COGL_BUFFER_ACCESS_READ, 0)))
    {
      tex_2d->first_pixel.gl_format = gl_format;
      tex_2d->first_pixel.gl_type = gl_type;
      memcpy (tex_2d->first_pixel.data, data,
              _cogl_get_format_bpp (_cogl_bitmap_get_format (dst_bmp)));

      _cogl_bitmap_unmap (dst_bmp);
    }

  ctx->texture_driver->gen (GL_TEXTURE_2D, 1, &tex_2d->gl_texture);
  ctx->texture_driver->upload_to_gl (GL_TEXTURE_2D,
                                     tex_2d->gl_texture,
                                     FALSE,
                                     dst_bmp,
                                     gl_intformat,
                                     gl_format,
                                     gl_type);

  tex_2d->gl_format = gl_intformat;

  cogl_object_unref (dst_bmp);

  return _cogl_texture_2d_handle_new (tex_2d);
}

CoglTexture2D *
cogl_texture_2d_new_from_data (CoglContext *ctx,
                               int width,
                               int height,
                               CoglPixelFormat format,
                               CoglPixelFormat internal_format,
                               int rowstride,
                               const guint8 *data,
                               GError **error)
{
  CoglBitmap *bmp;
  CoglHandle tex;

  g_return_val_if_fail (format != COGL_PIXEL_FORMAT_ANY, NULL);
  g_return_val_if_fail (data != NULL, NULL);

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_get_format_bpp (format);

  /* Wrap the data into a bitmap */
  bmp = _cogl_bitmap_new_from_data ((guint8 *)data,
                                    format,
                                    width,
                                    height,
                                    rowstride,
                                    NULL, NULL);

  tex =_cogl_texture_2d_new_from_bitmap (bmp, COGL_TEXTURE_NONE,
                                         internal_format,
                                         error);

  cogl_object_unref (bmp);

  return tex;
}

CoglTexture2D *
cogl_texture_2d_new_from_foreign (CoglContext *ctx,
                                  GLuint gl_handle,
                                  int width,
                                  int height,
                                  CoglPixelFormat format,
                                  GError **error)
{
  /* NOTE: width, height and internal format are not queriable
   * in GLES, hence such a function prototype.
   */

  GLenum         gl_error      = 0;
  GLint          gl_compressed = GL_FALSE;
  GLenum         gl_int_format = 0;
  CoglTexture2D *tex_2d;

  if (!ctx->texture_driver->allows_foreign_gl_target (GL_TEXTURE_2D))
    return COGL_INVALID_HANDLE;

  /* Make sure it is a valid GL texture object */
  if (!ctx->glIsTexture (gl_handle))
    return COGL_INVALID_HANDLE;

  /* Make sure binding succeeds */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D, gl_handle, TRUE);
  if (ctx->glGetError () != GL_NO_ERROR)
    return COGL_INVALID_HANDLE;

  /* Obtain texture parameters
     (only level 0 we are interested in) */

#if HAVE_COGL_GL
  if (ctx->driver == COGL_DRIVER_GL)
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

  /* Note: previously this code would query the texture object for
     whether it has GL_GENERATE_MIPMAP enabled to determine whether to
     auto-generate the mipmap. This doesn't make much sense any more
     since Cogl switch to using glGenerateMipmap. Ideally I think
     cogl_texture_new_from_foreign should take a flags parameter so
     that the application can decide whether it wants
     auto-mipmapping. To be compatible with existing code, Cogl now
     disables its own auto-mipmapping but leaves the value of
     GL_GENERATE_MIPMAP alone so that it would still work but without
     the dirtiness tracking that Cogl would do. */

  /* Create new texture */
  tex_2d = _cogl_texture_2d_create_base (width, height,
                                         COGL_TEXTURE_NO_AUTO_MIPMAP,
                                         format);

  /* Setup bitmap info */
  tex_2d->is_foreign = TRUE;
  tex_2d->mipmaps_dirty = TRUE;

  tex_2d->format = format;

  tex_2d->gl_texture = gl_handle;
  tex_2d->gl_format = gl_int_format;

  /* Unknown filter */
  tex_2d->min_filter = GL_FALSE;
  tex_2d->mag_filter = GL_FALSE;

  return _cogl_texture_2d_handle_new (tex_2d);
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
                                     GError **error)
{
  CoglTexture2D *tex_2d;
  GLenum gl_error;

  g_return_val_if_fail (_cogl_context_get_winsys (ctx) ==
                        _cogl_winsys_egl_get_vtable (),
                        NULL);

  g_return_val_if_fail (ctx->private_feature_flags &
                        COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE,
                        NULL);

  tex_2d = _cogl_texture_2d_create_base (width, height, COGL_TEXTURE_NONE,
                                         format);

  ctx->texture_driver->gen (GL_TEXTURE_2D, 1, &tex_2d->gl_texture);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   FALSE);

  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;
  ctx->glEGLImageTargetTexture2D (GL_TEXTURE_2D, image);
  if (ctx->glGetError () != GL_NO_ERROR)
    {
      g_set_error (error,
                   COGL_TEXTURE_ERROR,
                   COGL_TEXTURE_ERROR_BAD_PARAMETER,
                   "Could not create a CoglTexture2D from a given EGLImage");
      return NULL;
    }

  return _cogl_texture_2d_handle_new (tex_2d);
}
#endif /* defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base) */

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
static CoglPixelFormat
get_buffer_format (struct wl_buffer *buffer)
{
  struct wl_compositor *compositor = buffer->compositor;
  struct wl_visual *visual = buffer->visual;
#if G_BYTE_ORDER == G_BIG_ENDIAN
  if (visual == &compositor->premultiplied_argb_visual)
    return COGL_PIXEL_FORMAT_ARGB_8888_PRE;
  else if (visual == &compositor->argb_visual)
    return COGL_PIXEL_FORMAT_ARGB_8888;
  else if (visual == &compositor->rgb_visual)
    return COGL_PIXEL_FORMAT_RGB_888;
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
  if (visual == &compositor->premultiplied_argb_visual)
    return COGL_PIXEL_FORMAT_BGRA_8888_PRE;
  else if (visual == &compositor->argb_visual)
    return COGL_PIXEL_FORMAT_BGRA_8888;
  else if (visual == &compositor->rgb_visual)
    return COGL_PIXEL_FORMAT_BGR_888;
#endif
  else
    g_return_val_if_reached (COGL_PIXEL_FORMAT_ANY);
}

CoglTexture2D *
cogl_wayland_texture_2d_new_from_buffer (CoglContext *ctx,
                                         struct wl_buffer *buffer,
                                         GError **error)
{
  CoglPixelFormat format = get_buffer_format (buffer);

  if (wl_buffer_is_shm (buffer))
    {
      int stride = wl_shm_buffer_get_stride (buffer);
      return cogl_texture_2d_new_from_data (ctx,
                                            buffer->width,
                                            buffer->height,
                                            format,
                                            COGL_PIXEL_FORMAT_ANY,
                                            stride,
                                            wl_shm_buffer_get_data (buffer),
                                            error);
    }
  else
    {
      EGLImageKHR image;

      g_return_val_if_fail (_cogl_context_get_winsys (ctx) ==
                            _cogl_winsys_egl_get_vtable (),
                            NULL);
      image = _cogl_egl_create_image (ctx,
                                      EGL_WAYLAND_BUFFER_WL,
                                      buffer,
                                      NULL);
      return _cogl_egl_texture_2d_new_from_image (ctx,
                                                  buffer->width,
                                                  buffer->height,
                                                  format,
                                                  image,
                                                  error);
    }
}
#endif /* COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT */

void
_cogl_texture_2d_externally_modified (CoglHandle handle)
{
  if (!cogl_is_texture_2d (handle))
    return;

  COGL_TEXTURE_2D (handle)->mipmaps_dirty = TRUE;
}

void
_cogl_texture_2d_copy_from_framebuffer (CoglHandle handle,
                                        int dst_x,
                                        int dst_y,
                                        int src_x,
                                        int src_y,
                                        int width,
                                        int height)
{
  CoglTexture2D *tex_2d;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_texture_2d (handle));

  tex_2d = COGL_TEXTURE_2D (handle);

  /* Make sure the current framebuffers are bound. We explicitly avoid
     flushing the clip state so we can bind our own empty state */
  _cogl_framebuffer_flush_state (cogl_get_draw_framebuffer (),
                                 _cogl_get_read_framebuffer (),
                                 0);

  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   tex_2d->is_foreign);

  ctx->glCopyTexSubImage2D (GL_TEXTURE_2D,
                            0, /* level */
                            dst_x, dst_y,
                            src_x, src_y,
                            width, height);

  tex_2d->mipmaps_dirty = TRUE;
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

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (min_filter == tex_2d->min_filter
      && mag_filter == tex_2d->mag_filter)
    return;

  /* Store new values */
  tex_2d->min_filter = min_filter;
  tex_2d->mag_filter = mag_filter;

  /* Apply new filters to the texture */
  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   tex_2d->is_foreign);
  GE( ctx, glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter) );
  GE( ctx, glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter) );
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
                                       tex_2d->is_foreign);

      /* glGenerateMipmap is defined in the FBO extension. If it's not
         available we'll fallback to temporarily enabling
         GL_GENERATE_MIPMAP and reuploading the first pixel */
      if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
        ctx->texture_driver->gl_generate_mipmaps (GL_TEXTURE_2D);
#if defined(HAVE_COGL_GLES) || defined(HAVE_COGL_GL)
      else
        {
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
  guint8        *data;

  _COGL_GET_CONTEXT (ctx, FALSE);

  ctx->texture_driver->pixel_format_to_gl (_cogl_bitmap_get_format (bmp),
                                           NULL, /* internal format */
                                           &gl_format,
                                           &gl_type);

  /* If this touches the first pixel then we'll update our copy */
  if (dst_x == 0 && dst_y == 0 &&
      !cogl_features_available (COGL_FEATURE_OFFSCREEN) &&
      (data = _cogl_bitmap_map (bmp, COGL_BUFFER_ACCESS_READ, 0)))
    {
      CoglPixelFormat bpp =
        _cogl_get_format_bpp (_cogl_bitmap_get_format (bmp));
      tex_2d->first_pixel.gl_format = gl_format;
      tex_2d->first_pixel.gl_type = gl_type;
      memcpy (tex_2d->first_pixel.data,
              data + _cogl_bitmap_get_rowstride (bmp) * src_y + bpp * src_x,
              bpp);

      _cogl_bitmap_unmap (bmp);
    }

  /* Send data to GL */
  ctx->texture_driver->upload_subregion_to_gl (GL_TEXTURE_2D,
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

  _COGL_GET_CONTEXT (ctx, FALSE);

  bpp = _cogl_get_format_bpp (format);

  ctx->texture_driver->pixel_format_to_gl (format,
                                           NULL, /* internal format */
                                           &gl_format,
                                           &gl_type);

  ctx->texture_driver->prep_gl_for_pixels_download (rowstride, bpp);

  _cogl_bind_gl_texture_transient (GL_TEXTURE_2D,
                                   tex_2d->gl_texture,
                                   tex_2d->is_foreign);
  return ctx->texture_driver->gl_get_tex_image (GL_TEXTURE_2D,
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

static gboolean
_cogl_texture_2d_is_foreign (CoglTexture *tex)
{
  return COGL_TEXTURE_2D (tex)->is_foreign;
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
    _cogl_texture_2d_is_foreign
  };
