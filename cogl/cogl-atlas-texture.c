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
#include "cogl-atlas-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-texture-driver.h"
#include "cogl-atlas.h"

static void _cogl_atlas_texture_free (CoglAtlasTexture *sub_tex);

COGL_HANDLE_DEFINE (AtlasTexture, atlas_texture);

static const CoglTextureVtable cogl_atlas_texture_vtable;

static void
_cogl_atlas_texture_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglTextureSliceCallback callback,
                                       void *user_data)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_foreach_sub_texture_in_region (atlas_tex->sub_texture,
                                               virtual_tx_1,
                                               virtual_ty_1,
                                               virtual_tx_2,
                                               virtual_ty_2,
                                               callback,
                                               user_data);
}

static void
_cogl_atlas_texture_set_wrap_mode_parameter (CoglTexture *tex,
                                             GLenum wrap_mode)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_set_wrap_mode_parameter (atlas_tex->sub_texture, wrap_mode);
}

static void
_cogl_atlas_texture_free (CoglAtlasTexture *atlas_tex)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Remove the texture from the atlas */
  if (atlas_tex->in_atlas)
    cogl_atlas_remove_rectangle ((atlas_tex->format & COGL_A_BIT) ?
                                 ctx->atlas_alpha :
                                 ctx->atlas_no_alpha,
                                 &atlas_tex->rectangle);

  cogl_handle_unref (atlas_tex->sub_texture);
}

static gint
_cogl_atlas_texture_get_max_waste (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_get_max_waste (atlas_tex->sub_texture);
}

static gboolean
_cogl_atlas_texture_is_sliced (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_is_sliced (atlas_tex->sub_texture);
}

static gboolean
_cogl_atlas_texture_can_hardware_repeat (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return _cogl_texture_can_hardware_repeat (atlas_tex->sub_texture);
}

static void
_cogl_atlas_texture_transform_coords_to_gl (CoglTexture *tex,
                                            float *s,
                                            float *t)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_transform_coords_to_gl (atlas_tex->sub_texture, s, t);
}

static gboolean
_cogl_atlas_texture_get_gl_texture (CoglTexture *tex,
                                    GLuint *out_gl_handle,
                                    GLenum *out_gl_target)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_get_gl_texture (atlas_tex->sub_texture,
                                      out_gl_handle,
                                      out_gl_target);
}

static void
_cogl_atlas_texture_set_filters (CoglTexture *tex,
                                 GLenum min_filter,
                                 GLenum mag_filter)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_set_filters (atlas_tex->sub_texture, min_filter, mag_filter);
}

static void
_cogl_atlas_texture_ensure_mipmaps (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* FIXME: If mipmaps are required then we need to migrate the
     texture out of the atlas because it will show artifacts */

  /* Forward on to the sub texture */
  _cogl_texture_ensure_mipmaps (atlas_tex->sub_texture);
}

static gboolean
_cogl_atlas_texture_set_region (CoglTexture    *tex,
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
  CoglAtlasTexture  *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* If the texture is in the atlas then we need to copy the edge
     pixels to the border */
  if (atlas_tex->in_atlas)
    {
      CoglHandle big_texture;

      _COGL_GET_CONTEXT (ctx, FALSE);

      big_texture = ((atlas_tex->format & COGL_A_BIT) ?
                     ctx->atlas_alpha_texture : ctx->atlas_no_alpha_texture);

      /* Copy the central data */
      if (!cogl_texture_set_region (big_texture,
                                    src_x, src_y,
                                    dst_x + atlas_tex->rectangle.x + 1,
                                    dst_y + atlas_tex->rectangle.y + 1,
                                    dst_width,
                                    dst_height,
                                    width, height,
                                    format,
                                    rowstride,
                                    data))
        return FALSE;

      /* Update the left edge pixels */
      if (dst_x == 0 &&
          !cogl_texture_set_region (big_texture,
                                    src_x, src_y,
                                    atlas_tex->rectangle.x,
                                    dst_y + atlas_tex->rectangle.y + 1,
                                    1, dst_height,
                                    width, height,
                                    format, rowstride,
                                    data))
        return FALSE;
      /* Update the right edge pixels */
      if (dst_x + dst_width == atlas_tex->rectangle.width - 2 &&
          !cogl_texture_set_region (big_texture,
                                    src_x + dst_width - 1, src_y,
                                    atlas_tex->rectangle.x +
                                    atlas_tex->rectangle.width - 1,
                                    dst_y + atlas_tex->rectangle.y + 1,
                                    1, dst_height,
                                    width, height,
                                    format, rowstride,
                                    data))
        return FALSE;
      /* Update the top edge pixels */
      if (dst_y == 0 &&
          !cogl_texture_set_region (big_texture,
                                    src_x, src_y,
                                    dst_x + atlas_tex->rectangle.x + 1,
                                    atlas_tex->rectangle.y,
                                    dst_width, 1,
                                    width, height,
                                    format, rowstride,
                                    data))
        return FALSE;
      /* Update the bottom edge pixels */
      if (dst_y + dst_height == atlas_tex->rectangle.height - 2 &&
          !cogl_texture_set_region (big_texture,
                                    src_x, src_y + dst_height - 1,
                                    dst_x + atlas_tex->rectangle.x + 1,
                                    atlas_tex->rectangle.y +
                                    atlas_tex->rectangle.height - 1,
                                    dst_width, 1,
                                    width, height,
                                    format, rowstride,
                                    data))
        return FALSE;

      return TRUE;
    }
  else
    /* Otherwise we can just forward on to the sub texture */
    return cogl_texture_set_region (atlas_tex->sub_texture,
                                    src_x, src_y,
                                    dst_x, dst_y,
                                    dst_width, dst_height,
                                    width, height,
                                    format, rowstride,
                                    data);
}

static int
_cogl_atlas_texture_get_data (CoglTexture     *tex,
                              CoglPixelFormat  format,
                              unsigned int     rowstride,
                              guint8          *data)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_get_data (atlas_tex->sub_texture,
                                format,
                                rowstride,
                                data);
}

static CoglPixelFormat
_cogl_atlas_texture_get_format (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* We don't want to forward this on the sub-texture because it isn't
     the necessarily the same format. This will happen if the texture
     isn't pre-multiplied */
  return atlas_tex->format;
}

static GLenum
_cogl_atlas_texture_get_gl_format (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return _cogl_texture_get_gl_format (atlas_tex->sub_texture);
}

static gint
_cogl_atlas_texture_get_width (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_get_width (atlas_tex->sub_texture);
}

static gint
_cogl_atlas_texture_get_height (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_get_height (atlas_tex->sub_texture);
}

static gboolean
_cogl_atlas_texture_reserve_space (CoglPixelFormat      format,
                                   CoglAtlas          **atlas_ptr,
                                   CoglHandle          *atlas_tex_ptr,
                                   gpointer             rectangle_data,
                                   guint                width,
                                   guint                height,
                                   CoglAtlasRectangle  *rectangle)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  /* Create the atlas if we haven't already */
  if (*atlas_ptr == NULL)
    *atlas_ptr = cogl_atlas_new (256, 256, NULL);

  /* Create the texture if we haven't already */
  if (*atlas_tex_ptr == NULL)
    *atlas_tex_ptr = _cogl_texture_2d_new_with_size (256, 256,
                                                     COGL_TEXTURE_NONE,
                                                     format);

  /* Try to grab the space in the atlas */
  /* FIXME: This should try to reorganise the atlas to make space and
     grow it if necessary. */
  return cogl_atlas_add_rectangle (*atlas_ptr, width, height,
                                   rectangle_data, rectangle);
}

CoglHandle
_cogl_atlas_texture_new_from_bitmap (CoglHandle       bmp_handle,
                                     CoglTextureFlags flags,
                                     CoglPixelFormat  internal_format)
{
  CoglAtlasTexture       *atlas_tex;
  CoglBitmap             *bmp = (CoglBitmap *) bmp_handle;
  CoglTextureUploadData   upload_data;
  CoglAtlas             **atlas_ptr;
  CoglHandle             *atlas_tex_ptr;
  CoglAtlasRectangle      rectangle;
  gfloat                  tx1, ty1, tx2, ty2;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  g_return_val_if_fail (bmp_handle != COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  /* We can't put the texture in the atlas if there are any special
     flags. This precludes textures with COGL_TEXTURE_NO_ATLAS and
     COGL_TEXTURE_NO_SLICING from being atlased */
  if (flags)
    return COGL_INVALID_HANDLE;

  /* We can't atlas zero-sized textures because it breaks the atlas
     data structure */
  if (bmp->width < 1 || bmp->height < 1)
    return COGL_INVALID_HANDLE;

  /* If we can't read back texture data then it will be too slow to
     migrate textures and we shouldn't use the atlas */
  if (!cogl_features_available (COGL_FEATURE_TEXTURE_READ_PIXELS))
    return COGL_INVALID_HANDLE;

  upload_data.bitmap = *bmp;
  upload_data.bitmap_owner = FALSE;

  if (!_cogl_texture_upload_data_prepare_format (&upload_data,
                                                 &internal_format))
    {
      _cogl_texture_upload_data_free (&upload_data);
      return COGL_INVALID_HANDLE;
    }

  /* If the texture is in a strange format then we can't use it */
  if (internal_format != COGL_PIXEL_FORMAT_RGB_888 &&
      (internal_format & ~COGL_PREMULT_BIT) != COGL_PIXEL_FORMAT_RGBA_8888)
    {
      _cogl_texture_upload_data_free (&upload_data);
      return COGL_INVALID_HANDLE;
    }

  if ((internal_format & COGL_A_BIT))
    {
      atlas_ptr = &ctx->atlas_alpha;
      atlas_tex_ptr = &ctx->atlas_alpha_texture;
    }
  else
    {
      atlas_ptr = &ctx->atlas_no_alpha;
      atlas_tex_ptr = &ctx->atlas_no_alpha_texture;
    }

  /* We need to allocate the texture now because we need the pointer
     to set as the data for the rectangle in the atlas */
  atlas_tex = g_new (CoglAtlasTexture, 1);

  /* Try to make some space in the atlas for the texture */
  if (!_cogl_atlas_texture_reserve_space (internal_format,
                                          atlas_ptr,
                                          atlas_tex_ptr,
                                          atlas_tex,
                                          /* Add two pixels for the border */
                                          upload_data.bitmap.width + 2,
                                          upload_data.bitmap.height + 2,
                                          &rectangle))
    {
      g_free (atlas_tex);
      _cogl_texture_upload_data_free (&upload_data);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_upload_data_convert (&upload_data, internal_format))
    {
      cogl_atlas_remove_rectangle (*atlas_ptr, &rectangle);
      g_free (atlas_tex);
      _cogl_texture_upload_data_free (&upload_data);
      return COGL_INVALID_HANDLE;
    }

  tx1 = (rectangle.x + 1) / (gfloat) cogl_atlas_get_width (*atlas_ptr);
  ty1 = (rectangle.y + 1) / (gfloat) cogl_atlas_get_height (*atlas_ptr);
  tx2 = ((rectangle.x + rectangle.width - 1) /
         (gfloat) cogl_atlas_get_width (*atlas_ptr));
  ty2 = ((rectangle.y + rectangle.height - 1) /
         (gfloat) cogl_atlas_get_height (*atlas_ptr));

  atlas_tex->_parent.vtable = &cogl_atlas_texture_vtable;
  atlas_tex->format = internal_format;
  atlas_tex->rectangle = rectangle;
  atlas_tex->in_atlas = TRUE;
  atlas_tex->sub_texture = cogl_texture_new_from_sub_texture (*atlas_tex_ptr,
                                                              tx1, ty1,
                                                              tx2, ty2);

  /* Defer to set_region so that we can share the code for copying the
     edge pixels to the border */
  _cogl_atlas_texture_set_region (COGL_TEXTURE (atlas_tex),
                                  0, 0,
                                  0, 0,
                                  upload_data.bitmap.width,
                                  upload_data.bitmap.height,
                                  upload_data.bitmap.width,
                                  upload_data.bitmap.height,
                                  upload_data.bitmap.format,
                                  upload_data.bitmap.rowstride,
                                  upload_data.bitmap.data);

  return _cogl_atlas_texture_handle_new (atlas_tex);
}

static const CoglTextureVtable
cogl_atlas_texture_vtable =
  {
    _cogl_atlas_texture_set_region,
    _cogl_atlas_texture_get_data,
    _cogl_atlas_texture_foreach_sub_texture_in_region,
    _cogl_atlas_texture_get_max_waste,
    _cogl_atlas_texture_is_sliced,
    _cogl_atlas_texture_can_hardware_repeat,
    _cogl_atlas_texture_transform_coords_to_gl,
    _cogl_atlas_texture_get_gl_texture,
    _cogl_atlas_texture_set_filters,
    _cogl_atlas_texture_ensure_mipmaps,
    _cogl_atlas_texture_set_wrap_mode_parameter,
    _cogl_atlas_texture_get_format,
    _cogl_atlas_texture_get_gl_format,
    _cogl_atlas_texture_get_width,
    _cogl_atlas_texture_get_height
  };
