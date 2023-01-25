/* gtkiconcache.c
 * Copyright (C) 2004  Anders Carlsson <andersca@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "st-icon-cache.h"

#include <glib/gstdio.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>


#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#define MAJOR_VERSION 1
#define MINOR_VERSION 0

#define GET_UINT16(cache, offset) (GUINT16_FROM_BE (*(guint16 *)((cache) + (offset))))
#define GET_UINT32(cache, offset) (GUINT32_FROM_BE (*(guint32 *)((cache) + (offset))))


struct _StIconCache {
  GMappedFile *map;
  char *buffer;

  guint32 last_chain_offset;
};

static void
clear_icon_cache (gpointer data)
{
  StIconCache *cache = data;
  g_clear_pointer (&cache->map, g_mapped_file_unref);
}

StIconCache *
st_icon_cache_ref (StIconCache *cache)
{
  return g_atomic_rc_box_acquire (cache);
}

void
st_icon_cache_unref (StIconCache *cache)
{
  g_atomic_rc_box_release_full (cache, clear_icon_cache);
}

StIconCache *
st_icon_cache_new_for_path (const char *path)
{
  StIconCache *cache = NULL;
  GMappedFile *map;

  char *cache_filename;
  int fd = -1;
  GStatBuf st;
  GStatBuf path_st;

   /* Check if we have a cache file */
  cache_filename = g_build_filename (path, "icon-theme.cache", NULL);

  g_debug ("look for icon cache in %s", path);

  if (g_stat (path, &path_st) < 0)
    goto done;

  /* Open the file and map it into memory */
  fd = g_open (cache_filename, O_RDONLY|_O_BINARY, 0);

  if (fd < 0)
    goto done;

  if (fstat (fd, &st) < 0 || st.st_size < 4)
    goto done;

  /* Verify cache is uptodate */
  if (st.st_mtime < path_st.st_mtime)
    {
      g_debug ("icon cache outdated");
      goto done;
    }

  map = g_mapped_file_new (cache_filename, FALSE, NULL);

  if (!map)
    goto done;

  g_debug ("found icon cache for %s", path);

  cache = g_atomic_rc_box_new0 (StIconCache);
  cache->map = map;
  cache->buffer = g_mapped_file_get_contents (map);

 done:
  g_free (cache_filename);
  if (fd >= 0)
    close (fd);

  return cache;
}

StIconCache *
st_icon_cache_new (const char *data)
{
  StIconCache *cache;

  cache = g_atomic_rc_box_new0 (StIconCache);
  cache->buffer = (char *)data;

  return cache;
}

static int
get_directory_index (StIconCache *cache,
                     const char  *directory)
{
  guint32 dir_list_offset;
  int n_dirs;
  int i;

  dir_list_offset = GET_UINT32 (cache->buffer, 8);

  n_dirs = GET_UINT32 (cache->buffer, dir_list_offset);

  for (i = 0; i < n_dirs; i++)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, dir_list_offset + 4 + 4 * i);
      char *name = cache->buffer + name_offset;
      if (strcmp (name, directory) == 0)
        return i;
    }

  return -1;
}

int
st_icon_cache_get_directory_index (StIconCache *cache,
                                   const char *directory)
{
  return get_directory_index (cache, directory);
}

static guint
icon_name_hash (gconstpointer key)
{
  const signed char *p = key;
  guint32 h = *p;

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;

  return h;
}

static int
find_image_offset (StIconCache *cache,
                   const char  *icon_name,
                   int          directory_index)
{
  guint32 hash_offset;
  guint32 n_buckets;
  guint32 chain_offset;
  int hash;
  guint32 image_list_offset, n_images;
  int i;

  if (!icon_name)
    return 0;

  chain_offset = cache->last_chain_offset;
  if (chain_offset)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      char *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
        goto find_dir;
    }

  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);
  hash = icon_name_hash (icon_name) % n_buckets;

  chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * hash);
  while (chain_offset != 0xffffffff)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      char *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
        {
          cache->last_chain_offset = chain_offset;
          goto find_dir;
        }

      chain_offset = GET_UINT32 (cache->buffer, chain_offset);
    }

  cache->last_chain_offset = 0;
  return 0;

find_dir:
  /* We've found an icon list, now check if we have the right icon in it */
  image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
  n_images = GET_UINT32 (cache->buffer, image_list_offset);

  for (i = 0; i < n_images; i++)
    {
      if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * i) ==
          directory_index)
        return image_list_offset + 4 + 8 * i;
    }

  return 0;
}

int
st_icon_cache_get_icon_flags (StIconCache *cache,
                              const char  *icon_name,
                              int          directory_index)
{
  guint32 image_offset;

  image_offset = find_image_offset (cache, icon_name, directory_index);

  if (!image_offset)
    return 0;

  return GET_UINT16 (cache->buffer, image_offset + 2);
}

gboolean
st_icon_cache_has_icons (StIconCache *cache,
                         const char  *directory)
{
  int directory_index;
  guint32 hash_offset, n_buckets;
  guint32 chain_offset;
  guint32 image_list_offset, n_images;
  int i, j;

  directory_index = get_directory_index (cache, directory);

  if (directory_index == -1)
    return FALSE;

  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  for (i = 0; i < n_buckets; i++)
    {
      chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * i);
      while (chain_offset != 0xffffffff)
        {
          image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
          n_images = GET_UINT32 (cache->buffer, image_list_offset);

          for (j = 0; j < n_images; j++)
            {
              if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * j) ==
                  directory_index)
                return TRUE;
            }

          chain_offset = GET_UINT32 (cache->buffer, chain_offset);
        }
    }

  return FALSE;
}

void
st_icon_cache_add_icons (StIconCache *cache,
                         const char  *directory,
                         GHashTable   *hash_table)
{
  int directory_index;
  guint32 hash_offset, n_buckets;
  guint32 chain_offset;
  guint32 image_list_offset, n_images;
  int i, j;

  directory_index = get_directory_index (cache, directory);

  if (directory_index == -1)
    return;

  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  for (i = 0; i < n_buckets; i++)
    {
      chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * i);
      while (chain_offset != 0xffffffff)
        {
          guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
          char *name = cache->buffer + name_offset;

          image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
          n_images = GET_UINT32 (cache->buffer, image_list_offset);

          for (j = 0; j < n_images; j++)
            {
              if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * j) ==
                  directory_index)
                g_hash_table_insert (hash_table, name, NULL);
            }

          chain_offset = GET_UINT32 (cache->buffer, chain_offset);
        }
    }
}

gboolean
st_icon_cache_has_icon (StIconCache *cache,
                        const char  *icon_name)
{
  guint32 hash_offset;
  guint32 n_buckets;
  guint32 chain_offset;
  int hash;

  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  hash = icon_name_hash (icon_name) % n_buckets;

  chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * hash);
  while (chain_offset != 0xffffffff)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      char *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
        return TRUE;

      chain_offset = GET_UINT32 (cache->buffer, chain_offset);
    }

  return FALSE;
}

gboolean
st_icon_cache_has_icon_in_directory (StIconCache *cache,
                                     const char  *icon_name,
                                     const char  *directory)
{
  guint32 hash_offset;
  guint32 n_buckets;
  guint32 chain_offset;
  int hash;
  gboolean found_icon = FALSE;
  int directory_index;

  directory_index = get_directory_index (cache, directory);

  if (directory_index == -1)
    return FALSE;

  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  hash = icon_name_hash (icon_name) % n_buckets;

  chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * hash);
  while (chain_offset != 0xffffffff)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      char *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
        {
          found_icon = TRUE;
          break;
        }

      chain_offset = GET_UINT32 (cache->buffer, chain_offset);
    }

  if (found_icon)
    {
      guint32 image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
      guint32 n_images =  GET_UINT32 (cache->buffer, image_list_offset);
      guint32 image_offset = image_list_offset + 4;
      int i;
      for (i = 0; i < n_images; i++)
        {
          guint16 index = GET_UINT16 (cache->buffer, image_offset);

          if (index == directory_index)
            return TRUE;
          image_offset += 8;
        }
    }

  return FALSE;
}

static void
pixbuf_destroy_cb (guchar   *pixels,
                   gpointer  data)
{
  StIconCache *cache = data;

  st_icon_cache_unref (cache);
}

GdkPixbuf *
st_icon_cache_get_icon (StIconCache *cache,
                        const char  *icon_name,
                        int          directory_index)
{
  guint32 offset, image_data_offset, pixel_data_offset;
  guint32 length, type;
  GdkPixbuf *pixbuf;
  GdkPixdata pixdata;
  GError *error = NULL;

  offset = find_image_offset (cache, icon_name, directory_index);

  if (!offset)
    return NULL;

  image_data_offset = GET_UINT32 (cache->buffer, offset + 4);

  if (!image_data_offset)
    return NULL;

  pixel_data_offset = GET_UINT32 (cache->buffer, image_data_offset);

  type = GET_UINT32 (cache->buffer, pixel_data_offset);

  if (type != 0)
    {
      g_debug ("invalid pixel data type %u", type);
      return NULL;
    }

  length = GET_UINT32 (cache->buffer, pixel_data_offset + 4);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (!gdk_pixdata_deserialize (&pixdata, length,
                                (guchar *)(cache->buffer + pixel_data_offset + 8),
                                &error))
    {
      g_debug ("could not deserialize data: %s", error->message);
      g_error_free (error);

      return NULL;
    }
G_GNUC_END_IGNORE_DEPRECATIONS

  pixbuf = gdk_pixbuf_new_from_data (pixdata.pixel_data, GDK_COLORSPACE_RGB,
                                     (pixdata.pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA,
                                     8, pixdata.width, pixdata.height, pixdata.rowstride,
                                     (GdkPixbufDestroyNotify)pixbuf_destroy_cb,
                                     cache);
  if (!pixbuf)
    {
      g_debug ("could not convert pixdata to pixbuf: %s", error->message);
      g_error_free (error);

      return NULL;
    }

  st_icon_cache_ref (cache);

  return pixbuf;
}

