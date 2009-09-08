/*
 * st-widget.h: Base class for St actors
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

/**
 * SECTION:st-texture-cache
 * @short_description: A per-process store to cache textures
 *
 * #StTextureCache allows an application to re-use an previously loaded
 * textures.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

#include "st-texture-cache.h"
#include "st-marshal.h"
#include "st-private.h"
#include "st-subtexture.h"
G_DEFINE_TYPE (StTextureCache, st_texture_cache, G_TYPE_OBJECT)

#define TEXTURE_CACHE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), ST_TYPE_TEXTURE_CACHE, StTextureCachePrivate))

typedef struct _StTextureCachePrivate StTextureCachePrivate;

struct _StTextureCachePrivate
{
  GHashTable *cache;
};

typedef struct FinalizedClosure
{
  gchar          *path;
  StTextureCache *cache;
} FinalizedClosure;

enum
{
  PROP_0,
};

static StTextureCache* __cache_singleton = NULL;

/*
 * Convention: posX with a value of -1 indicates whole texture
 */
typedef struct StTextureCacheItem {
  char          filename[256];
  int           width, height;
  int           posX, posY;
  ClutterActor *ptr;
} StTextureCacheItem;

static StTextureCacheItem *
st_texture_cache_item_new (void)
{
  return g_slice_new0 (StTextureCacheItem);
}

static void
st_texture_cache_item_free (StTextureCacheItem *item)
{
  g_slice_free (StTextureCacheItem, item);
}

static void
st_texture_cache_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_texture_cache_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_texture_cache_dispose (GObject *object)
{
  if (G_OBJECT_CLASS (st_texture_cache_parent_class)->dispose)
    G_OBJECT_CLASS (st_texture_cache_parent_class)->dispose (object);
}

static void
st_texture_cache_finalize (GObject *object)
{
  StTextureCachePrivate *priv = TEXTURE_CACHE_PRIVATE(object);

  if (priv->cache)
    {
      g_hash_table_unref (priv->cache);
      priv->cache = NULL;
    }

  G_OBJECT_CLASS (st_texture_cache_parent_class)->finalize (object);
}

static void
st_texture_cache_class_init (StTextureCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (StTextureCachePrivate));

  object_class->get_property = st_texture_cache_get_property;
  object_class->set_property = st_texture_cache_set_property;
  object_class->dispose = st_texture_cache_dispose;
  object_class->finalize = st_texture_cache_finalize;

}

static void
st_texture_cache_init (StTextureCache *self)
{
  StTextureCachePrivate *priv = TEXTURE_CACHE_PRIVATE(self);

  priv->cache = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       NULL);

}

/**
 * st_texture_cache_get_default:
 *
 * Returns the default texture cache. This is owned by St and should not be
 * unreferenced or freed.
 *
 * Returns: a StTextureCache
 */
StTextureCache*
st_texture_cache_get_default (void)
{
  if (G_UNLIKELY (__cache_singleton == NULL))
    __cache_singleton = g_object_new (ST_TYPE_TEXTURE_CACHE, NULL);

  return __cache_singleton;
}

#if 0
static void
on_texure_finalized (gpointer data,
                     GObject *where_the_object_was)
{
  FinalizedClosure *closure = (FinalizedClosure *) data;
  StTextureCachePrivate *priv = TEXTURE_CACHE_PRIVATE(closure->cache);

  g_hash_table_remove (priv->cache, closure->path);

  g_free(closure->path);
  g_free(closure);
}
#endif

/**
 * st_texture_cache_get_size:
 * @self: A #StTextureCache
 *
 * Returns the number of items in the texture cache
 *
 * Returns: the current size of the cache
 */
gint
st_texture_cache_get_size (StTextureCache *self)
{
  StTextureCachePrivate *priv = TEXTURE_CACHE_PRIVATE(self);

  return g_hash_table_size (priv->cache);
}

static void
add_texture_to_cache (StTextureCache     *self,
                      const gchar        *path,
                      StTextureCacheItem *item)
{
  /*  FinalizedClosure        *closure; */
  StTextureCachePrivate *priv = TEXTURE_CACHE_PRIVATE(self);

  g_hash_table_insert (priv->cache, g_strdup (path), item);

#if 0
  /* Make sure we can remove from hash */
  closure = g_new0 (FinalizedClosure, 1);
  closure->path = g_strdup (path);
  closure->cache = self;

  g_object_weak_ref (G_OBJECT (res), on_texure_finalized, closure);
#endif
}

/* NOTE: you should unref the returned texture when not needed */

/**
 * st_texture_cache_get_texture:
 * @self: A #StTextureCache
 * @path: A path to a image file
 *
 * Create a new ClutterTexture with the specified image. Adds the image to the
 * cache if the image had not been previously loaded. Subsequent calls with
 * the same image path will return a new ClutterTexture with the previously
 * loaded image.
 *
 * Returns: a newly created ClutterTexture
 */
ClutterTexture*
st_texture_cache_get_texture (StTextureCache *self,
                              const gchar    *path)
{
  ClutterActor *texture;
  CoglHandle *handle;
  StTextureCachePrivate *priv;
  StTextureCacheItem *item;

  g_return_val_if_fail (ST_IS_TEXTURE_CACHE (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);


  priv = TEXTURE_CACHE_PRIVATE (self);

  item = g_hash_table_lookup (priv->cache, path);

  if (item && item->posX != -1)
    {
      GError *err = NULL;
      /*
       * We have a cache hit, but it's for a partial texture. The only
       * sane option is to read it from disk and just don't cache it
       * at all.
       */
      return CLUTTER_TEXTURE(clutter_texture_new_from_file(path, &err));
    }
  if (!item)
    {
      GError *err = NULL;

      item = st_texture_cache_item_new ();
      item->posX = -1;
      item->posY = -1;
      item->ptr = clutter_texture_new_from_file (path, &err);
      clutter_texture_get_base_size (CLUTTER_TEXTURE (item->ptr),
                                     &item->width, &item->height);

      if (!item->ptr)
        {
          if (err)
            {
              g_warning ("Error loading image: %s", err->message);
              g_error_free (err);
            }

          return NULL;
        }

      add_texture_to_cache (self, path, item);
    }

  texture = clutter_texture_new ();
  handle = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (item->ptr));
  clutter_texture_set_cogl_texture ((ClutterTexture*) texture, handle);

  return (ClutterTexture*) texture;
}


/**
 * st_texture_cache_get_actor:
 * @self: A #StTextureCache
 * @path: A path to a image file
 *
 * Create a new ClutterSubTexture with the specified image. Adds the image to the
 * cache if the image had not been previously loaded. Subsequent calls with
 * the same image path will return a new ClutterTexture with the previously
 * loaded image.
 *
 * Use this function if all you need is an actor for drawing.
 *
 * Returns: a newly created ClutterTexture
 */
ClutterActor*
st_texture_cache_get_actor (StTextureCache *self,
                            const gchar    *path)
{
  StTextureCachePrivate *priv;
  StTextureCacheItem *item;
  GError *err = NULL;

  g_return_val_if_fail (ST_IS_TEXTURE_CACHE (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  priv = TEXTURE_CACHE_PRIVATE (self);


  item = g_hash_table_lookup (priv->cache, path);

  if (item)
    {
      int posX = item->posX;
      int posY = item->posY;
      if (posX == -1)
        posX = 0;
      if (posY == -1)
        posY = 0;
      return st_subtexture_new (CLUTTER_TEXTURE (item->ptr), posX, posY,
                                item->width, item->height);
    }

  item = st_texture_cache_item_new ();
  item->posX = -1;
  item->posY = -1;
  item->ptr = clutter_texture_new_from_file (path, &err);
  clutter_texture_get_base_size (CLUTTER_TEXTURE (item->ptr),
                                 &item->width, &item->height);

  if (!item->ptr)
    {
      if (err)
        {
          g_warning ("Error loading image: %s", err->message);
          g_error_free (err);
        }

      return NULL;
    }

  add_texture_to_cache (self, path, item);

  return st_subtexture_new (CLUTTER_TEXTURE (item->ptr), 0, 0, item->width,
                            item->height);
}

void
st_texture_cache_load_cache (StTextureCache *self,
                             const gchar    *filename)
{
  FILE *file;
  StTextureCacheItem *element, head;
  int ret;
  ClutterActor *actor;
  GError *error = NULL;
  StTextureCachePrivate *priv;

  g_return_if_fail (ST_IS_TEXTURE_CACHE (self));
  g_return_if_fail (filename != NULL);

  priv = TEXTURE_CACHE_PRIVATE (self);

  file = fopen(filename, "rm");
  if (!file)
    return;

  ret = fread (&head, sizeof(StTextureCacheItem), 1, file);
  if (ret < 0)
    {
      fclose (file);
      return;
    }

  /* check if we already if this texture in the cache */
  if (g_hash_table_lookup (priv->cache, head.filename))
    {
      /* skip it, we're done */
      fclose (file);
      return;
    }

  actor = clutter_texture_new_from_file (head.filename, &error);

  if (error)
    {
      g_critical (G_STRLOC ": Error opening cache image file: %s",
                  error->message);
      g_clear_error (&error);
      fclose (file);
      return;
    }

  element = st_texture_cache_item_new ();
  element->posX = -1;
  element->posY = -1;
  element->ptr = actor;
  strncpy (element->filename, head.filename, 256);
  clutter_texture_get_base_size (CLUTTER_TEXTURE (element->ptr),
                                 &element->width, &element->height);
  g_hash_table_insert (priv->cache, element->filename, element);

  while (!feof (file))
    {
      element = st_texture_cache_item_new ();
      ret = fread (element, sizeof (StTextureCacheItem), 1, file);
      if (ret < 1)
        {
          /* end of file */
          st_texture_cache_item_free (element);
          break;
        }

      element->ptr = actor;

      if (g_hash_table_lookup (priv->cache, element->filename))
        {
          /* file is already in the cache.... */
          st_texture_cache_item_free (element);
        } else {
          g_hash_table_insert (priv->cache, element->filename, element);
        }
    }
}
