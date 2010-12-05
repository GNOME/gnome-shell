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
#include "cogl-atlas.h"
#include "cogl-rectangle-map.h"
#include "cogl-context.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-driver.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-debug.h"

#include <stdlib.h>

#ifndef HAVE_COGL_GLES2

#define glGenFramebuffers                 ctx->drv.pf_glGenFramebuffers
#define glBindFramebuffer                 ctx->drv.pf_glBindFramebuffer
#define glFramebufferTexture2D            ctx->drv.pf_glFramebufferTexture2D
#define glCheckFramebufferStatus          ctx->drv.pf_glCheckFramebufferStatus
#define glDeleteFramebuffers              ctx->drv.pf_glDeleteFramebuffers

#endif /* HAVE_COGL_GLES2 */

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER          0x8D40
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING  0x8CA6
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0    0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

/* If we want to do mulitple blits from a texture (such as when
   reorganizing the atlas) then it's quicker to download all of the
   data once and upload multiple times from that. This struct is used
   to keep the image data for a series of blits */

typedef struct _CoglAtlasBlitData
{
  CoglHandle src_tex, dst_tex;

  /* If we're using an FBO to blit, then FBO will be non-zero and
     old_fbo will be the previous framebuffer binding */
  GLuint fbo, old_fbo;

  /* If we're not using an FBO then we g_malloc a buffer and copy the
     complete texture data in */
  unsigned char *image_data;
  CoglPixelFormat format;
  int bpp;
  unsigned int src_height, src_width;

  GLenum dst_gl_target;
} CoglAtlasBlitData;

static void
_cogl_atlas_blit_begin (CoglAtlasBlitData *data,
                        CoglHandle dst_tex,
                        CoglHandle src_tex)
{
  GLenum src_gl_target;
  GLuint src_gl_texture;
  GLuint dst_gl_texture;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  data->dst_tex = dst_tex;
  data->src_tex = src_tex;
  data->fbo = 0;

  /* If we can use an FBO then we don't need to download the data and
     we can tell GL to blit directly between the textures */
  if (cogl_features_available (COGL_FEATURE_OFFSCREEN) &&
      !cogl_texture_is_sliced (dst_tex) &&
      cogl_texture_get_gl_texture (src_tex, &src_gl_texture, &src_gl_target) &&
      cogl_texture_get_gl_texture (dst_tex, &dst_gl_texture,
                                   &data->dst_gl_target))
    {
      /* Ideally we would use the cogl-offscreen API here, but I'd
         rather avoid creating a stencil renderbuffer which you can't
         currently do */
      /* Preserve the previous framebuffer binding so we don't trample
         on cogl-offscreen */
      data->old_fbo = 0;
      GE( glGetIntegerv (GL_FRAMEBUFFER_BINDING, (GLint *) &data->old_fbo) );

      _cogl_texture_set_filters (src_tex, GL_NEAREST, GL_NEAREST);

      /* Create an FBO to read from the src texture */
      GE( glGenFramebuffers (1, &data->fbo) );
      GE( glBindFramebuffer (GL_FRAMEBUFFER, data->fbo) );
      GE( glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  src_gl_target, src_gl_texture, 0) );
      if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
          /* The FBO failed for whatever reason so we'll fallback to
             reading the texture data */
          GE( glBindFramebuffer (GL_FRAMEBUFFER, data->old_fbo) );
          GE( glDeleteFramebuffers (1, &data->fbo) );
          data->fbo = 0;
        }

      _cogl_bind_gl_texture_transient (data->dst_gl_target,
                                       dst_gl_texture,
                                       FALSE);
    }

  if (data->fbo)
    COGL_NOTE (ATLAS, "Blit set up using an FBO");
  else
    {
      /* We need to retrieve the entire texture data (there is no
         glGetTexSubImage2D) */

      data->format = cogl_texture_get_format (src_tex);
      data->bpp = _cogl_get_format_bpp (data->format);
      data->src_width = cogl_texture_get_width (src_tex);
      data->src_height = cogl_texture_get_height (src_tex);

      data->image_data = g_malloc (data->bpp * data->src_width *
                                   data->src_height);
      cogl_texture_get_data (src_tex, data->format,
                             data->src_width * data->bpp, data->image_data);
    }
}

static void
_cogl_atlas_blit (CoglAtlasBlitData *data,
                  unsigned int src_x,
                  unsigned int src_y,
                  unsigned int dst_x,
                  unsigned int dst_y,
                  unsigned int width,
                  unsigned int height)
{
  /* If we have an FBO then we can do a fast blit */
  if (data->fbo)
    GE( glCopyTexSubImage2D (data->dst_gl_target, 0, dst_x, dst_y, src_x, src_y,
                             width, height) );
  else
    cogl_texture_set_region (data->dst_tex,
                             src_x, src_y,
                             dst_x, dst_y,
                             width, height,
                             data->src_width, data->src_height,
                             data->format,
                             data->src_width * data->bpp,
                             data->image_data);
}

static void
_cogl_atlas_blit_end (CoglAtlasBlitData *data)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (data->fbo)
    {
      GE( glBindFramebuffer (GL_FRAMEBUFFER, data->old_fbo) );
      GE( glDeleteFramebuffers (1, &data->fbo) );
    }
  else
    g_free (data->image_data);
}

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
  _cogl_callback_list_init (&atlas->reorganize_callbacks);

  return atlas;
}

void
_cogl_atlas_free (CoglAtlas *atlas)
{
  if (atlas->texture)
    cogl_handle_unref (atlas->texture);
  if (atlas->map)
    _cogl_rectangle_map_free (atlas->map);

  _cogl_callback_list_destroy (&atlas->reorganize_callbacks);

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
                     CoglHandle               old_texture,
                     CoglHandle               new_texture,
                     void                    *skip_user_data)
{
  unsigned int i;
  CoglAtlasBlitData blit_data;

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
      _cogl_atlas_blit_begin (&blit_data, new_texture, old_texture);

      for (i = 0; i < n_textures; i++)
        {
          /* Skip the texture that is being added because it doesn't contain
             any data yet */
          if (textures[i].user_data != skip_user_data)
            _cogl_atlas_blit (&blit_data,
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

      _cogl_atlas_blit_end (&blit_data);
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
  GLenum gl_type;

  _cogl_pixel_format_to_gl (format,
                            &gl_intformat,
                            NULL, /* gl_format */
                            &gl_type);

  /* At least on Intel hardware, the texture size will be rounded up
     to at least 1MB so we might as well try to aim for that as an
     initial minimum size. If the format is only 1 byte per pixel we
     can use 1024x1024, otherwise we'll assume it will take 4 bytes
     per pixel and use 512x512. */
  if (_cogl_get_format_bpp (format) == 1)
    size = 1024;
  else
    size = 512;

  /* Some platforms might not support this large size so we'll
     decrease the size until it can */
  while (size > 1 &&
         !_cogl_texture_driver_size_supported (GL_TEXTURE_2D,
                                               gl_intformat,
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
  GLenum gl_type;

  _cogl_pixel_format_to_gl (format,
                            &gl_intformat,
                            NULL, /* gl_format */
                            &gl_type);

  /* Keep trying increasingly larger atlases until we can fit all of
     the textures */
  while (_cogl_texture_driver_size_supported (GL_TEXTURE_2D,
                                              gl_intformat,
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

static CoglHandle
_cogl_atlas_create_texture (CoglAtlas *atlas,
                            int width,
                            int height)
{
  CoglHandle tex;

  if ((atlas->flags & COGL_ATLAS_CLEAR_TEXTURE))
    {
      guint8 *clear_data;
      CoglBitmap *clear_bmp;
      int bpp = _cogl_get_format_bpp (atlas->texture_format);

      /* Create a buffer of zeroes to initially clear the texture */
      clear_data = g_malloc0 (width * height * bpp);
      clear_bmp = _cogl_bitmap_new_from_data (clear_data,
                                              atlas->texture_format,
                                              width,
                                              height,
                                              width * bpp,
                                              (CoglBitmapDestroyNotify) g_free,
                                              NULL);

      tex = _cogl_texture_2d_new_from_bitmap (clear_bmp, COGL_TEXTURE_NONE,
                                              atlas->texture_format);
      cogl_object_unref (clear_bmp);
    }
  else
    tex = _cogl_texture_2d_new_with_size (width, height, COGL_TEXTURE_NONE,
                                          atlas->texture_format);

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
_cogl_atlas_notify_reorganize (CoglAtlas *atlas)
{
  _cogl_callback_list_invoke (&atlas->reorganize_callbacks);
}

gboolean
_cogl_atlas_reserve_space (CoglAtlas             *atlas,
                           unsigned int           width,
                           unsigned int           height,
                           void                  *user_data)
{
  CoglAtlasGetRectanglesData data;
  CoglRectangleMap *new_map;
  CoglHandle new_tex;
  unsigned int map_width, map_height;
  gboolean ret;
  CoglRectangleMapEntry new_position;

  /* Check if we can fit the rectangle into the existing map */
  if (atlas->map &&
      _cogl_rectangle_map_add (atlas->map, width, height,
                               user_data,
                               &new_position))
    {
      int waste =
        _cogl_rectangle_map_get_remaining_space (atlas->map) *
        100 / (_cogl_rectangle_map_get_width (atlas->map) *
               _cogl_rectangle_map_get_height (atlas->map));
      COGL_NOTE (ATLAS, "Atlas is %ix%i, has %i textures and is %i%% waste",
                 _cogl_rectangle_map_get_width (atlas->map),
                 _cogl_rectangle_map_get_height (atlas->map),
                 _cogl_rectangle_map_get_n_rectangles (atlas->map),
                 waste);

      atlas->update_position_cb (user_data,
                                 atlas->texture,
                                 &new_position);

      return TRUE;
    }

  /* We need to reorganise the atlas so we'll get an array of all the
     textures currently in the atlas. */
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
      COGL_NOTE (ATLAS, "Could not fit texture in the atlas");
      ret = FALSE;
    }
  /* We need to migrate the existing textures into a new texture */
  else if ((new_tex = _cogl_atlas_create_texture
            (atlas,
             _cogl_rectangle_map_get_width (new_map),
             _cogl_rectangle_map_get_height (new_map))) == COGL_INVALID_HANDLE)
    {
      COGL_NOTE (ATLAS, "Could not create a CoglTexture2D");
      _cogl_rectangle_map_free (new_map);
      ret = FALSE;
    }
  else
    {
      int waste;

      _cogl_atlas_notify_reorganize (atlas);

      COGL_NOTE (ATLAS,
                 "Atlas %s with size %ix%i",
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
                               new_tex,
                               user_data);
          _cogl_rectangle_map_free (atlas->map);
          cogl_handle_unref (atlas->texture);
        }
      else
        /* We know there's only one texture so we can just directly
           update the rectangle from its new position */
        atlas->update_position_cb (data.textures[0].user_data,
                                   new_tex,
                                   &data.textures[0].new_position);

      atlas->map = new_map;
      atlas->texture = new_tex;

      waste = (_cogl_rectangle_map_get_remaining_space (atlas->map) *
               100 / (_cogl_rectangle_map_get_width (atlas->map) *
                      _cogl_rectangle_map_get_height (atlas->map)));

      COGL_NOTE (ATLAS, "Atlas is %ix%i, has %i textures and is %i%% waste",
                 _cogl_rectangle_map_get_width (atlas->map),
                 _cogl_rectangle_map_get_height (atlas->map),
                 _cogl_rectangle_map_get_n_rectangles (atlas->map),
                 waste);

      ret = TRUE;
    }

  g_free (data.textures);

  return ret;
}

void
_cogl_atlas_remove (CoglAtlas *atlas,
                    const CoglRectangleMapEntry *rectangle)
{
  _cogl_rectangle_map_remove (atlas->map, rectangle);

  COGL_NOTE (ATLAS, "Removed rectangle sized %ix%i",
             rectangle->width,
             rectangle->height);
  COGL_NOTE (ATLAS, "Atlas is %ix%i, has %i textures and is %i%% waste",
             _cogl_rectangle_map_get_width (atlas->map),
             _cogl_rectangle_map_get_height (atlas->map),
             _cogl_rectangle_map_get_n_rectangles (atlas->map),
             _cogl_rectangle_map_get_remaining_space (atlas->map) *
             100 / (_cogl_rectangle_map_get_width (atlas->map) *
                    _cogl_rectangle_map_get_height (atlas->map)));
};

CoglHandle
_cogl_atlas_copy_rectangle (CoglAtlas        *atlas,
                            unsigned int      x,
                            unsigned int      y,
                            unsigned int      width,
                            unsigned int      height,
                            CoglTextureFlags  flags,
                            CoglPixelFormat   format)
{
  CoglHandle tex;
  CoglAtlasBlitData blit_data;

  /* Create a new texture at the right size */
  tex = cogl_texture_new_with_size (width, height, flags, format);

  /* Blit the data out of the atlas to the new texture. If FBOs
     aren't available this will end up having to copy the entire
     atlas texture */
  _cogl_atlas_blit_begin (&blit_data, tex, atlas->texture);
  _cogl_atlas_blit (&blit_data,
                    x, y,
                    0, 0,
                    width, height);
  _cogl_atlas_blit_end (&blit_data);

  return tex;
}

void
_cogl_atlas_add_reorganize_callback (CoglAtlas            *atlas,
                                     CoglCallbackListFunc  callback,
                                     void                 *user_data)
{
  _cogl_callback_list_add (&atlas->reorganize_callbacks, callback, user_data);
}

void
_cogl_atlas_remove_reorganize_callback (CoglAtlas            *atlas,
                                        CoglCallbackListFunc  callback,
                                        void                 *user_data)
{
  _cogl_callback_list_remove (&atlas->reorganize_callbacks,
                              callback, user_data);
}
