/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-texture-cache.h: Cached textures object
 *
 * Copyright 2007 OpenedHand
 * Copyright 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef _ST_TEXTURE_CACHE
#define _ST_TEXTURE_CACHE

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_TEXTURE_CACHE st_texture_cache_get_type()

#define ST_TEXTURE_CACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  ST_TYPE_TEXTURE_CACHE, StTextureCache))

#define ST_TEXTURE_CACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  ST_TYPE_TEXTURE_CACHE, StTextureCacheClass))

#define ST_IS_TEXTURE_CACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  ST_TYPE_TEXTURE_CACHE))

#define ST_IS_TEXTURE_CACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  ST_TYPE_TEXTURE_CACHE))

#define ST_TEXTURE_CACHE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  ST_TYPE_TEXTURE_CACHE, StTextureCacheClass))

/**
 * StTextureCache:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
typedef struct {
    /*< private >*/
    GObject parent;
} StTextureCache;

typedef struct {
  GObjectClass parent_class;

  void (* loaded)        (StTextureCache *self,
                          const gchar      *path,
                          ClutterTexture   *texture);

  void (* error_loading) (StTextureCache *self,
                          GError           *error);
} StTextureCacheClass;

GType st_texture_cache_get_type (void);

StTextureCache* st_texture_cache_get_default (void);

ClutterTexture* st_texture_cache_get_texture (StTextureCache *self,
                                              const gchar    *path);
ClutterActor*   st_texture_cache_get_actor   (StTextureCache *self,
                                              const gchar    *path);

gint            st_texture_cache_get_size    (StTextureCache *self);

void st_texture_cache_load_cache (StTextureCache *self,
                                  const char     *filename);

G_END_DECLS

#endif /* _ST_TEXTURE_CACHE */
