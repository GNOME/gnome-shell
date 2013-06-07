/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
 * Copyright (C) 2010 Red Hat, Inc.
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
 *  Matthew Allum  <mallum@openedhand.com>
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-bitmap.h"
#include "cogl-bitmap-private.h"
#include "cogl-buffer-private.h"
#include "cogl-pixel-buffer-private.h"
#include "cogl-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-driver.h"
#include "cogl-texture-2d-sliced-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-2d-gl.h"
#include "cogl-texture-3d-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-sub-texture-private.h"
#include "cogl-atlas-texture-private.h"
#include "cogl-pipeline.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-object-private.h"
#include "cogl-primitives.h"
#include "cogl-framebuffer-private.h"
#include "cogl1-context.h"
#include "cogl-sub-texture.h"
#include "cogl-primitive-texture.h"
#include "cogl-error-private.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

/* This isn't defined in the GLES headers */
#ifndef GL_RED
#define GL_RED 0x1903
#endif

uint32_t
cogl_texture_error_quark (void)
{
  return g_quark_from_static_string ("cogl-texture-error-quark");
}

/* XXX:
 * The CoglObject macros don't support any form of inheritance, so for
 * now we implement the CoglObject support for the CoglTexture
 * abstract class manually.
 */

static GSList *_cogl_texture_types;

void
_cogl_texture_register_texture_type (const CoglObjectClass *klass)
{
  _cogl_texture_types = g_slist_prepend (_cogl_texture_types, (void *) klass);
}

CoglBool
cogl_is_texture (void *object)
{
  CoglObject *obj = (CoglObject *)object;
  GSList *l;

  if (object == NULL)
    return FALSE;

  for (l = _cogl_texture_types; l; l = l->next)
    if (l->data == obj->klass)
      return TRUE;

  return FALSE;
}

void *
cogl_texture_ref (void *object)
{
  if (!cogl_is_texture (object))
    return NULL;

  _COGL_OBJECT_DEBUG_REF (CoglTexture, object);

  cogl_object_ref (object);

  return object;
}

void
cogl_texture_unref (void *object)
{
  if (!cogl_is_texture (object))
    {
      g_warning (G_STRINGIFY (cogl_texture_unref)
                 ": Ignoring unref of CoglObject "
                 "due to type mismatch");
      return;
    }

  _COGL_OBJECT_DEBUG_UNREF (CoglTexture, object);

  cogl_object_unref (object);
}

void
_cogl_texture_init (CoglTexture *texture,
                    CoglContext *context,
                    int width,
                    int height,
                    const CoglTextureVtable *vtable)
{
  texture->context = context;
  texture->max_level = 0;
  texture->width = width;
  texture->height = height;
  texture->allocated = FALSE;
  texture->vtable = vtable;
  texture->framebuffers = NULL;
}

void
_cogl_texture_free (CoglTexture *texture)
{
  g_free (texture);
}

CoglBool
_cogl_texture_needs_premult_conversion (CoglPixelFormat src_format,
                                        CoglPixelFormat dst_format)
{
  return ((src_format & dst_format & COGL_A_BIT) &&
          src_format != COGL_PIXEL_FORMAT_A_8 &&
          dst_format != COGL_PIXEL_FORMAT_A_8 &&
          (src_format & COGL_PREMULT_BIT) !=
          (dst_format & COGL_PREMULT_BIT));
}

CoglPixelFormat
_cogl_texture_determine_internal_format (CoglPixelFormat src_format,
                                         CoglPixelFormat dst_format)
{
  /* If the application hasn't specified a specific format then we'll
   * pick the most appropriate. By default Cogl will use a
   * premultiplied internal format. Later we will add control over
   * this. */
  if (dst_format == COGL_PIXEL_FORMAT_ANY)
    {
      if (COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT (src_format))
        return src_format | COGL_PREMULT_BIT;
      else
        return src_format;
    }
  else
    /* XXX: It might be nice to make this match the component ordering
       of the source format when the formats are otherwise the same
       because on GL there is no way to specify the ordering of the
       internal format. However when using GLES with the
       GL_EXT_texture_format_BGRA8888 the order of the internal format
       becomes important because it must exactly match the format of
       the uploaded data. That means that if someone creates a texture
       with some RGBA data and then later tries to upload BGRA data we
       do actually have to swizzle the components */
    return dst_format;
}

CoglBool
_cogl_texture_is_foreign (CoglTexture *texture)
{
  if (texture->vtable->is_foreign)
    return texture->vtable->is_foreign (texture);
  else
    return FALSE;
}

CoglTexture *
cogl_texture_new_from_sub_texture (CoglTexture *full_texture,
                                   int sub_x,
                                   int sub_y,
                                   int sub_width,
                                   int sub_height)
{
  _COGL_GET_CONTEXT (ctx, NULL);
  return COGL_TEXTURE (cogl_sub_texture_new (ctx,
                                             full_texture, sub_x, sub_y,
                                             sub_width, sub_height));
}

unsigned int
cogl_texture_get_width (CoglTexture *texture)
{
  return texture->width;
}

unsigned int
cogl_texture_get_height (CoglTexture *texture)
{
  return texture->height;
}

CoglPixelFormat
cogl_texture_get_format (CoglTexture *texture)
{
  return texture->vtable->get_format (texture);
}

unsigned int
cogl_texture_get_rowstride (CoglTexture *texture)
{
  CoglPixelFormat format = cogl_texture_get_format (texture);
  /* FIXME: This function should go away. It previously just returned
     the rowstride that was used to upload the data as far as I can
     tell. This is not helpful */

  /* Just guess at a suitable rowstride */
  return (_cogl_pixel_format_get_bytes_per_pixel (format)
          * cogl_texture_get_width (texture));
}

int
cogl_texture_get_max_waste (CoglTexture *texture)
{
  return texture->vtable->get_max_waste (texture);
}

int
_cogl_texture_get_n_levels (CoglTexture *texture)
{
  int width = cogl_texture_get_width (texture);
  int height = cogl_texture_get_height (texture);
  int max_dimension = MAX (width, height);

  if (cogl_is_texture_3d (texture))
    {
      CoglTexture3D *tex_3d = COGL_TEXTURE_3D (texture);
      max_dimension = MAX (max_dimension, tex_3d->depth);
    }

  return _cogl_util_fls (max_dimension);
}

void
_cogl_texture_get_level_size (CoglTexture *texture,
                              int level,
                              int *width,
                              int *height,
                              int *depth)
{
  int current_width = cogl_texture_get_width (texture);
  int current_height = cogl_texture_get_height (texture);
  int current_depth;
  int i;

  if (cogl_is_texture_3d (texture))
    {
      CoglTexture3D *tex_3d = COGL_TEXTURE_3D (texture);
      current_depth = tex_3d->depth;
    }
  else
    current_depth = 0;

  /* NB: The OpenGL spec (like D3D) uses a floor() convention to
   * round down the size of a mipmap level when dividing the size
   * of the previous level results in a fraction...
   */
  for (i = 0; i < level; i++)
    {
      current_width = MAX (1, current_width >> 1);
      current_height = MAX (1, current_height >> 1);
      current_depth = MAX (1, current_depth >> 1);
    }

  if (width)
    *width = current_width;
  if (height)
    *height = current_height;
  if (depth)
    *depth = current_depth;
}

CoglBool
cogl_texture_is_sliced (CoglTexture *texture)
{
  return texture->vtable->is_sliced (texture);
}

/* If this returns FALSE, that implies _foreach_sub_texture_in_region
 * will be needed to iterate over multiple sub textures for regions whos
 * texture coordinates extend out of the range [0,1]
 */
CoglBool
_cogl_texture_can_hardware_repeat (CoglTexture *texture)
{
  return texture->vtable->can_hardware_repeat (texture);
}

/* NB: You can't use this with textures comprised of multiple sub textures (use
 * cogl_texture_is_sliced() to check) since coordinate transformation for such
 * textures will be different for each slice. */
void
_cogl_texture_transform_coords_to_gl (CoglTexture *texture,
                                      float *s,
                                      float *t)
{
  texture->vtable->transform_coords_to_gl (texture, s, t);
}

CoglTransformResult
_cogl_texture_transform_quad_coords_to_gl (CoglTexture *texture,
                                           float *coords)
{
  return texture->vtable->transform_quad_coords_to_gl (texture, coords);
}

CoglBool
cogl_texture_get_gl_texture (CoglTexture *texture,
			     GLuint *out_gl_handle,
			     GLenum *out_gl_target)
{
  return texture->vtable->get_gl_texture (texture,
                                          out_gl_handle, out_gl_target);
}

CoglTextureType
_cogl_texture_get_type (CoglTexture *texture)
{
  return texture->vtable->get_type (texture);
}

void
_cogl_texture_pre_paint (CoglTexture *texture, CoglTexturePrePaintFlags flags)
{
  /* Assert that the storage for the texture exists already if we're
   * about to reference it for painting.
   *
   * Note: we abort on error here since it's a bit late to do anything
   * about it if we fail to allocate the texture and the app could
   * have explicitly allocated the texture earlier to handle problems
   * gracefully.
   *
   * XXX: Maybe it could even be considered a programmer error if the
   * texture hasn't been allocated by this point since it implies we
   * are abount to paint with undefined texture contents?
   */
  cogl_texture_allocate (texture, NULL);

  texture->vtable->pre_paint (texture, flags);
}

void
_cogl_texture_ensure_non_quad_rendering (CoglTexture *texture)
{
  texture->vtable->ensure_non_quad_rendering (texture);
}

CoglBool
_cogl_texture_set_region_from_bitmap (CoglTexture *texture,
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
  _COGL_RETURN_VAL_IF_FAIL ((cogl_bitmap_get_width (bmp) - src_x)
                            >= width, FALSE);
  _COGL_RETURN_VAL_IF_FAIL ((cogl_bitmap_get_height (bmp) - src_y)
                            >= height, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (width > 0, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (height > 0, FALSE);

  /* Assert that the storage for this texture has been allocated */
  if (!cogl_texture_allocate (texture, error))
    return FALSE;

  /* Note that we don't prepare the bitmap for upload here because
     some backends may be internally using a different format for the
     actual GL texture than that reported by
     cogl_texture_get_format. For example the atlas textures are
     always stored in an RGBA texture even if the texture format is
     advertised as RGB. */

  return texture->vtable->set_region (texture,
                                      src_x, src_y,
                                      dst_x, dst_y,
                                      width, height,
                                      level,
                                      bmp,
                                      error);
}

CoglBool
cogl_texture_set_region_from_bitmap (CoglTexture *texture,
                                     int src_x,
                                     int src_y,
                                     int dst_x,
                                     int dst_y,
                                     unsigned int dst_width,
                                     unsigned int dst_height,
                                     CoglBitmap *bitmap)
{
  CoglError *ignore_error = NULL;
  CoglBool status =
    _cogl_texture_set_region_from_bitmap (texture,
                                          src_x, src_y,
                                          dst_width, dst_height,
                                          bitmap,
                                          dst_x, dst_y,
                                          0, /* level */
                                          &ignore_error);

  if (!status)
    cogl_error_free (ignore_error);
  return status;
}

CoglBool
_cogl_texture_set_region (CoglTexture *texture,
                          int width,
                          int height,
                          CoglPixelFormat format,
                          int rowstride,
                          const uint8_t *data,
                          int dst_x,
                          int dst_y,
                          int level,
                          CoglError **error)
{
  CoglContext *ctx = texture->context;
  CoglBitmap *source_bmp;
  CoglBool ret;

  _COGL_RETURN_VAL_IF_FAIL (format != COGL_PIXEL_FORMAT_ANY, FALSE);

  /* Rowstride from width if none specified */
  if (rowstride == 0)
    rowstride = _cogl_pixel_format_get_bytes_per_pixel (format) * width;

  /* Init source bitmap */
  source_bmp = cogl_bitmap_new_for_data (ctx,
                                         width, height,
                                         format,
                                         rowstride,
                                         (uint8_t *) data);

  ret = _cogl_texture_set_region_from_bitmap (texture,
                                              0, 0,
                                              width, height,
                                              source_bmp,
                                              dst_x, dst_y,
                                              level,
                                              error);

  cogl_object_unref (source_bmp);

  return ret;
}

CoglBool
cogl_texture_set_region (CoglTexture *texture,
			 int src_x,
			 int src_y,
			 int dst_x,
			 int dst_y,
			 unsigned int dst_width,
			 unsigned int dst_height,
			 int width,
			 int height,
			 CoglPixelFormat format,
			 unsigned int rowstride,
			 const uint8_t *data)
{
  CoglError *ignore_error = NULL;
  const uint8_t *first_pixel;
  int bytes_per_pixel = _cogl_pixel_format_get_bytes_per_pixel (format);
  CoglBool status;

  /* Rowstride from width if none specified */
  if (rowstride == 0)
    rowstride = bytes_per_pixel * width;

  first_pixel = data + rowstride * src_y + bytes_per_pixel * src_x;

  status = _cogl_texture_set_region (texture,
                                     dst_width,
                                     dst_height,
                                     format,
                                     rowstride,
                                     first_pixel,
                                     dst_x,
                                     dst_y,
                                     0,
                                     &ignore_error);
  if (!status)
    cogl_error_free (ignore_error);
  return status;
}

CoglBool
cogl_texture_set_data (CoglTexture *texture,
                       CoglPixelFormat format,
                       int rowstride,
                       const uint8_t *data,
                       int level,
                       CoglError **error)
{
  int level_width;
  int level_height;

  _cogl_texture_get_level_size (texture,
                                level,
                                &level_width,
                                &level_height,
                                NULL);

  return _cogl_texture_set_region (texture,
                                   level_width,
                                   level_height,
                                   format,
                                   rowstride,
                                   data,
                                   0, 0, /* dest x, y */
                                   level,
                                   error);
}

/* Reads back the contents of a texture by rendering it to the framebuffer
 * and reading back the resulting pixels.
 *
 * It will perform multiple renders if the texture is larger than the
 * current glViewport.
 *
 * It assumes the projection and modelview have already been setup so
 * that rendering to 0,0 with the same width and height of the viewport
 * will exactly cover the viewport.
 *
 * NB: Normally this approach isn't normally used since we can just use
 * glGetTexImage, but may be used as a fallback in some circumstances.
 */
static CoglBool
do_texture_draw_and_read (CoglFramebuffer *fb,
                          CoglPipeline *pipeline,
                          CoglTexture *texture,
                          CoglBitmap *target_bmp,
                          float *viewport,
                          CoglError **error)
{
  float rx1, ry1;
  float rx2, ry2;
  float tx1, ty1;
  float tx2, ty2;
  int bw,  bh;
  CoglBitmap *rect_bmp;
  unsigned int tex_width, tex_height;
  CoglContext *ctx = fb->context;

  tex_width = cogl_texture_get_width (texture);
  tex_height = cogl_texture_get_height (texture);

  ry2 = 0;
  ty2 = 0;

  /* Walk Y axis until whole bitmap height consumed */
  for (bh = tex_height; bh > 0; bh -= viewport[3])
    {
      /* Rectangle Y coords */
      ry1 = ry2;
      ry2 += (bh < viewport[3]) ? bh : viewport[3];

      /* Normalized texture Y coords */
      ty1 = ty2;
      ty2 = (ry2 / (float) tex_height);

      rx2 = 0;
      tx2 = 0;

      /* Walk X axis until whole bitmap width consumed */
      for (bw = tex_width; bw > 0; bw-=viewport[2])
        {
          int width;
          int height;

          /* Rectangle X coords */
          rx1 = rx2;
          rx2 += (bw < viewport[2]) ? bw : viewport[2];

          width = rx2 - rx1;
          height = ry2 - ry1;

          /* Normalized texture X coords */
          tx1 = tx2;
          tx2 = (rx2 / (float) tex_width);

          /* Draw a portion of texture */
          cogl_framebuffer_draw_textured_rectangle (fb,
                                                    pipeline,
                                                    0, 0,
                                                    rx2 - rx1,
                                                    ry2 - ry1,
                                                    tx1, ty1,
                                                    tx2, ty2);

          /* Read into a temporary bitmap */
          rect_bmp = _cogl_bitmap_new_with_malloc_buffer
                                              (ctx,
                                               width, height,
                                               COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                               error);
          if (!rect_bmp)
            return FALSE;

          if (!_cogl_framebuffer_read_pixels_into_bitmap
                                               (fb,
                                                viewport[0], viewport[1],
                                                COGL_READ_PIXELS_COLOR_BUFFER,
                                                rect_bmp,
                                                error))
            {
              cogl_object_unref (rect_bmp);
              return FALSE;
            }

          /* Copy to target bitmap */
          if (!_cogl_bitmap_copy_subregion (rect_bmp,
                                            target_bmp,
                                            0, 0,
                                            rx1, ry1,
                                            width,
                                            height,
                                            error))
            {
              cogl_object_unref (rect_bmp);
              return FALSE;
            }

          /* Free temp bitmap */
          cogl_object_unref (rect_bmp);
        }
    }

  return TRUE;
}

/* Reads back the contents of a texture by rendering it to the framebuffer
 * and reading back the resulting pixels.
 *
 * NB: Normally this approach isn't normally used since we can just use
 * glGetTexImage, but may be used as a fallback in some circumstances.
 */
static CoglBool
_cogl_texture_draw_and_read (CoglTexture *texture,
                             CoglBitmap *target_bmp,
                             GLuint target_gl_format,
                             GLuint target_gl_type,
                             CoglError **error)
{
  CoglFramebuffer *framebuffer = cogl_get_draw_framebuffer ();
  CoglContext *ctx = framebuffer->context;
  float save_viewport[4];
  float viewport[4];
  CoglBool status = FALSE;

  viewport[0] = 0;
  viewport[1] = 0;
  viewport[2] = cogl_framebuffer_get_width (framebuffer);
  viewport[3] = cogl_framebuffer_get_height (framebuffer);

  cogl_framebuffer_get_viewport4fv (framebuffer, save_viewport);
  _cogl_framebuffer_push_projection (framebuffer);
  cogl_framebuffer_orthographic (framebuffer,
                                 0, 0,
                                 viewport[2],
                                 viewport[3],
                                 0, 100);

  cogl_framebuffer_push_matrix (framebuffer);
  cogl_framebuffer_identity_matrix (framebuffer);

  /* Direct copy operation */

  if (ctx->texture_download_pipeline == NULL)
    {
      ctx->texture_download_pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_blend (ctx->texture_download_pipeline,
                               "RGBA = ADD (SRC_COLOR, 0)",
                               NULL);
    }

  cogl_pipeline_set_layer_texture (ctx->texture_download_pipeline, 0, texture);

  cogl_pipeline_set_layer_combine (ctx->texture_download_pipeline,
                                   0, /* layer */
                                   "RGBA = REPLACE (TEXTURE)",
                                   NULL);

  cogl_pipeline_set_layer_filters (ctx->texture_download_pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  if (!do_texture_draw_and_read (framebuffer,
                                 ctx->texture_download_pipeline,
                                 texture, target_bmp, viewport,
                                 error))
    return FALSE;

  /* XXX: As an alleged PowerVR driver bug workaround where the driver
   * is apparently not maintaining the alpha component of some
   * framebuffers we render the alpha component of the texture
   * separately to be sure we retrieve all components of the texture.
   *
   * TODO: verify if this is still an issue
   */
  if ((cogl_texture_get_format (texture) & COGL_A_BIT)/* && a_bits == 0*/)
    {
      uint8_t *srcdata;
      uint8_t *dstdata;
      uint8_t *srcpixel;
      uint8_t *dstpixel;
      int target_width = cogl_bitmap_get_width (target_bmp);
      int target_height = cogl_bitmap_get_height (target_bmp);
      int target_rowstride = cogl_bitmap_get_rowstride (target_bmp);
      int bpp = _cogl_pixel_format_get_bytes_per_pixel (COGL_PIXEL_FORMAT_RGBA_8888);
      int alpha_rowstride = bpp * target_width;
      CoglBitmap *alpha_bmp;
      int x,y;

      if ((dstdata = _cogl_bitmap_map (target_bmp,
                                       COGL_BUFFER_ACCESS_WRITE,
                                       COGL_BUFFER_MAP_HINT_DISCARD,
                                       error)) == NULL)
        goto EXIT;

      /* Create temp bitmap for alpha values */
      alpha_bmp =
        _cogl_bitmap_new_with_malloc_buffer (ctx,
                                             target_width,
                                             target_height,
                                             COGL_PIXEL_FORMAT_RGBA_8888,
                                             error);
      if (!alpha_bmp)
        {
          _cogl_bitmap_unmap (target_bmp);
          goto EXIT;
        }


      /* Draw alpha values into RGB channels */
      cogl_pipeline_set_layer_combine (ctx->texture_download_pipeline,
                                       0, /* layer */
                                       "RGBA = REPLACE (TEXTURE[A])",
                                       NULL);

      if (!do_texture_draw_and_read (framebuffer,
                                     ctx->texture_download_pipeline,
                                     texture, alpha_bmp, viewport,
                                     error))
        {
          cogl_object_unref (alpha_bmp);
          _cogl_bitmap_unmap (target_bmp);
          goto EXIT;
        }

      /* Copy temp R to target A */

      /* Note: we don't try to catch errors since "mapping" an
       * malloc buffer should never fail */
      srcdata = _cogl_bitmap_map (alpha_bmp,
                                  COGL_BUFFER_ACCESS_READ,
                                  0 /* hints */,
                                  NULL);

      for (y=0; y<target_height; ++y)
        {
          for (x=0; x<target_width; ++x)
            {
              srcpixel = srcdata + x*bpp;
              dstpixel = dstdata + x*bpp;
              dstpixel[3] = srcpixel[0];
            }
          srcdata += alpha_rowstride;
          dstdata += target_rowstride;
        }

      _cogl_bitmap_unmap (alpha_bmp);

      _cogl_bitmap_unmap (target_bmp);

      cogl_object_unref (alpha_bmp);
    }

  status = TRUE;

EXIT:
  /* Restore old state */
  cogl_framebuffer_pop_matrix (framebuffer);
  _cogl_framebuffer_pop_projection (framebuffer);
  cogl_framebuffer_set_viewport (framebuffer,
                                 save_viewport[0],
                                 save_viewport[1],
                                 save_viewport[2],
                                 save_viewport[3]);

  return status;
}

static CoglBool
get_texture_bits_via_offscreen (CoglTexture    *texture,
                                int             x,
                                int             y,
                                int             width,
                                int             height,
                                uint8_t         *dst_bits,
                                unsigned int    dst_rowstride,
                                CoglPixelFormat dst_format)
{
  CoglContext *ctx = texture->context;
  CoglOffscreen *offscreen;
  CoglFramebuffer *framebuffer;
  CoglBitmap *bitmap;
  CoglBool ret;
  CoglError *ignore_error = NULL;

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    return FALSE;

  offscreen = _cogl_offscreen_new_to_texture_full
                                      (texture,
                                       COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL,
                                       0);

  framebuffer = COGL_FRAMEBUFFER (offscreen);
  if (!cogl_framebuffer_allocate (framebuffer, &ignore_error))
    {
      cogl_error_free (ignore_error);
      return FALSE;
    }

  bitmap = cogl_bitmap_new_for_data (ctx,
                                     width, height,
                                     dst_format,
                                     dst_rowstride,
                                     dst_bits);
  ret = _cogl_framebuffer_read_pixels_into_bitmap (framebuffer,
                                                   x, y,
                                                   COGL_READ_PIXELS_COLOR_BUFFER,
                                                   bitmap,
                                                   &ignore_error);

  if (!ret)
    cogl_error_free (ignore_error);

  cogl_object_unref (bitmap);

  cogl_object_unref (framebuffer);

  return ret;
}

static CoglBool
get_texture_bits_via_copy (CoglTexture *texture,
                           int x,
                           int y,
                           int width,
                           int height,
                           uint8_t *dst_bits,
                           unsigned int dst_rowstride,
                           CoglPixelFormat dst_format)
{
  unsigned int full_rowstride;
  uint8_t *full_bits;
  CoglBool ret = TRUE;
  int bpp;
  int full_tex_width, full_tex_height;

  full_tex_width = cogl_texture_get_width (texture);
  full_tex_height = cogl_texture_get_height (texture);

  bpp = _cogl_pixel_format_get_bytes_per_pixel (dst_format);

  full_rowstride = bpp * full_tex_width;
  full_bits = g_malloc (full_rowstride * full_tex_height);

  if (texture->vtable->get_data (texture,
                                 dst_format,
                                 full_rowstride,
                                 full_bits))
    {
      uint8_t *dst = dst_bits;
      uint8_t *src = full_bits + x * bpp + y * full_rowstride;
      int i;

      for (i = 0; i < height; i++)
        {
          memcpy (dst, src, bpp * width);
          dst += dst_rowstride;
          src += full_rowstride;
        }
    }
  else
    ret = FALSE;

  g_free (full_bits);

  return ret;
}

typedef struct
{
  int orig_width;
  int orig_height;
  CoglBitmap *target_bmp;
  uint8_t *target_bits;
  CoglBool success;
  CoglError *error;
} CoglTextureGetData;

static void
texture_get_cb (CoglTexture *texture,
                const float *subtexture_coords,
                const float *virtual_coords,
                void        *user_data)
{
  CoglTextureGetData *tg_data = user_data;
  CoglPixelFormat format = cogl_bitmap_get_format (tg_data->target_bmp);
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (format);
  unsigned int rowstride = cogl_bitmap_get_rowstride (tg_data->target_bmp);
  int subtexture_width = cogl_texture_get_width (texture);
  int subtexture_height = cogl_texture_get_height (texture);

  int x_in_subtexture = (int) (0.5 + subtexture_width * subtexture_coords[0]);
  int y_in_subtexture = (int) (0.5 + subtexture_height * subtexture_coords[1]);
  int width = ((int) (0.5 + subtexture_width * subtexture_coords[2])
               - x_in_subtexture);
  int height = ((int) (0.5 + subtexture_height * subtexture_coords[3])
                - y_in_subtexture);
  int x_in_bitmap = (int) (0.5 + tg_data->orig_width * virtual_coords[0]);
  int y_in_bitmap = (int) (0.5 + tg_data->orig_height * virtual_coords[1]);

  uint8_t *dst_bits;

  if (!tg_data->success)
    return;

  dst_bits = tg_data->target_bits + x_in_bitmap * bpp + y_in_bitmap * rowstride;

  /* If we can read everything as a single slice, then go ahead and do that
   * to avoid allocating an FBO. We'll leave it up to the GL implementation to
   * do glGetTexImage as efficiently as possible. (GLES doesn't have that,
   * so we'll fall through) */
  if (x_in_subtexture == 0 && y_in_subtexture == 0 &&
      width == subtexture_width && height == subtexture_height)
    {
      if (texture->vtable->get_data (texture,
                                     format,
                                     rowstride,
                                     dst_bits))
        return;
    }

  /* Next best option is a FBO and glReadPixels */
  if (get_texture_bits_via_offscreen (texture,
                                      x_in_subtexture, y_in_subtexture,
                                      width, height,
                                      dst_bits,
                                      rowstride,
                                      format))
    return;

  /* Getting ugly: read the entire texture, copy out the part we want */
  if (get_texture_bits_via_copy (texture,
                                 x_in_subtexture, y_in_subtexture,
                                 width, height,
                                 dst_bits,
                                 rowstride,
                                 format))
    return;

  /* No luck, the caller will fall back to the draw-to-backbuffer and
   * read implementation */
  tg_data->success = FALSE;
}

int
cogl_texture_get_data (CoglTexture *texture,
		       CoglPixelFormat format,
		       unsigned int rowstride,
		       uint8_t *data)
{
  CoglContext *ctx = texture->context;
  int bpp;
  int byte_size;
  CoglPixelFormat closest_format;
  GLenum closest_gl_format;
  GLenum closest_gl_type;
  CoglBitmap *target_bmp;
  int tex_width;
  int tex_height;
  CoglPixelFormat texture_format;
  CoglError *ignore_error = NULL;

  CoglTextureGetData tg_data;

  texture_format = cogl_texture_get_format (texture);

  /* Default to internal format if none specified */
  if (format == COGL_PIXEL_FORMAT_ANY)
    format = texture_format;

  tex_width = cogl_texture_get_width (texture);
  tex_height = cogl_texture_get_height (texture);

  /* Rowstride from texture width if none specified */
  bpp = _cogl_pixel_format_get_bytes_per_pixel (format);
  if (rowstride == 0)
    rowstride = tex_width * bpp;

  /* Return byte size if only that requested */
  byte_size = tex_height * rowstride;
  if (data == NULL)
    return byte_size;

  closest_format =
    ctx->texture_driver->find_best_gl_get_data_format (ctx,
                                                       format,
                                                       &closest_gl_format,
                                                       &closest_gl_type);

  /* We can assume that whatever data GL gives us will have the
     premult status of the original texture */
  if (COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT (closest_format))
    closest_format = ((closest_format & ~COGL_PREMULT_BIT) |
                      (texture_format & COGL_PREMULT_BIT));

  /* If the application is requesting a conversion from a
   * component-alpha texture and the driver doesn't support them
   * natively then we can only read into an alpha-format buffer. In
   * this case the driver will be faking the alpha textures with a
   * red-component texture and it won't swizzle to the correct format
   * while reading */
  if ((ctx->private_feature_flags & COGL_PRIVATE_FEATURE_ALPHA_TEXTURES) == 0)
    {
      if (texture_format == COGL_PIXEL_FORMAT_A_8)
        {
          closest_format = COGL_PIXEL_FORMAT_A_8;
          closest_gl_format = GL_RED;
          closest_gl_type = GL_UNSIGNED_BYTE;
        }
      else if (format == COGL_PIXEL_FORMAT_A_8)
        {
          /* If we are converting to a component-alpha texture then we
           * need to read all of the components to a temporary buffer
           * because there is no way to get just the 4th component.
           * Note: it doesn't matter whether the texture is
           * pre-multiplied here because we're only going to look at
           * the alpha component */
          closest_format = COGL_PIXEL_FORMAT_RGBA_8888;
          closest_gl_format = GL_RGBA;
          closest_gl_type = GL_UNSIGNED_BYTE;
        }
    }

  /* Is the requested format supported? */
  if (closest_format == format)
    /* Target user data directly */
    target_bmp = cogl_bitmap_new_for_data (ctx,
                                           tex_width,
                                           tex_height,
                                           format,
                                           rowstride,
                                           data);
  else
    {
      target_bmp = _cogl_bitmap_new_with_malloc_buffer (ctx,
                                                        tex_width, tex_height,
                                                        closest_format,
                                                        &ignore_error);
      if (!target_bmp)
        {
          cogl_error_free (ignore_error);
          return 0;
        }
    }

  tg_data.target_bits = _cogl_bitmap_map (target_bmp, COGL_BUFFER_ACCESS_WRITE,
                                          COGL_BUFFER_MAP_HINT_DISCARD,
                                          &ignore_error);
  if (tg_data.target_bits)
    {
      tg_data.orig_width = tex_width;
      tg_data.orig_height = tex_height;
      tg_data.target_bmp = target_bmp;
      tg_data.error = NULL;
      tg_data.success = TRUE;

      /* If there are any dependent framebuffers on the texture then we
         need to flush their journals so the texture contents will be
         up-to-date */
      _cogl_texture_flush_journal_rendering (texture);

      /* Iterating through the subtextures allows piecing together
       * the data for a sliced texture, and allows us to do the
       * read-from-framebuffer logic here in a simple fashion rather than
       * passing offsets down through the code. */
      cogl_meta_texture_foreach_in_region (COGL_META_TEXTURE (texture),
                                           0, 0, 1, 1,
                                           COGL_PIPELINE_WRAP_MODE_REPEAT,
                                           COGL_PIPELINE_WRAP_MODE_REPEAT,
                                           texture_get_cb,
                                           &tg_data);

      _cogl_bitmap_unmap (target_bmp);
    }
  else
    {
      cogl_error_free (ignore_error);
      tg_data.success = FALSE;
    }

  /* XXX: In some cases _cogl_texture_2d_download_from_gl may fail
   * to read back the texture data; such as for GLES which doesn't
   * support glGetTexImage, so here we fallback to drawing the
   * texture and reading the pixels from the framebuffer. */
  if (!tg_data.success)
    {
      if (!_cogl_texture_draw_and_read (texture, target_bmp,
                                        closest_gl_format,
                                        closest_gl_type,
                                        &ignore_error))
        {
          /* We have no more fallbacks so we just give up and
           * hope for the best */
          g_warning ("Failed to read texture since draw-and-read "
                     "fallback failed: %s", ignore_error->message);
          cogl_error_free (ignore_error);
          cogl_object_unref (target_bmp);
          return 0;
        }
    }

  /* Was intermediate used? */
  if (closest_format != format)
    {
      CoglBitmap *new_bmp;
      CoglBool result;
      CoglError *error = NULL;

      /* Convert to requested format directly into the user's buffer */
      new_bmp = cogl_bitmap_new_for_data (ctx,
                                          tex_width, tex_height,
                                          format,
                                          rowstride,
                                          data);
      result = _cogl_bitmap_convert_into_bitmap (target_bmp, new_bmp, &error);

      if (!result)
        {
          cogl_error_free (error);
          /* Return failure after cleaning up */
          byte_size = 0;
        }

      cogl_object_unref (new_bmp);
    }

  cogl_object_unref (target_bmp);

  return byte_size;
}

static void
_cogl_texture_framebuffer_destroy_cb (void *user_data,
                                      void *instance)
{
  CoglTexture *tex = user_data;
  CoglFramebuffer *framebuffer = instance;

  tex->framebuffers = g_list_remove (tex->framebuffers, framebuffer);
}

void
_cogl_texture_associate_framebuffer (CoglTexture *texture,
                                     CoglFramebuffer *framebuffer)
{
  static CoglUserDataKey framebuffer_destroy_notify_key;

  /* Note: we don't take a reference on the framebuffer here because
   * that would introduce a circular reference. */
  texture->framebuffers = g_list_prepend (texture->framebuffers, framebuffer);

  /* Since we haven't taken a reference on the framebuffer we setup
    * some private data so we will be notified if it is destroyed... */
  _cogl_object_set_user_data (COGL_OBJECT (framebuffer),
                              &framebuffer_destroy_notify_key,
                              texture,
                              _cogl_texture_framebuffer_destroy_cb);
}

const GList *
_cogl_texture_get_associated_framebuffers (CoglTexture *texture)
{
  return texture->framebuffers;
}

void
_cogl_texture_flush_journal_rendering (CoglTexture *texture)
{
  GList *l;

  /* It could be that a referenced texture is part of a framebuffer
   * which has an associated journal that must be flushed before it
   * can be sampled from by the current primitive... */
  for (l = texture->framebuffers; l; l = l->next)
    _cogl_framebuffer_flush_journal (l->data);
}

/* This function lets you define a meta texture as a grid of textures
 * whereby the x and y grid-lines are defined by an array of
 * CoglSpans. With that grid based description this function can then
 * iterate all the cells of the grid that lye within a region
 * specified as virtual, meta-texture, coordinates.  This function can
 * also cope with regions that extend beyond the original meta-texture
 * grid by iterating cells repeatedly according to the wrap_x/y
 * arguments.
 *
 * To differentiate between texture coordinates of a specific, real,
 * slice texture and the texture coordinates of a composite, meta
 * texture, the coordinates of the meta texture are called "virtual"
 * coordinates and the coordinates of spans are called "slice"
 * coordinates.
 *
 * Note: no guarantee is given about the order in which the slices
 * will be visited.
 *
 * Note: The slice coordinates passed to @callback are always
 * normalized coordinates even if the span coordinates aren't
 * normalized.
 */
void
_cogl_texture_spans_foreach_in_region (CoglSpan *x_spans,
                                       int n_x_spans,
                                       CoglSpan *y_spans,
                                       int n_y_spans,
                                       CoglTexture **textures,
                                       float *virtual_coords,
                                       float x_normalize_factor,
                                       float y_normalize_factor,
                                       CoglPipelineWrapMode wrap_x,
                                       CoglPipelineWrapMode wrap_y,
                                       CoglMetaTextureCallback callback,
                                       void *user_data)
{
  CoglSpanIter iter_x;
  CoglSpanIter iter_y;
  float slice_coords[4];
  float span_virtual_coords[4];

  /* Iterate the y axis of the virtual rectangle */
  for (_cogl_span_iter_begin (&iter_y,
                              y_spans,
                              n_y_spans,
                              y_normalize_factor,
                              virtual_coords[1],
                              virtual_coords[3],
                              wrap_y);
       !_cogl_span_iter_end (&iter_y);
       _cogl_span_iter_next (&iter_y))
    {
      if (iter_y.flipped)
        {
          slice_coords[1] = iter_y.intersect_end;
          slice_coords[3] = iter_y.intersect_start;
          span_virtual_coords[1] = iter_y.intersect_end;
          span_virtual_coords[3] = iter_y.intersect_start;
        }
      else
        {
          slice_coords[1] = iter_y.intersect_start;
          slice_coords[3] = iter_y.intersect_end;
          span_virtual_coords[1] = iter_y.intersect_start;
          span_virtual_coords[3] = iter_y.intersect_end;
        }

      /* Map the current intersection to normalized slice coordinates */
      slice_coords[1] = (slice_coords[1] - iter_y.pos) / iter_y.span->size;
      slice_coords[3] = (slice_coords[3] - iter_y.pos) / iter_y.span->size;

      /* Iterate the x axis of the virtual rectangle */
      for (_cogl_span_iter_begin (&iter_x,
                                  x_spans,
                                  n_x_spans,
                                  x_normalize_factor,
                                  virtual_coords[0],
                                  virtual_coords[2],
                                  wrap_x);
	   !_cogl_span_iter_end (&iter_x);
	   _cogl_span_iter_next (&iter_x))
        {
          CoglTexture *span_tex;

          if (iter_x.flipped)
            {
              slice_coords[0] = iter_x.intersect_end;
              slice_coords[2] = iter_x.intersect_start;
              span_virtual_coords[0] = iter_x.intersect_end;
              span_virtual_coords[2] = iter_x.intersect_start;
            }
          else
            {
              slice_coords[0] = iter_x.intersect_start;
              slice_coords[2] = iter_x.intersect_end;
              span_virtual_coords[0] = iter_x.intersect_start;
              span_virtual_coords[2] = iter_x.intersect_end;
            }

          /* Map the current intersection to normalized slice coordinates */
          slice_coords[0] = (slice_coords[0] - iter_x.pos) / iter_x.span->size;
          slice_coords[2] = (slice_coords[2] - iter_x.pos) / iter_x.span->size;

	  /* Pluck out the cogl texture for this span */
          span_tex = textures[iter_y.index * n_x_spans + iter_x.index];

          callback (COGL_TEXTURE (span_tex),
                    slice_coords,
                    span_virtual_coords,
                    user_data);
	}
    }
}

void
_cogl_texture_set_allocated (CoglTexture *texture,
                             CoglBool allocated)
{
  texture->allocated = allocated;
}

CoglBool
cogl_texture_allocate (CoglTexture *texture,
                       CoglError **error)
{
  if (texture->allocated)
    return TRUE;

  texture->allocated = texture->vtable->allocate (texture, error);

  return texture->allocated;
}
