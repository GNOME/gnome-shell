/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-texture-cache.h: Object for loading and caching images as textures
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2010, Maxim Ermilov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ST_TEXTURE_CACHE_H__
#define __ST_TEXTURE_CACHE_H__

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#include <gio/gio.h>
#include <clutter/clutter.h>

#include <st/st-types.h>
#include <st/st-theme-node.h>
#include <st/st-widget.h>

#define ST_TYPE_TEXTURE_CACHE                 (st_texture_cache_get_type ())
G_DECLARE_FINAL_TYPE (StTextureCache, st_texture_cache,
                      ST, TEXTURE_CACHE, GObject)

typedef struct _StTextureCachePrivate StTextureCachePrivate;

struct _StTextureCache
{
  GObject parent;

  StTextureCachePrivate *priv;
};

typedef enum {
  ST_TEXTURE_CACHE_POLICY_NONE,
  ST_TEXTURE_CACHE_POLICY_FOREVER
} StTextureCachePolicy;

StTextureCache* st_texture_cache_get_default (void);

ClutterActor *
st_texture_cache_load_sliced_image (StTextureCache *cache,
                                    GFile          *file,
                                    gint            grid_width,
                                    gint            grid_height,
                                    gint            paint_scale,
                                    gfloat          resource_scale,
                                    GFunc           load_callback,
                                    gpointer        user_data);

GIcon *st_texture_cache_bind_cairo_surface_property (StTextureCache    *cache,
                                                     GObject           *object,
                                                     const char *property_name);
GIcon *
st_texture_cache_load_cairo_surface_to_gicon (StTextureCache  *cache,
                                              cairo_surface_t *surface);

ClutterActor *st_texture_cache_load_gicon (StTextureCache *cache,
                                           StThemeNode    *theme_node,
                                           GIcon          *icon,
                                           gint            size,
                                           gint            paint_scale,
                                           gfloat          resource_scale);

ClutterActor *st_texture_cache_load_file_async (StTextureCache    *cache,
                                                GFile             *file,
                                                int                available_width,
                                                int                available_height,
                                                int                paint_scale,
                                                gfloat             resource_scale);

CoglTexture     *st_texture_cache_load_file_to_cogl_texture (StTextureCache *cache,
                                                             GFile          *file,
                                                             gint            paint_scale,
                                                             gfloat          resource_scale);

cairo_surface_t *st_texture_cache_load_file_to_cairo_surface (StTextureCache *cache,
                                                              GFile          *file,
                                                              gint            paint_scale,
                                                              gfloat          resource_scale);

/**
 * StTextureCacheLoader: (skip)
 * @cache: a #StTextureCache
 * @key: Unique identifier for this texture
 * @data: Callback user data
 * @error: A #GError
 *
 * See st_texture_cache_load().  Implementations should return a
 * texture handle for the given key, or set @error.
 *
 */
typedef CoglTexture * (*StTextureCacheLoader) (StTextureCache *cache, const char *key, void *data, GError **error);

CoglTexture * st_texture_cache_load (StTextureCache       *cache,
                                     const char           *key,
                                     StTextureCachePolicy  policy,
                                     StTextureCacheLoader  load,
                                     void                 *data,
                                     GError              **error);

gboolean st_texture_cache_rescan_icon_theme (StTextureCache *cache);

#endif /* __ST_TEXTURE_CACHE_H__ */
