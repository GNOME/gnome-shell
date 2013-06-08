/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010,2011 Intel Corporation.
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

#include "cogl-debug.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-atlas-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-sub-texture-private.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-texture-driver.h"
#include "cogl-rectangle-map.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-atlas.h"
#include "cogl1-context.h"
#include "cogl-sub-texture.h"
#include "cogl-error-private.h"
#include "cogl-texture-gl-private.h"

#include <stdlib.h>

static void _cogl_atlas_texture_free (CoglAtlasTexture *sub_tex);

COGL_TEXTURE_INTERNAL_DEFINE (AtlasTexture, atlas_texture);

static const CoglTextureVtable cogl_atlas_texture_vtable;

static CoglSubTexture *
_cogl_atlas_texture_create_sub_texture (CoglTexture *full_texture,
                                        const CoglRectangleMapEntry *rectangle)
{
  CoglContext *ctx = full_texture->context;
  /* Create a subtexture for the given rectangle not including the
     1-pixel border */
  return cogl_sub_texture_new (ctx,
                               full_texture,
                               rectangle->x + 1,
                               rectangle->y + 1,
                               rectangle->width - 2,
                               rectangle->height - 2);
}

static void
_cogl_atlas_texture_update_position_cb (void *user_data,
                                        CoglTexture *new_texture,
                                        const CoglRectangleMapEntry *rectangle)
{
  CoglAtlasTexture *atlas_tex = user_data;

  /* Update the sub texture */
  if (atlas_tex->sub_texture)
    cogl_object_unref (atlas_tex->sub_texture);
  atlas_tex->sub_texture = COGL_TEXTURE (
    _cogl_atlas_texture_create_sub_texture (new_texture, rectangle));

  /* Update the position */
  atlas_tex->rectangle = *rectangle;
}

static void
_cogl_atlas_texture_pre_reorganize_foreach_cb
                                         (const CoglRectangleMapEntry *entry,
                                          void *rectangle_data,
                                          void *user_data)
{
  CoglAtlasTexture *atlas_tex = rectangle_data;

  /* Keep a reference to the texture because we don't want it to be
     destroyed during the reorganization */
  cogl_object_ref (atlas_tex);

  /* Notify cogl-pipeline.c that the texture's underlying GL texture
   * storage is changing so it knows it may need to bind a new texture
   * if the CoglTexture is reused with the same texture unit. */
  _cogl_pipeline_texture_storage_change_notify (COGL_TEXTURE (atlas_tex));
}

static void
_cogl_atlas_texture_pre_reorganize_cb (void *data)
{
  CoglAtlas *atlas = data;

  /* We don't know if any journal entries currently depend on OpenGL
   * texture coordinates that would be invalidated by reorganizing
   * this atlas so we flush all journals before migrating.
   *
   * We are assuming that texture atlas migration never happens
   * during a flush so we don't have to consider recursion here.
   */
  cogl_flush ();

  if (atlas->map)
    _cogl_rectangle_map_foreach (atlas->map,
                                 _cogl_atlas_texture_pre_reorganize_foreach_cb,
                                 NULL);
}

typedef struct
{
  CoglAtlasTexture **textures;
  /* Number of textures found so far */
  unsigned int n_textures;
} CoglAtlasTextureGetRectanglesData;

static void
_cogl_atlas_texture_get_rectangles_cb (const CoglRectangleMapEntry *entry,
                                       void *rectangle_data,
                                       void *user_data)
{
  CoglAtlasTextureGetRectanglesData *data = user_data;

  data->textures[data->n_textures++] = rectangle_data;
}

static void
_cogl_atlas_texture_post_reorganize_cb (void *user_data)
{
  CoglAtlas *atlas = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (atlas->map)
    {
      CoglAtlasTextureGetRectanglesData data;
      unsigned int i;

      data.textures = g_new (CoglAtlasTexture *,
                             _cogl_rectangle_map_get_n_rectangles (atlas->map));
      data.n_textures = 0;

      /* We need to remove all of the references that we took during
         the preorganize callback. We have to get a separate array of
         the textures because CoglRectangleMap doesn't support
         removing rectangles during iteration */
      _cogl_rectangle_map_foreach (atlas->map,
                                   _cogl_atlas_texture_get_rectangles_cb,
                                   &data);

      for (i = 0; i < data.n_textures; i++)
        {
          /* Ignore textures that don't have an atlas yet. This will
             happen when a new texture is added because we allocate
             the structure for the texture so that it can get stored
             in the atlas but it isn't a valid object yet */
          if (data.textures[i]->atlas)
            cogl_object_unref (data.textures[i]);
        }

      g_free (data.textures);
    }

  /* Notify any listeners that an atlas has changed */
  g_hook_list_invoke (&ctx->atlas_reorganize_callbacks, FALSE);
}

static void
_cogl_atlas_texture_atlas_destroyed_cb (void *user_data)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Remove the atlas from the global list */
  ctx->atlases = g_slist_remove (ctx->atlases, user_data);
}

static CoglAtlas *
_cogl_atlas_texture_create_atlas (CoglContext *ctx)
{
  static CoglUserDataKey atlas_private_key;

  CoglAtlas *atlas = _cogl_atlas_new (COGL_PIXEL_FORMAT_RGBA_8888,
                                      0,
                                      _cogl_atlas_texture_update_position_cb);

  _cogl_atlas_add_reorganize_callback (atlas,
                                       _cogl_atlas_texture_pre_reorganize_cb,
                                       _cogl_atlas_texture_post_reorganize_cb,
                                       atlas);

  ctx->atlases = g_slist_prepend (ctx->atlases, atlas);

  /* Set some data on the atlas so we can get notification when it is
     destroyed in order to remove it from the list. ctx->atlases
     effectively holds a weak reference. We don't need a strong
     reference because the atlas textures take a reference on the
     atlas so it will stay alive */
  cogl_object_set_user_data (COGL_OBJECT (atlas), &atlas_private_key, atlas,
                             _cogl_atlas_texture_atlas_destroyed_cb);

  return atlas;
}

static void
_cogl_atlas_texture_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglMetaTextureCallback callback,
                                       void *user_data)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);
  CoglMetaTexture *meta_texture = COGL_META_TEXTURE (atlas_tex->sub_texture);

  /* Forward on to the sub texture */
  cogl_meta_texture_foreach_in_region (meta_texture,
                                       virtual_tx_1,
                                       virtual_ty_1,
                                       virtual_tx_2,
                                       virtual_ty_2,
                                       COGL_PIPELINE_WRAP_MODE_REPEAT,
                                       COGL_PIPELINE_WRAP_MODE_REPEAT,
                                       callback,
                                       user_data);
}

static void
_cogl_atlas_texture_gl_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                       GLenum wrap_mode_s,
                                                       GLenum wrap_mode_t,
                                                       GLenum wrap_mode_p)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_gl_flush_legacy_texobj_wrap_modes (atlas_tex->sub_texture,
                                                   wrap_mode_s,
                                                   wrap_mode_t,
                                                   wrap_mode_p);
}

static void
_cogl_atlas_texture_remove_from_atlas (CoglAtlasTexture *atlas_tex)
{
  if (atlas_tex->atlas)
    {
      _cogl_atlas_remove (atlas_tex->atlas,
                          &atlas_tex->rectangle);

      cogl_object_unref (atlas_tex->atlas);
      atlas_tex->atlas = NULL;
    }
}

static void
_cogl_atlas_texture_free (CoglAtlasTexture *atlas_tex)
{
  _cogl_atlas_texture_remove_from_atlas (atlas_tex);

  if (atlas_tex->sub_texture)
    cogl_object_unref (atlas_tex->sub_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (atlas_tex));
}

static int
_cogl_atlas_texture_get_max_waste (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_get_max_waste (atlas_tex->sub_texture);
}

static CoglBool
_cogl_atlas_texture_is_sliced (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_is_sliced (atlas_tex->sub_texture);
}

static CoglBool
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

static CoglTransformResult
_cogl_atlas_texture_transform_quad_coords_to_gl (CoglTexture *tex,
                                                 float *coords)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return _cogl_texture_transform_quad_coords_to_gl (atlas_tex->sub_texture,
                                                    coords);
}

static CoglBool
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
_cogl_atlas_texture_gl_flush_legacy_texobj_filters (CoglTexture *tex,
                                                    GLenum min_filter,
                                                    GLenum mag_filter)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_gl_flush_legacy_texobj_filters (atlas_tex->sub_texture,
                                                min_filter, mag_filter);
}

static void
_cogl_atlas_texture_migrate_out_of_atlas (CoglAtlasTexture *atlas_tex)
{
  CoglTexture *standalone_tex;

  /* Make sure this texture is not in the atlas */
  if (!atlas_tex->atlas)
    return;

  COGL_NOTE (ATLAS, "Migrating texture out of the atlas");

  /* We don't know if any journal entries currently depend on
   * OpenGL texture coordinates that would be invalidated by
   * migrating textures in this atlas so we flush all journals
   * before migrating.
   *
   * We are assuming that texture atlas migration never happens
   * during a flush so we don't have to consider recursion here.
   */
  cogl_flush ();

  standalone_tex =
    _cogl_atlas_copy_rectangle (atlas_tex->atlas,
                                atlas_tex->rectangle.x + 1,
                                atlas_tex->rectangle.y + 1,
                                atlas_tex->rectangle.width - 2,
                                atlas_tex->rectangle.height - 2,
                                atlas_tex->format);
  /* Note: we simply silently ignore failures to migrate a texture
   * out (most likely due to lack of memory) and hope for the
   * best.
   *
   * Maybe we should find a way to report the problem back to the
   * app.
   */
  if (!standalone_tex)
    return;

  /* Notify cogl-pipeline.c that the texture's underlying GL texture
   * storage is changing so it knows it may need to bind a new texture
   * if the CoglTexture is reused with the same texture unit. */
  _cogl_pipeline_texture_storage_change_notify (COGL_TEXTURE (atlas_tex));

  /* We need to unref the sub texture after doing the copy because
     the copy can involve rendering which might cause the texture
     to be used if it is used from a layer that is left in a
     texture unit */
  cogl_object_unref (atlas_tex->sub_texture);
  atlas_tex->sub_texture = standalone_tex;

  _cogl_atlas_texture_remove_from_atlas (atlas_tex);
}

static void
_cogl_atlas_texture_pre_paint (CoglTexture *tex, CoglTexturePrePaintFlags flags)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  if ((flags & COGL_TEXTURE_NEEDS_MIPMAP))
    /* Mipmaps do not work well with the current atlas so instead
       we'll just migrate the texture out and use a regular texture */
    _cogl_atlas_texture_migrate_out_of_atlas (atlas_tex);

  /* Forward on to the sub texture */
  _cogl_texture_pre_paint (atlas_tex->sub_texture, flags);
}

static void
_cogl_atlas_texture_ensure_non_quad_rendering (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Sub textures can't support non-quad rendering so we'll just
     migrate the texture out */
  _cogl_atlas_texture_migrate_out_of_atlas (atlas_tex);

  /* Forward on to the sub texture */
  _cogl_texture_ensure_non_quad_rendering (atlas_tex->sub_texture);
}

static CoglBool
_cogl_atlas_texture_set_region_with_border (CoglAtlasTexture *atlas_tex,
                                            int src_x,
                                            int src_y,
                                            int dst_x,
                                            int dst_y,
                                            int dst_width,
                                            int dst_height,
                                            CoglBitmap *bmp,
                                            CoglError **error)
{
  CoglAtlas *atlas = atlas_tex->atlas;

  /* Copy the central data */
  if (!_cogl_texture_set_region_from_bitmap (atlas->texture,
                                             src_x, src_y,
                                             dst_width,
                                             dst_height,
                                             bmp,
                                             dst_x + atlas_tex->rectangle.x + 1,
                                             dst_y + atlas_tex->rectangle.y + 1,
                                             0, /* level 0 */
                                             error))
    return FALSE;

  /* Update the left edge pixels */
  if (dst_x == 0 &&
      !_cogl_texture_set_region_from_bitmap (atlas->texture,
                                             src_x, src_y,
                                             1, dst_height,
                                             bmp,
                                             atlas_tex->rectangle.x,
                                             dst_y + atlas_tex->rectangle.y + 1,
                                             0, /* level 0 */
                                             error))
    return FALSE;
  /* Update the right edge pixels */
  if (dst_x + dst_width == atlas_tex->rectangle.width - 2 &&
      !_cogl_texture_set_region_from_bitmap (atlas->texture,
                                             src_x + dst_width - 1, src_y,
                                             1, dst_height,
                                             bmp,
                                             atlas_tex->rectangle.x +
                                             atlas_tex->rectangle.width - 1,
                                             dst_y + atlas_tex->rectangle.y + 1,
                                             0, /* level 0 */
                                             error))
    return FALSE;
  /* Update the top edge pixels */
  if (dst_y == 0 &&
      !_cogl_texture_set_region_from_bitmap (atlas->texture,
                                             src_x, src_y,
                                             dst_width, 1,
                                             bmp,
                                             dst_x + atlas_tex->rectangle.x + 1,
                                             atlas_tex->rectangle.y,
                                             0, /* level 0 */
                                             error))
    return FALSE;
  /* Update the bottom edge pixels */
  if (dst_y + dst_height == atlas_tex->rectangle.height - 2 &&
      !_cogl_texture_set_region_from_bitmap (atlas->texture,
                                             src_x, src_y + dst_height - 1,
                                             dst_width, 1,
                                             bmp,
                                             dst_x + atlas_tex->rectangle.x + 1,
                                             atlas_tex->rectangle.y +
                                             atlas_tex->rectangle.height - 1,
                                             0, /* level 0 */
                                             error))
    return FALSE;

  return TRUE;
}

static CoglBitmap *
_cogl_atlas_texture_convert_bitmap_for_upload (CoglAtlasTexture *atlas_tex,
                                               CoglBitmap *bmp,
                                               CoglBool can_convert_in_place,
                                               CoglError **error)
{
  CoglPixelFormat internal_format;
  CoglBitmap *upload_bmp;
  CoglBitmap *override_bmp;

  /* We'll prepare to upload using the format of the actual texture of
     the atlas texture instead of the format reported by
     cogl_texture_get_format which would be the original internal
     format specified when the texture was created. However we'll
     preserve the premult status of the internal format because the
     images are all stored in the original premult format of the
     orignal format so we do need to trigger the conversion */

  internal_format = (COGL_PIXEL_FORMAT_RGBA_8888 |
                     (atlas_tex->format & COGL_PREMULT_BIT));

  upload_bmp = _cogl_bitmap_convert_for_upload (bmp,
                                                internal_format,
                                                can_convert_in_place,
                                                error);
  if (upload_bmp == NULL)
    return NULL;

  /* We'll create another bitmap which uses the same data but
     overrides the format to remove the premult flag so that uploads
     to the atlas texture won't trigger the conversion again */

  override_bmp =
    _cogl_bitmap_new_shared (upload_bmp,
                             cogl_bitmap_get_format (upload_bmp) &
                             ~COGL_PREMULT_BIT,
                             cogl_bitmap_get_width (upload_bmp),
                             cogl_bitmap_get_height (upload_bmp),
                             cogl_bitmap_get_rowstride (upload_bmp));

  cogl_object_unref (upload_bmp);

  return override_bmp;
}

static CoglBool
_cogl_atlas_texture_set_region (CoglTexture *tex,
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
  CoglAtlasTexture  *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  if (level != 0 && atlas_tex->atlas)
    _cogl_atlas_texture_migrate_out_of_atlas (atlas_tex);

  /* If the texture is in the atlas then we need to copy the edge
     pixels to the border */
  if (atlas_tex->atlas)
    {
      CoglBool ret;
      CoglBitmap *upload_bmp =
        _cogl_atlas_texture_convert_bitmap_for_upload (atlas_tex,
                                                       bmp,
                                                       FALSE, /* can't convert
                                                                 in place */
                                                       error);
      if (!upload_bmp)
        return FALSE;

      /* Upload the data ignoring the premult bit */
      ret = _cogl_atlas_texture_set_region_with_border (atlas_tex,
                                                        src_x, src_y,
                                                        dst_x, dst_y,
                                                        dst_width, dst_height,
                                                        upload_bmp,
                                                        error);

      cogl_object_unref (upload_bmp);

      return ret;
    }
  else
    /* Otherwise we can just forward on to the sub texture */
    return _cogl_texture_set_region_from_bitmap (atlas_tex->sub_texture,
                                                 src_x, src_y,
                                                 dst_width, dst_height,
                                                 bmp,
                                                 dst_x, dst_y,
                                                 level,
                                                 error);
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
  return _cogl_texture_gl_get_format (atlas_tex->sub_texture);
}

static CoglBool
_cogl_atlas_texture_can_use_format (CoglPixelFormat format)
{
  /* We don't care about the ordering or the premult status and we can
     accept RGBA or RGB textures. Although we could also accept
     luminance and alpha only textures or 16-bit formats it seems that
     if the application is explicitly using these formats then they've
     got a reason to want the lower memory requirements so putting
     them in the atlas might not be a good idea */
  format &= ~(COGL_PREMULT_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT);
  return (format == COGL_PIXEL_FORMAT_RGB_888 ||
          format == COGL_PIXEL_FORMAT_RGBA_8888);
}

CoglAtlasTexture *
cogl_atlas_texture_new_with_size (CoglContext *ctx,
                                  int width,
                                  int height,
                                  CoglPixelFormat internal_format,
                                  CoglError **error)
{
  CoglAtlasTexture *atlas_tex;

  /* We can't atlas zero-sized textures because it breaks the atlas
     data structure */
  if (width < 1 || height < 1)
    {
      _cogl_set_error (error,
                       COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_SIZE,
                       "1x1 atlas textures not supported");
      return NULL;
    }

  /* If we can't use FBOs then it will be too slow to migrate textures
     and we shouldn't use the atlas */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Atlasing disabled because migrations "
                       "would be too slow");
      return NULL;
    }

  COGL_NOTE (ATLAS, "Adding texture of size %ix%i", width, height);

  /* If the texture is in a strange format then we won't use it */
  if (!_cogl_atlas_texture_can_use_format (internal_format))
    {
      COGL_NOTE (ATLAS, "Texture can not be added because the "
                 "format is unsupported");
      _cogl_set_error (error,
                       COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_FORMAT,
                       "Texture format unsuitable for atlasing");
      return NULL;
    }

  /* We need to allocate the texture now because we need the pointer
     to set as the data for the rectangle in the atlas */
  atlas_tex = g_new (CoglAtlasTexture, 1);
  /* Mark it as having no atlas so we don't try to unref it in
     _cogl_atlas_texture_post_reorganize_cb */
  atlas_tex->atlas = NULL;

  _cogl_texture_init (COGL_TEXTURE (atlas_tex),
                      ctx,
                      width, height,
                      &cogl_atlas_texture_vtable);

  atlas_tex->sub_texture = NULL;

  atlas_tex->format = internal_format;
  atlas_tex->atlas = NULL;

  return _cogl_atlas_texture_object_new (atlas_tex);
}

static CoglBool
_cogl_atlas_texture_allocate (CoglTexture *tex,
                              CoglError **error)
{
  CoglContext *ctx = tex->context;
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);
  CoglAtlas *atlas;
  GSList *l;

  /* Look for an existing atlas that can hold the texture */
  for (l = ctx->atlases; l; l = l->next)
    /* Try to make some space in the atlas for the texture */
    if (_cogl_atlas_reserve_space (atlas = l->data,
                                   /* Add two pixels for the border */
                                   tex->width + 2, tex->height + 2,
                                   atlas_tex))
      {
        cogl_object_ref (atlas);
        break;
      }

  /* If we couldn't find a suitable atlas then start another */
  if (l == NULL)
    {
      atlas = _cogl_atlas_texture_create_atlas (ctx);
      COGL_NOTE (ATLAS, "Created new atlas for textures: %p", atlas);
      if (!_cogl_atlas_reserve_space (atlas,
                                      /* Add two pixels for the border */
                                      tex->width + 2, tex->height + 2,
                                      atlas_tex))
        {
          /* Ok, this means we really can't add it to the atlas */
          cogl_object_unref (atlas);

          _cogl_set_error (error,
                           COGL_SYSTEM_ERROR,
                           COGL_SYSTEM_ERROR_NO_MEMORY,
                           "Not enough memory to atlas texture");
          return FALSE;
        }
    }

  atlas_tex->atlas = atlas;

  return TRUE;
}

CoglAtlasTexture *
_cogl_atlas_texture_new_from_bitmap (CoglBitmap *bmp,
                                     CoglPixelFormat internal_format,
                                     CoglBool can_convert_in_place,
                                     CoglError **error)
{
  CoglContext *ctx = _cogl_bitmap_get_context (bmp);
  CoglAtlasTexture *atlas_tex;
  CoglBitmap *upload_bmp;
  int bmp_width;
  int bmp_height;
  CoglPixelFormat bmp_format;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_bitmap (bmp), NULL);

  bmp_width = cogl_bitmap_get_width (bmp);
  bmp_height = cogl_bitmap_get_height (bmp);
  bmp_format = cogl_bitmap_get_format (bmp);

  internal_format = _cogl_texture_determine_internal_format (bmp_format,
                                                             internal_format);

  atlas_tex = cogl_atlas_texture_new_with_size (ctx,
                                                bmp_width, bmp_height,
                                                internal_format,
                                                error);
  if (!atlas_tex)
    return NULL;

  if (!cogl_texture_allocate (COGL_TEXTURE (atlas_tex), error))
    {
      cogl_object_unref (atlas_tex);
      return NULL;
    }

  upload_bmp =
    _cogl_atlas_texture_convert_bitmap_for_upload (atlas_tex,
                                                   bmp,
                                                   can_convert_in_place,
                                                   error);
  if (upload_bmp == NULL)
    {
      cogl_object_unref (atlas_tex);
      return NULL;
    }

  /* Defer to set_region so that we can share the code for copying the
     edge pixels to the border. */
  if (!_cogl_atlas_texture_set_region_with_border (atlas_tex,
                                                   0, /* src_x */
                                                   0, /* src_y */
                                                   0, /* dst_x */
                                                   0, /* dst_y */
                                                   bmp_width, /* dst_width */
                                                   bmp_height, /* dst_height */
                                                   upload_bmp,
                                                   error))
    {
      cogl_object_unref (upload_bmp);
      cogl_object_unref (atlas_tex);
      return NULL;
    }

  cogl_object_unref (upload_bmp);

  return atlas_tex;
}

CoglAtlasTexture *
cogl_atlas_texture_new_from_bitmap (CoglBitmap *bmp,
                                    CoglPixelFormat internal_format,
                                    CoglError **error)
{
  return _cogl_atlas_texture_new_from_bitmap (bmp, internal_format,
                                              FALSE, error);
}

CoglAtlasTexture *
cogl_atlas_texture_new_from_data (CoglContext *ctx,
                                  int width,
                                  int height,
                                  CoglPixelFormat format,
                                  CoglPixelFormat internal_format,
                                  int rowstride,
                                  const uint8_t *data,
                                  CoglError **error)
{
  CoglBitmap *bmp;
  CoglAtlasTexture *atlas_tex;

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

  atlas_tex = cogl_atlas_texture_new_from_bitmap (bmp, internal_format, error);

  cogl_object_unref (bmp);

  return atlas_tex;
}

CoglAtlasTexture *
cogl_atlas_texture_new_from_file (CoglContext *ctx,
                                  const char *filename,
                                  CoglPixelFormat internal_format,
                                  CoglError **error)
{
  CoglBitmap *bmp;
  CoglAtlasTexture *atlas_tex = NULL;

  _COGL_RETURN_VAL_IF_FAIL (error == NULL || *error == NULL, NULL);

  bmp = cogl_bitmap_new_from_file (filename, error);
  if (bmp == NULL)
    return NULL;

  atlas_tex = _cogl_atlas_texture_new_from_bitmap (bmp,
                                                   internal_format,
                                                   TRUE, /* convert in-place */
                                                   error);

  cogl_object_unref (bmp);

  return atlas_tex;
}

void
_cogl_atlas_texture_add_reorganize_callback (CoglContext *ctx,
                                             GHookFunc callback,
                                             void *user_data)
{
  GHook *hook = g_hook_alloc (&ctx->atlas_reorganize_callbacks);
  hook->func = callback;
  hook->data = user_data;
  g_hook_prepend (&ctx->atlas_reorganize_callbacks, hook);
}

void
_cogl_atlas_texture_remove_reorganize_callback (CoglContext *ctx,
                                                GHookFunc callback,
                                                void *user_data)
{
  GHook *hook = g_hook_find_func_data (&ctx->atlas_reorganize_callbacks,
                                       FALSE,
                                       callback,
                                       user_data);

  if (hook)
    g_hook_destroy_link (&ctx->atlas_reorganize_callbacks, hook);
}

static CoglTextureType
_cogl_atlas_texture_get_type (CoglTexture *tex)
{
  return COGL_TEXTURE_TYPE_2D;
}

static const CoglTextureVtable
cogl_atlas_texture_vtable =
  {
    FALSE, /* not primitive */
    _cogl_atlas_texture_allocate,
    _cogl_atlas_texture_set_region,
    NULL, /* get_data */
    _cogl_atlas_texture_foreach_sub_texture_in_region,
    _cogl_atlas_texture_get_max_waste,
    _cogl_atlas_texture_is_sliced,
    _cogl_atlas_texture_can_hardware_repeat,
    _cogl_atlas_texture_transform_coords_to_gl,
    _cogl_atlas_texture_transform_quad_coords_to_gl,
    _cogl_atlas_texture_get_gl_texture,
    _cogl_atlas_texture_gl_flush_legacy_texobj_filters,
    _cogl_atlas_texture_pre_paint,
    _cogl_atlas_texture_ensure_non_quad_rendering,
    _cogl_atlas_texture_gl_flush_legacy_texobj_wrap_modes,
    _cogl_atlas_texture_get_format,
    _cogl_atlas_texture_get_gl_format,
    _cogl_atlas_texture_get_type,
    NULL, /* is_foreign */
    NULL /* set_auto_mipmap */
  };
