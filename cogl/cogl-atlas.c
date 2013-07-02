/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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

#include "cogl-atlas.h"
#include "cogl-rectangle-map.h"
#include "cogl-context-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-2d-sliced.h"
#include "cogl-texture-driver.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-debug.h"
#include "cogl-framebuffer-private.h"
#include "cogl-blit.h"
#include "cogl-private.h"

#include <stdlib.h>

static void _cogl_atlas_free (CoglAtlas *atlas);

COGL_OBJECT_INTERNAL_DEFINE (Atlas, atlas);

CoglAtlas *
_cogl_atlas_new (CoglPixelFormat texture_format,
                 CoglAtlasFlags flags,
                 CoglAtlasUpdatePositionCallback update_position_cb)
{
  CoglAtlas *atlas = g_new (CoglAtlas, 1);

  atlas->update_position_cb = update_position_cb;
  atlas->map = NULL;
  atlas->texture = NULL;
  atlas->flags = flags;
  atlas->texture_format = texture_format;
  g_hook_list_init (&atlas->pre_reorganize_callbacks, sizeof (GHook));
  g_hook_list_init (&atlas->post_reorganize_callbacks, sizeof (GHook));

  return _cogl_atlas_object_new (atlas);
}

static void
_cogl_atlas_free (CoglAtlas *atlas)
{
  COGL_NOTE (ATLAS, "%p: Atlas destroyed", atlas);

  if (atlas->texture)
    cogl_object_unref (atlas->texture);
  if (atlas->map)
    _cogl_rectangle_map_free (atlas->map);

  g_hook_list_clear (&atlas->pre_reorganize_callbacks);
  g_hook_list_clear (&atlas->post_reorganize_callbacks);

  g_free (atlas);
}

typedef struct _CoglAtlasRepositionData
{
  /* The current user data for this texture */
  void *user_data;
  /* The old and new positions of the texture */
  CoglRectangleMapEntry old_position;
  CoglRectangleMapEntry new_position;
} CoglAtlasRepositionData;

static void
_cogl_atlas_migrate (CoglAtlas               *atlas,
                     unsigned int             n_textures,
                     CoglAtlasRepositionData *textures,
                     CoglTexture             *old_texture,
                     CoglTexture             *new_texture,
                     void                    *skip_user_data)
{
  unsigned int i;
  CoglBlitData blit_data;

  /* If the 'disable migrate' flag is set then we won't actually copy
     the textures to their new location. Instead we'll just invoke the
     callback to update the position */
  if ((atlas->flags & COGL_ATLAS_DISABLE_MIGRATION))
    for (i = 0; i < n_textures; i++)
      /* Update the texture position */
      atlas->update_position_cb (textures[i].user_data,
                                 new_texture,
                                 &textures[i].new_position);
  else
    {
      _cogl_blit_begin (&blit_data, new_texture, old_texture);

      for (i = 0; i < n_textures; i++)
        {
          /* Skip the texture that is being added because it doesn't contain
             any data yet */
          if (textures[i].user_data != skip_user_data)
            _cogl_blit (&blit_data,
                        textures[i].old_position.x,
                        textures[i].old_position.y,
                        textures[i].new_position.x,
                        textures[i].new_position.y,
                        textures[i].new_position.width,
                        textures[i].new_position.height);

          /* Update the texture position */
          atlas->update_position_cb (textures[i].user_data,
                                     new_texture,
                                     &textures[i].new_position);
        }

      _cogl_blit_end (&blit_data);
    }
}

typedef struct _CoglAtlasGetRectanglesData
{
  CoglAtlasRepositionData *textures;
  /* Number of textures found so far */
  unsigned int n_textures;
} CoglAtlasGetRectanglesData;

static void
_cogl_atlas_get_rectangles_cb (const CoglRectangleMapEntry *rectangle,
                               void                        *rect_data,
                               void                        *user_data)
{
  CoglAtlasGetRectanglesData *data = user_data;

  data->textures[data->n_textures].old_position = *rectangle;
  data->textures[data->n_textures++].user_data = rect_data;
}

static void
_cogl_atlas_get_next_size (unsigned int *map_width,
                           unsigned int *map_height)
{
  /* Double the size of the texture by increasing whichever dimension
     is smaller */
  if (*map_width < *map_height)
    *map_width <<= 1;
  else
    *map_height <<= 1;
}

static void
_cogl_atlas_get_initial_size (CoglPixelFormat format,
                              unsigned int *map_width,
                              unsigned int *map_height)
{
  unsigned int size;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  /* At least on Intel hardware, the texture size will be rounded up
     to at least 1MB so we might as well try to aim for that as an
     initial minimum size. If the format is only 1 byte per pixel we
     can use 1024x1024, otherwise we'll assume it will take 4 bytes
     per pixel and use 512x512. */
  if (_cogl_pixel_format_get_bytes_per_pixel (format) == 1)
    size = 1024;
  else
    size = 512;

  /* Some platforms might not support this large size so we'll
     decrease the size until it can */
  while (size > 1 &&
         !ctx->texture_driver->size_supported (ctx,
                                               GL_TEXTURE_2D,
                                               gl_intformat,
                                               gl_format,
                                               gl_type,
                                               size, size))
    size >>= 1;

  *map_width = size;
  *map_height = size;
}

static CoglRectangleMap *
_cogl_atlas_create_map (CoglPixelFormat          format,
                        unsigned int             map_width,
                        unsigned int             map_height,
                        unsigned int             n_textures,
                        CoglAtlasRepositionData *textures)
{
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  _COGL_GET_CONTEXT (ctx, NULL);

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  /* Keep trying increasingly larger atlases until we can fit all of
     the textures */
  while (ctx->texture_driver->size_supported (ctx,
                                              GL_TEXTURE_2D,
                                              gl_intformat,
                                              gl_format,
                                              gl_type,
                                              map_width, map_height))
    {
      CoglRectangleMap *new_atlas = _cogl_rectangle_map_new (map_width,
                                                             map_height,
                                                             NULL);
      unsigned int i;

      COGL_NOTE (ATLAS, "Trying to resize the atlas to %ux%u",
                 map_width, map_height);

      /* Add all of the textures and keep track of the new position */
      for (i = 0; i < n_textures; i++)
        if (!_cogl_rectangle_map_add (new_atlas,
                                      textures[i].old_position.width,
                                      textures[i].old_position.height,
                                      textures[i].user_data,
                                      &textures[i].new_position))
          break;

      /* If the atlas can contain all of the textures then we have a
         winner */
      if (i >= n_textures)
        return new_atlas;
      else
        COGL_NOTE (ATLAS, "Atlas size abandoned after trying "
                   "%u out of %u textures",
                   i, n_textures);

      _cogl_rectangle_map_free (new_atlas);
      _cogl_atlas_get_next_size (&map_width, &map_height);
    }

  /* If we get here then there's no atlas that can accommodate all of
     the rectangles */

  return NULL;
}

static CoglTexture2D *
_cogl_atlas_create_texture (CoglAtlas *atlas,
                            int width,
                            int height)
{
  CoglTexture2D *tex;
  CoglError *ignore_error = NULL;

  _COGL_GET_CONTEXT (ctx, NULL);

  if ((atlas->flags & COGL_ATLAS_CLEAR_TEXTURE))
    {
      uint8_t *clear_data;
      CoglBitmap *clear_bmp;
      int bpp = _cogl_pixel_format_get_bytes_per_pixel (atlas->texture_format);

      /* Create a buffer of zeroes to initially clear the texture */
      clear_data = g_malloc0 (width * height * bpp);
      clear_bmp = cogl_bitmap_new_for_data (ctx,
                                            width,
                                            height,
                                            atlas->texture_format,
                                            width * bpp,
                                            clear_data);

      tex = cogl_texture_2d_new_from_bitmap (clear_bmp);

      _cogl_texture_set_internal_format (COGL_TEXTURE (tex),
                                         atlas->texture_format);

      if (!cogl_texture_allocate (COGL_TEXTURE (tex), &ignore_error))
        {
          cogl_error_free (ignore_error);
          cogl_object_unref (tex);
          tex = NULL;
        }

      cogl_object_unref (clear_bmp);

      g_free (clear_data);
    }
  else
    {
      tex = cogl_texture_2d_new_with_size (ctx, width, height);

      _cogl_texture_set_internal_format (COGL_TEXTURE (tex),
                                         atlas->texture_format);

      if (!cogl_texture_allocate (COGL_TEXTURE (tex), &ignore_error))
        {
          cogl_error_free (ignore_error);
          cogl_object_unref (tex);
          tex = NULL;
        }
    }

  return tex;
}

static int
_cogl_atlas_compare_size_cb (const void *a,
                             const void *b)
{
  const CoglAtlasRepositionData *ta = a;
  const CoglAtlasRepositionData *tb = b;
  unsigned int a_size, b_size;

  a_size = ta->old_position.width * ta->old_position.height;
  b_size = tb->old_position.width * tb->old_position.height;

  return a_size < b_size ? 1 : a_size > b_size ? -1 : 0;
}

static void
_cogl_atlas_notify_pre_reorganize (CoglAtlas *atlas)
{
  g_hook_list_invoke (&atlas->pre_reorganize_callbacks, FALSE);
}

static void
_cogl_atlas_notify_post_reorganize (CoglAtlas *atlas)
{
  g_hook_list_invoke (&atlas->post_reorganize_callbacks, FALSE);
}

CoglBool
_cogl_atlas_reserve_space (CoglAtlas             *atlas,
                           unsigned int           width,
                           unsigned int           height,
                           void                  *user_data)
{
  CoglAtlasGetRectanglesData data;
  CoglRectangleMap *new_map;
  CoglTexture2D *new_tex;
  unsigned int map_width, map_height;
  CoglBool ret;
  CoglRectangleMapEntry new_position;

  /* Check if we can fit the rectangle into the existing map */
  if (atlas->map &&
      _cogl_rectangle_map_add (atlas->map, width, height,
                               user_data,
                               &new_position))
    {
      COGL_NOTE (ATLAS, "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
                 atlas,
                 _cogl_rectangle_map_get_width (atlas->map),
                 _cogl_rectangle_map_get_height (atlas->map),
                 _cogl_rectangle_map_get_n_rectangles (atlas->map),
                 /* waste as a percentage */
                 _cogl_rectangle_map_get_remaining_space (atlas->map) *
                 100 / (_cogl_rectangle_map_get_width (atlas->map) *
                        _cogl_rectangle_map_get_height (atlas->map)));

      atlas->update_position_cb (user_data,
                                 atlas->texture,
                                 &new_position);

      return TRUE;
    }

  /* If we make it here then we need to reorganize the atlas. First
     we'll notify any users of the atlas that this is going to happen
     so that for example in CoglAtlasTexture it can notify that the
     storage has changed and cause a flush */
  _cogl_atlas_notify_pre_reorganize (atlas);

  /* Get an array of all the textures currently in the atlas. */
  data.n_textures = 0;
  if (atlas->map == NULL)
    data.textures = g_malloc (sizeof (CoglAtlasRepositionData));
  else
    {
      unsigned int n_rectangles =
        _cogl_rectangle_map_get_n_rectangles (atlas->map);
      data.textures = g_malloc (sizeof (CoglAtlasRepositionData) *
                                (n_rectangles + 1));
      _cogl_rectangle_map_foreach (atlas->map,
                                   _cogl_atlas_get_rectangles_cb,
                                   &data);
    }

  /* Add the new rectangle as a dummy texture so that it can be
     positioned with the rest */
  data.textures[data.n_textures].old_position.x = 0;
  data.textures[data.n_textures].old_position.y = 0;
  data.textures[data.n_textures].old_position.width = width;
  data.textures[data.n_textures].old_position.height = height;
  data.textures[data.n_textures++].user_data = user_data;

  /* The atlasing algorithm works a lot better if the rectangles are
     added in decreasing order of size so we'll first sort the
     array */
  qsort (data.textures, data.n_textures,
         sizeof (CoglAtlasRepositionData),
         _cogl_atlas_compare_size_cb);

  /* Try to create a new atlas that can contain all of the textures */
  if (atlas->map)
    {
      map_width = _cogl_rectangle_map_get_width (atlas->map);
      map_height = _cogl_rectangle_map_get_height (atlas->map);

      /* If there is enough space in for the new rectangle in the
         existing atlas with at least 6% waste we'll start with the
         same size, otherwise we'll immediately double it */
      if ((map_width * map_height -
           _cogl_rectangle_map_get_remaining_space (atlas->map) +
           width * height) * 53 / 50 >
          map_width * map_height)
        _cogl_atlas_get_next_size (&map_width, &map_height);
    }
  else
    _cogl_atlas_get_initial_size (atlas->texture_format,
                                  &map_width, &map_height);

  new_map = _cogl_atlas_create_map (atlas->texture_format,
                                    map_width, map_height,
                                    data.n_textures, data.textures);

  /* If we can't create a map with the texture then give up */
  if (new_map == NULL)
    {
      COGL_NOTE (ATLAS, "%p: Could not fit texture in the atlas", atlas);
      ret = FALSE;
    }
  /* We need to migrate the existing textures into a new texture */
  else if ((new_tex = _cogl_atlas_create_texture
            (atlas,
             _cogl_rectangle_map_get_width (new_map),
             _cogl_rectangle_map_get_height (new_map))) == NULL)
    {
      COGL_NOTE (ATLAS, "%p: Could not create a CoglTexture2D", atlas);
      _cogl_rectangle_map_free (new_map);
      ret = FALSE;
    }
  else
    {
      int waste;

      COGL_NOTE (ATLAS,
                 "%p: Atlas %s with size %ix%i",
                 atlas,
                 atlas->map == NULL ||
                 _cogl_rectangle_map_get_width (atlas->map) !=
                 _cogl_rectangle_map_get_width (new_map) ||
                 _cogl_rectangle_map_get_height (atlas->map) !=
                 _cogl_rectangle_map_get_height (new_map) ?
                 "resized" : "reorganized",
                 _cogl_rectangle_map_get_width (new_map),
                 _cogl_rectangle_map_get_height (new_map));

      if (atlas->map)
        {
          /* Move all the textures to the right position in the new
             texture. This will also update the texture's rectangle */
          _cogl_atlas_migrate (atlas,
                               data.n_textures,
                               data.textures,
                               atlas->texture,
                               COGL_TEXTURE (new_tex),
                               user_data);
          _cogl_rectangle_map_free (atlas->map);
          cogl_object_unref (atlas->texture);
        }
      else
        /* We know there's only one texture so we can just directly
           update the rectangle from its new position */
        atlas->update_position_cb (data.textures[0].user_data,
                                   COGL_TEXTURE (new_tex),
                                   &data.textures[0].new_position);

      atlas->map = new_map;
      atlas->texture = COGL_TEXTURE (new_tex);

      waste = (_cogl_rectangle_map_get_remaining_space (atlas->map) *
               100 / (_cogl_rectangle_map_get_width (atlas->map) *
                      _cogl_rectangle_map_get_height (atlas->map)));

      COGL_NOTE (ATLAS, "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
                 atlas,
                 _cogl_rectangle_map_get_width (atlas->map),
                 _cogl_rectangle_map_get_height (atlas->map),
                 _cogl_rectangle_map_get_n_rectangles (atlas->map),
                 waste);

      ret = TRUE;
    }

  g_free (data.textures);

  _cogl_atlas_notify_post_reorganize (atlas);

  return ret;
}

void
_cogl_atlas_remove (CoglAtlas *atlas,
                    const CoglRectangleMapEntry *rectangle)
{
  _cogl_rectangle_map_remove (atlas->map, rectangle);

  COGL_NOTE (ATLAS, "%p: Removed rectangle sized %ix%i",
             atlas,
             rectangle->width,
             rectangle->height);
  COGL_NOTE (ATLAS, "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
             atlas,
             _cogl_rectangle_map_get_width (atlas->map),
             _cogl_rectangle_map_get_height (atlas->map),
             _cogl_rectangle_map_get_n_rectangles (atlas->map),
             _cogl_rectangle_map_get_remaining_space (atlas->map) *
             100 / (_cogl_rectangle_map_get_width (atlas->map) *
                    _cogl_rectangle_map_get_height (atlas->map)));
};

static CoglTexture *
create_migration_texture (CoglContext *ctx,
                          int width,
                          int height,
                          CoglPixelFormat internal_format)
{
  CoglTexture *tex;
  CoglError *skip_error = NULL;

  if ((_cogl_util_is_pot (width) && _cogl_util_is_pot (height)) ||
      (cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT_BASIC) &&
       cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP)))
    {
      /* First try creating a fast-path non-sliced texture */
      tex = COGL_TEXTURE (cogl_texture_2d_new_with_size (ctx,
                                                         width, height));

      _cogl_texture_set_internal_format (tex, internal_format);

      /* TODO: instead of allocating storage here it would be better
       * if we had some api that let us just check that the size is
       * supported by the hardware so storage could be allocated
       * lazily when uploading data. */
      if (!cogl_texture_allocate (tex, &skip_error))
        {
          cogl_error_free (skip_error);
          cogl_object_unref (tex);
          tex = NULL;
        }
    }
  else
    tex = NULL;

  if (!tex)
    {
      CoglTexture2DSliced *tex_2ds =
        cogl_texture_2d_sliced_new_with_size (ctx,
                                              width,
                                              height,
                                              COGL_TEXTURE_MAX_WASTE);

      _cogl_texture_set_internal_format (COGL_TEXTURE (tex_2ds),
                                         internal_format);

      tex = COGL_TEXTURE (tex_2ds);
    }

  return tex;
}

CoglTexture *
_cogl_atlas_copy_rectangle (CoglAtlas *atlas,
                            int x,
                            int y,
                            int width,
                            int height,
                            CoglPixelFormat internal_format)
{
  CoglTexture *tex;
  CoglBlitData blit_data;
  CoglError *ignore_error = NULL;

  _COGL_GET_CONTEXT (ctx, NULL);

  /* Create a new texture at the right size */
  tex = create_migration_texture (ctx, width, height, internal_format);
  if (!cogl_texture_allocate (tex, &ignore_error))
    {
      cogl_error_free (ignore_error);
      cogl_object_unref (tex);
      return NULL;
    }

  /* Blit the data out of the atlas to the new texture. If FBOs
     aren't available this will end up having to copy the entire
     atlas texture */
  _cogl_blit_begin (&blit_data, tex, atlas->texture);
  _cogl_blit (&blit_data,
                    x, y,
                    0, 0,
                    width, height);
  _cogl_blit_end (&blit_data);

  return tex;
}

void
_cogl_atlas_add_reorganize_callback (CoglAtlas            *atlas,
                                     GHookFunc             pre_callback,
                                     GHookFunc             post_callback,
                                     void                 *user_data)
{
  if (pre_callback)
    {
      GHook *hook = g_hook_alloc (&atlas->post_reorganize_callbacks);
      hook->func = pre_callback;
      hook->data = user_data;
      g_hook_prepend (&atlas->pre_reorganize_callbacks, hook);
    }
  if (post_callback)
    {
      GHook *hook = g_hook_alloc (&atlas->pre_reorganize_callbacks);
      hook->func = post_callback;
      hook->data = user_data;
      g_hook_prepend (&atlas->post_reorganize_callbacks, hook);
    }
}

void
_cogl_atlas_remove_reorganize_callback (CoglAtlas            *atlas,
                                        GHookFunc             pre_callback,
                                        GHookFunc             post_callback,
                                        void                 *user_data)
{
  if (pre_callback)
    {
      GHook *hook = g_hook_find_func_data (&atlas->pre_reorganize_callbacks,
                                           FALSE,
                                           pre_callback,
                                           user_data);
      if (hook)
        g_hook_destroy_link (&atlas->pre_reorganize_callbacks, hook);
    }
  if (post_callback)
    {
      GHook *hook = g_hook_find_func_data (&atlas->post_reorganize_callbacks,
                                           FALSE,
                                           post_callback,
                                           user_data);
      if (hook)
        g_hook_destroy_link (&atlas->post_reorganize_callbacks, hook);
    }
}
