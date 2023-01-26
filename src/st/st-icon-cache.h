/* gtkiconcache.h
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
#ifndef __ST_ICON_CACHE_H__
#define __ST_ICON_CACHE_H__

#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _StIconCache StIconCache;

StIconCache *st_icon_cache_new (const char  *data);
StIconCache *st_icon_cache_new_for_path (const char  *path);
int st_icon_cache_get_directory_index (StIconCache *cache,
                                       const char  *directory);
gboolean st_icon_cache_has_icon (StIconCache *cache,
                                 const char  *icon_name);
gboolean st_icon_cache_has_icon_in_directory (StIconCache *cache,
                                              const char  *icon_name,
                                              const char  *directory);
gboolean st_icon_cache_has_icons (StIconCache *cache,
                                  const char  *directory);
void st_icon_cache_add_icons (StIconCache *cache,
                              const char  *directory,
                              GHashTable  *hash_table);

int st_icon_cache_get_icon_flags (StIconCache *cache,
                                  const char  *icon_name,
                                  int          directory_index);
GdkPixbuf * st_icon_cache_get_icon (StIconCache *cache,
                                    const char  *icon_name,
                                    int          directory_index);

StIconCache *st_icon_cache_ref (StIconCache *cache);
void st_icon_cache_unref (StIconCache *cache);

#endif /* __ST_ICON_CACHE_H__ */
