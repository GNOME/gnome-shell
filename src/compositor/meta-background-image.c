/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2014 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:meta-background-image
 * @title: MetaBackgroundImage
 * @short_description: objects holding images loaded from files, used for backgrounds
 */

#include <config.h>

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>
#include <meta/meta-background-image.h>
#include "cogl-utils.h"

enum
{
  LOADED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _MetaBackgroundImageCache
{
  GObject parent_instance;

  GHashTable *images;
};

struct _MetaBackgroundImageCacheClass
{
  GObjectClass parent_class;
};

struct _MetaBackgroundImage
{
  GObject parent_instance;
  GFile *file;
  MetaBackgroundImageCache *cache;
  gboolean in_cache;
  gboolean loaded;
  CoglTexture *texture;
};

struct _MetaBackgroundImageClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (MetaBackgroundImageCache, meta_background_image_cache, G_TYPE_OBJECT);

static void
meta_background_image_cache_init (MetaBackgroundImageCache *cache)
{
  cache->images = g_hash_table_new (g_file_hash, (GEqualFunc) g_file_equal);
}

static void
meta_background_image_cache_finalize (GObject *object)
{
  MetaBackgroundImageCache *cache = META_BACKGROUND_IMAGE_CACHE (object);
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, cache->images);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      MetaBackgroundImage *image = value;
      image->in_cache = FALSE;
    }

  g_hash_table_destroy (cache->images);

  G_OBJECT_CLASS (meta_background_image_cache_parent_class)->finalize (object);
}

static void
meta_background_image_cache_class_init (MetaBackgroundImageCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_background_image_cache_finalize;
}

/**
 * meta_background_image_cache_get_default:
 *
 * Return value: (transfer none): the global singleton background cache
 */
MetaBackgroundImageCache *
meta_background_image_cache_get_default (void)
{
  static MetaBackgroundImageCache *cache;

  if (cache == NULL)
    cache = g_object_new (META_TYPE_BACKGROUND_IMAGE_CACHE, NULL);

  return cache;
}

static void
load_file (GTask               *task,
           MetaBackgroundImage *image,
           gpointer             task_data,
           GCancellable        *cancellable)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  GFileInputStream *stream;

  stream = g_file_read (image->file, NULL, &error);
  if (stream == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  pixbuf = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (stream), NULL, &error);
  g_object_unref (stream);

  if (pixbuf == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, pixbuf, (GDestroyNotify) g_object_unref);
}

static void
file_loaded (GObject      *source_object,
             GAsyncResult *result,
             gpointer      user_data)
{
  MetaBackgroundImage *image = META_BACKGROUND_IMAGE (source_object);
  GError *error = NULL;
  CoglError *catch_error = NULL;
  GTask *task;
  CoglTexture *texture;
  GdkPixbuf *pixbuf, *rotated;
  int width, height, row_stride;
  guchar *pixels;
  gboolean has_alpha;

  task = G_TASK (result);
  pixbuf = g_task_propagate_pointer (task, &error);

  if (pixbuf == NULL)
    {
      char *uri = g_file_get_uri (image->file);
      g_warning ("Failed to load background '%s': %s",
                 uri, error->message);
      g_clear_error (&error);
      g_free (uri);
      goto out;
    }

  rotated = gdk_pixbuf_apply_embedded_orientation (pixbuf);
  if (rotated != NULL)
    {
      g_object_unref (pixbuf);
      pixbuf = rotated;
    }

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  row_stride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  texture = meta_create_texture (width, height,
                                 has_alpha ? COGL_TEXTURE_COMPONENTS_RGBA : COGL_TEXTURE_COMPONENTS_RGB,
                                 META_TEXTURE_ALLOW_SLICING);

  if (!cogl_texture_set_data (texture,
                              has_alpha ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888,
                              row_stride,
                              pixels, 0,
                              &catch_error))
    {
      g_warning ("Failed to create texture for background");
      cogl_error_free (catch_error);
      cogl_object_unref (texture);
    }

  image->texture = texture;

out:
  if (pixbuf != NULL)
    g_object_unref (pixbuf);

  image->loaded = TRUE;
  g_signal_emit (image, signals[LOADED], 0);
}

/**
 * meta_background_image_cache_load:
 * @cache: a #MetaBackgroundImageCache
 * @file: #GFile to load
 *
 * Loads an image to use as a background, or returns a reference to an
 * image that is already in the process of loading or loaded. In either
 * case, what is returned is a #MetaBackgroundImage which can be derefenced
 * to get a #CoglTexture. If meta_background_image_is_loaded() returns %TRUE,
 * the background is loaded, otherwise the MetaBackgroundImage::loaded
 * signal will be emitted exactly once. The 'loaded' state means that the
 * loading process finished, whether it succeeded or failed.
 *
 * Return value: (transfer full): a #MetaBackgroundImage to dereference to get the loaded texture
 */
MetaBackgroundImage *
meta_background_image_cache_load (MetaBackgroundImageCache *cache,
                                  GFile                    *file)
{
  MetaBackgroundImage *image;
  GTask *task;

  g_return_val_if_fail (META_IS_BACKGROUND_IMAGE_CACHE (cache), NULL);
  g_return_val_if_fail (file != NULL, NULL);

  image = g_hash_table_lookup (cache->images, file);
  if (image != NULL)
    return g_object_ref (image);

  image = g_object_new (META_TYPE_BACKGROUND_IMAGE, NULL);
  image->cache = cache;
  image->in_cache = TRUE;
  image->file = g_object_ref (file);
  g_hash_table_insert (cache->images, image->file, image);

  task = g_task_new (image, NULL, file_loaded, NULL);

  g_task_run_in_thread (task, (GTaskThreadFunc) load_file);
  g_object_unref (task);

  return image;
}

/**
 * meta_background_image_cache_purge:
 * @cache: a #MetaBackgroundImageCache
 * @file: file to remove from the cache
 *
 * Remove an entry from the cache; this would be used if monitoring
 * showed that the file changed.
 */
void
meta_background_image_cache_purge (MetaBackgroundImageCache *cache,
                                   GFile                    *file)
{
  MetaBackgroundImage *image;

  g_return_if_fail (META_IS_BACKGROUND_IMAGE_CACHE (cache));
  g_return_if_fail (file != NULL);

  image = g_hash_table_lookup (cache->images, file);
  if (image == NULL)
    return;

  g_hash_table_remove (cache->images, image->file);
  image->in_cache = FALSE;
}

G_DEFINE_TYPE (MetaBackgroundImage, meta_background_image, G_TYPE_OBJECT);

static void
meta_background_image_init (MetaBackgroundImage *image)
{
}

static void
meta_background_image_finalize (GObject *object)
{
  MetaBackgroundImage *image = META_BACKGROUND_IMAGE (object);

  if (image->in_cache)
    g_hash_table_remove (image->cache->images, image->file);

  if (image->texture)
    cogl_object_unref (image->texture);
  if (image->file)
    g_object_unref (image->file);

  G_OBJECT_CLASS (meta_background_image_parent_class)->finalize (object);
}

static void
meta_background_image_class_init (MetaBackgroundImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_background_image_finalize;

  signals[LOADED] =
    g_signal_new ("loaded",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

/**
 * meta_background_image_is_loaded:
 * @image: a #MetaBackgroundImage
 *
 * Return value: %TRUE if loading has already completed, %FALSE otherwise
 */
gboolean
meta_background_image_is_loaded (MetaBackgroundImage *image)
{
  g_return_val_if_fail (META_IS_BACKGROUND_IMAGE (image), FALSE);

  return image->loaded;
}

/**
 * meta_background_image_get_success:
 * @image: a #MetaBackgroundImage
 *
 * This function is a convenience function for checking for success,
 * without having to call meta_background_image_get_texture() and
 * handle the return of a Cogl type.
 *
 * Return value: %TRUE if loading completed successfully, otherwise %FALSE
 */
gboolean
meta_background_image_get_success (MetaBackgroundImage *image)
{
  g_return_val_if_fail (META_IS_BACKGROUND_IMAGE (image), FALSE);

  return image->texture != NULL;
}

/**
 * meta_background_image_get_texture:
 * @image: a #MetaBackgroundImage
 *
 * Return value: (transfer none): a #CoglTexture if loading succeeded; if
 *  loading failed or has not yet finished, %NULL.
 */
CoglTexture *
meta_background_image_get_texture (MetaBackgroundImage *image)
{
  g_return_val_if_fail (META_IS_BACKGROUND_IMAGE (image), NULL);

  return image->texture;
}
