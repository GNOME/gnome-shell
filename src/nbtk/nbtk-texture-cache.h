/*
 * nbtk-texture-cache.h: Cached textures object
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

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef _NBTK_TEXTURE_CACHE
#define _NBTK_TEXTURE_CACHE

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define NBTK_TYPE_TEXTURE_CACHE nbtk_texture_cache_get_type()

#define NBTK_TEXTURE_CACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  NBTK_TYPE_TEXTURE_CACHE, NbtkTextureCache))

#define NBTK_TEXTURE_CACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  NBTK_TYPE_TEXTURE_CACHE, NbtkTextureCacheClass))

#define NBTK_IS_TEXTURE_CACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  NBTK_TYPE_TEXTURE_CACHE))

#define NBTK_IS_TEXTURE_CACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  NBTK_TYPE_TEXTURE_CACHE))

#define NBTK_TEXTURE_CACHE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  NBTK_TYPE_TEXTURE_CACHE, NbtkTextureCacheClass))

/**
 * NbtkTextureCache:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
typedef struct {
    /*< private >*/
    GObject parent;
} NbtkTextureCache;

typedef struct {
  GObjectClass parent_class;

  void (* loaded)        (NbtkTextureCache *self,
                          const gchar      *path,
                          ClutterTexture   *texture);

  void (* error_loading) (NbtkTextureCache *self,
                          GError           *error);
} NbtkTextureCacheClass;

GType nbtk_texture_cache_get_type (void);

NbtkTextureCache* nbtk_texture_cache_get_default (void);
ClutterTexture*   nbtk_texture_cache_get_texture (NbtkTextureCache *self,
                                                  const gchar *path,
                                                  gboolean want_clone);
ClutterActor*     nbtk_texture_cache_get_actor (NbtkTextureCache *self,
                                                  const gchar *path);

gint              nbtk_texture_cache_get_size    (NbtkTextureCache *self);

void nbtk_texture_cache_load_cache(NbtkTextureCache *self, const char *filename);

G_END_DECLS

#endif /* _NBTK_TEXTURE_CACHE */
