/* Metacity Theme Rendering */

/* 
 * Copyright (C) 2001 Havoc Pennington
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "theme.h"
#include "util.h"
#include <string.h>

/* fill_gradient routine from GNOME background-properties, CVS says
 * Michael Fulbright checked it in, Copyright 1998 Red Hat Inc.
 */
static void
fill_gradient (GdkPixbuf       *pixbuf,
               const GdkColor  *c1,
	       const GdkColor  *c2,
               int              vertical,
               int              gradient_width,
               int              gradient_height,
               int              pixbuf_x,
               int              pixbuf_y)
{
  int i, j;
  int dr, dg, db;
  int gs1;
  int vc = (!vertical || (c1 == c2));
  int w = gdk_pixbuf_get_width (pixbuf);
  int h = gdk_pixbuf_get_height (pixbuf);
  guchar *b, *row;
  guchar *d = gdk_pixbuf_get_pixels (pixbuf);
  int rowstride = gdk_pixbuf_get_rowstride (pixbuf);

#define R1 c1->red
#define G1 c1->green
#define B1 c1->blue
#define R2 c2->red
#define G2 c2->green
#define B2 c2->blue

  dr = R2 - R1;
  dg = G2 - G1;
  db = B2 - B1;

  gs1 = (vertical) ? gradient_height - 1 : gradient_width - 1;

  row = g_new (unsigned char, rowstride);

  if (vc)
    {
      b = row;
      for (j = 0; j < w; j++)
        {
          *b++ = (R1 + ((j + pixbuf_x) * dr) / gs1) >> 8;
          *b++ = (G1 + ((j + pixbuf_x) * dg) / gs1) >> 8;
          *b++ = (B1 + ((j + pixbuf_x) * db) / gs1) >> 8;
        }
    }
  
  for (i = 0; i < h; i++)
    {
      if (!vc)
        {
          unsigned char cr, cg, cb;
          cr = (R1 + ((i + pixbuf_y) * dr) / gs1) >> 8;
          cg = (G1 + ((i + pixbuf_y) * dg) / gs1) >> 8;
          cb = (B1 + ((i + pixbuf_y) * db) / gs1) >> 8;
          b = row;
          for (j = 0; j < w; j++)
            {
              *b++ = cr;
              *b++ = cg;
              *b++ = cb;
            }
        }
      memcpy (d, row, w * 3);
      d += rowstride;
    }
  
#undef R1
#undef G1
#undef B1
#undef R2
#undef G2
#undef B2

  g_free (row);
}

typedef struct _CachedGradient CachedGradient;

struct _CachedGradient
{
  MetaGradientType  type;
  GdkColor          color_one;
  GdkColor          color_two;
  int               width;
  int               height;
  GdkPixbuf        *pixbuf;
  int               access_serial;
};

static GHashTable *gradient_cache = NULL;
static int access_counter = 0;
static int cache_size = 0;

#define GRADIENT_SIZE(g) ((g)->width * (g)->height * 4)

#define MAX_CACHE_SIZE (1024 * 128) /* 128k */

static guint
cached_gradient_hash (gconstpointer value)
{
  /* I have no idea how to write a hash function. */
  const CachedGradient *gradient = value;
  guint colorone_hash = gdk_color_hash (&gradient->color_one);
  guint colortwo_hash = gdk_color_hash (&gradient->color_two);
  guint hash = (colorone_hash >> 16) | (colortwo_hash << 16);

  hash ^= gradient->width << 22;
  hash ^= gradient->height;
  hash ^= gradient->type << 15;

  return hash;
}

static gboolean
cached_gradient_equal (gconstpointer value_a,
                       gconstpointer value_b)
{
  const CachedGradient *gradient_a = value_a;
  const CachedGradient *gradient_b = value_b;
  
  return gradient_a->type == gradient_b->type &&
    gradient_a->width == gradient_b->width &&
    gradient_a->height == gradient_b->height &&
    gdk_color_equal (&gradient_a->color_one, &gradient_b->color_one) &&
    gdk_color_equal (&gradient_a->color_two, &gradient_b->color_two);
}

static void
hash_listify (gpointer key, gpointer value, gpointer data)
{
  GSList **list = data;

  if (key != value)
    meta_bug ("Gradient cache got munged (value was overwritten)\n");
  
  *list = g_slist_prepend (*list, value);
}

/* sort gradients so that least-recently-used are first */
static int
gradient_lru_compare (gconstpointer a,
                      gconstpointer b)
{
  const CachedGradient *gradient_a = a;
  const CachedGradient *gradient_b = b;

  if (gradient_a->access_serial < gradient_b->access_serial)
    return -1;
  else if (gradient_a->access_serial > gradient_b->access_serial)
    return 1;
  else
    return 0;
}

static void
expire_some_old_gradients (void)
{
  GSList *all_gradients;
  GSList *tmp;
  
  all_gradients = NULL;

  g_hash_table_foreach (gradient_cache, hash_listify, &all_gradients);

  all_gradients = g_slist_sort (all_gradients, gradient_lru_compare);

  tmp = all_gradients;
  while (tmp != NULL)
    {
      CachedGradient *gradient = tmp->data;
      
      if (cache_size < MAX_CACHE_SIZE)
        break;

      meta_topic (META_DEBUG_GRADIENT_CACHE,
                  " Removing gradient of size %d from cache of size %d\n",
                  GRADIENT_SIZE (gradient), cache_size);
                  
      cache_size -= GRADIENT_SIZE (gradient);

      g_hash_table_remove (gradient_cache, gradient);

      g_object_unref (G_OBJECT (gradient->pixbuf));
      g_free (gradient);
      
      tmp = tmp->next;
    }

  g_slist_free (all_gradients);

  meta_topic (META_DEBUG_GRADIENT_CACHE,
              "Cache reduced to size %d bytes %d gradients after expiring old gradients\n",
              cache_size, g_hash_table_size (gradient_cache));
}

GdkPixbuf*
meta_theme_get_gradient (MetaGradientType  type,
                         const GdkColor   *color_one,
                         const GdkColor   *color_two,
                         int               width,
                         int               height)
{
  CachedGradient gradient;
  CachedGradient *cached;
  GdkPixbuf *retval;
  
  meta_topic (META_DEBUG_GRADIENT_CACHE,
              "Requesting %s gradient one %d/%d/%d two %d/%d/%d "
              "%d x %d\n",
              type == META_GRADIENT_VERTICAL ? "vertical" : "horizontal",
              color_one->red / 255, color_one->green / 255, color_one->blue / 255,
              color_two->red / 255, color_two->green / 255, color_two->blue / 255,
              width, height);
  
  if (gradient_cache == NULL)
    {
      gradient_cache = g_hash_table_new (cached_gradient_hash,
                                         cached_gradient_equal);
    }

  gradient.type = type;
  gradient.color_one = *color_one;
  gradient.color_two = *color_two;
  gradient.width = width;
  gradient.height = height;
  gradient.pixbuf = NULL;
  gradient.access_serial = access_counter;
  
  cached = g_hash_table_lookup (gradient_cache, &gradient);

  if (cached)
    {
      meta_topic (META_DEBUG_GRADIENT_CACHE,
                  "Found gradient in cache\n");
      ++access_counter;
      cached->access_serial = access_counter;
      g_object_ref (G_OBJECT (cached->pixbuf));
      return cached->pixbuf;
    }

  gradient.pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
                                    gradient.width, gradient.height);

  fill_gradient (gradient.pixbuf,
                 &gradient.color_one,
                 &gradient.color_two,
                 type == META_GRADIENT_VERTICAL ? TRUE : FALSE,
                 gradient.width,
                 gradient.height,
                 0, 0);

  cached = g_new (CachedGradient, 1);
  *cached = gradient;
  
  g_hash_table_insert (gradient_cache, cached, cached);

  meta_topic (META_DEBUG_GRADIENT_CACHE,
              "Caching newly-created gradient, size is %d bytes, total cache size %d bytes %d gradients, maximum %d bytes\n",
              GRADIENT_SIZE (cached),
              cache_size, g_hash_table_size (gradient_cache), MAX_CACHE_SIZE);
  
  cache_size += GRADIENT_SIZE (cached);

  g_object_ref (G_OBJECT (cached->pixbuf)); /* to return to caller */
  retval = cached->pixbuf;
  
  if (cache_size > MAX_CACHE_SIZE)
    expire_some_old_gradients (); /* may unref "cached->pixbuf" and free "cached" */
  
  return retval;
}

