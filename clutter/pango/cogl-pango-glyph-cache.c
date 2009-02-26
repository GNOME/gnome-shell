/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 * License along with this library. If not, see <http://www.gnu.org/licenses>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "cogl-pango-glyph-cache.h"
#include "cogl-pango-private.h"

/* Minimum width/height for each texture */
#define MIN_TEXTURE_SIZE  256
/* All glyph with heights within this margin from each other can be
   put in the same band */
#define BAND_HEIGHT_ROUND 4

typedef struct _CoglPangoGlyphCacheKey     CoglPangoGlyphCacheKey;
typedef struct _CoglPangoGlyphCacheTexture CoglPangoGlyphCacheTexture;
typedef struct _CoglPangoGlyphCacheBand    CoglPangoGlyphCacheBand;

struct _CoglPangoGlyphCache
{
  /* Hash table to quickly check whether a particular glyph in a
     particular font is already cached */
  GHashTable                    *hash_table;

  /* List of textures */
  CoglPangoGlyphCacheTexture *textures;

  /* List of horizontal bands of glyphs */
  CoglPangoGlyphCacheBand    *bands;

  /* If TRUE all of the textures will be created with automatic mipmap
     generation enabled */
  gboolean                       use_mipmapping;
};

struct _CoglPangoGlyphCacheKey
{
  PangoFont  *font;
  PangoGlyph  glyph;
};

/* Represents one texture that will be used to store glyphs. The
   texture is divided into horizontal bands which all contain glyphs
   of approximatly the same height */
struct _CoglPangoGlyphCacheTexture
{
  /* The width and height of the texture which should always be a
     power of two. This can vary so that glyphs larger than
     MIN_TEXTURE_SIZE can use a bigger texture */
  int        texture_size;

  /* The remaining vertical space not taken up by any bands */
  int        space_remaining;

  /* The actual texture */
  CoglHandle texture;

  CoglPangoGlyphCacheTexture *next;
};

/* Represents one horizontal band of a texture. Each band contains
   glyphs of a similar height */
struct _CoglPangoGlyphCacheBand
{
  /* The y position of the top of the band */
  int        top;

  /* The height of the band */
  int        height;

  /* The remaining horizontal space not taken up by any glyphs */
  int        space_remaining;

  /* The size of the texture. Needed to calculate texture
     coordinates */
  int        texture_size;

  /* The texture containing this band */
  CoglHandle texture;

  CoglPangoGlyphCacheBand *next;
};

static void
cogl_pango_glyph_cache_value_free (CoglPangoGlyphCacheValue *value)
{
  cogl_texture_unref (value->texture);
  g_slice_free (CoglPangoGlyphCacheValue, value);
}

static void
cogl_pango_glyph_cache_key_free (CoglPangoGlyphCacheKey *key)
{
  g_object_unref (key->font);
  g_slice_free (CoglPangoGlyphCacheKey, key);
}

static guint
cogl_pango_glyph_cache_hash_func (gconstpointer key)
{
  const CoglPangoGlyphCacheKey *cache_key
    = (const CoglPangoGlyphCacheKey *) key;

  /* Generate a number affected by both the font and the glyph
     number. We can safely directly compare the pointers because the
     key holds a reference to the font so it is not possible that a
     different font will have the same memory address */
  return GPOINTER_TO_UINT (cache_key->font) ^ cache_key->glyph;
}

static gboolean
cogl_pango_glyph_cache_equal_func (gconstpointer a,
				      gconstpointer b)
{
  const CoglPangoGlyphCacheKey *key_a
    = (const CoglPangoGlyphCacheKey *) a;
  const CoglPangoGlyphCacheKey *key_b
    = (const CoglPangoGlyphCacheKey *) b;

  /* We can safely directly compare the pointers for the fonts because
     the key holds a reference to the font so it is not possible that
     a different font will have the same memory address */
  return key_a->font == key_b->font
    && key_a->glyph == key_b->glyph;
}

static void
cogl_pango_glyph_cache_free_textures (CoglPangoGlyphCacheTexture *node)
{
  CoglPangoGlyphCacheTexture *next;

  while (node)
    {
      next = node->next;
      cogl_texture_unref (node->texture);
      g_slice_free (CoglPangoGlyphCacheTexture, node);
      node = next;
    }
}

static void
cogl_pango_glyph_cache_free_bands (CoglPangoGlyphCacheBand *node)
{
  CoglPangoGlyphCacheBand *next;

  while (node)
    {
      next = node->next;
      cogl_texture_unref (node->texture);
      g_slice_free (CoglPangoGlyphCacheBand, node);
      node = next;
    }
}

CoglPangoGlyphCache *
cogl_pango_glyph_cache_new (gboolean use_mipmapping)
{
  CoglPangoGlyphCache *cache;

  cache = g_malloc (sizeof (CoglPangoGlyphCache));

  cache->hash_table = g_hash_table_new_full
    (cogl_pango_glyph_cache_hash_func,
     cogl_pango_glyph_cache_equal_func,
     (GDestroyNotify) cogl_pango_glyph_cache_key_free,
     (GDestroyNotify) cogl_pango_glyph_cache_value_free);

  cache->textures = NULL;
  cache->bands = NULL;
  cache->use_mipmapping = use_mipmapping;

  return cache;
}

void
cogl_pango_glyph_cache_clear (CoglPangoGlyphCache *cache)
{
  cogl_pango_glyph_cache_free_textures (cache->textures);
  cache->textures = NULL;
  cogl_pango_glyph_cache_free_bands (cache->bands);
  cache->bands = NULL;

  g_hash_table_remove_all (cache->hash_table);
}

void
cogl_pango_glyph_cache_free (CoglPangoGlyphCache *cache)
{
  cogl_pango_glyph_cache_clear (cache);

  g_hash_table_unref (cache->hash_table);

  g_free (cache);
}

CoglPangoGlyphCacheValue *
cogl_pango_glyph_cache_lookup (CoglPangoGlyphCache *cache,
				  PangoFont              *font,
				  PangoGlyph              glyph)
{
  CoglPangoGlyphCacheKey key;

  key.font = font;
  key.glyph = glyph;

  return (CoglPangoGlyphCacheValue *)
    g_hash_table_lookup (cache->hash_table, &key);
}

CoglPangoGlyphCacheValue *
cogl_pango_glyph_cache_set (CoglPangoGlyphCache *cache,
			    PangoFont           *font,
			    PangoGlyph           glyph,
			    gconstpointer        pixels,
			    int                  width,
			    int                  height,
			    int                  stride,
			    int                  draw_x,
			    int                  draw_y)
{
  int                       band_height;
  CoglPangoGlyphCacheBand  *band;
  CoglPangoGlyphCacheKey   *key;
  CoglPangoGlyphCacheValue *value;

  /* Reserve an extra pixel gap around the glyph so that it can pull
     in blank pixels when linear filtering is enabled */
  width++;
  height++;

  /* Round the height up to the nearest multiple of
     BAND_HEIGHT_ROUND */
  band_height = (height + BAND_HEIGHT_ROUND - 1) & ~(BAND_HEIGHT_ROUND - 1);

  /* Look for a band with the same height and enough width available */
  for (band = cache->bands;
       band && (band->height != band_height || band->space_remaining < width);
       band = band->next);
  if (band == NULL)
    {
      CoglPangoGlyphCacheTexture *texture;

      /* Look for a texture with enough vertical space left for a band
	 with this height */
      for (texture = cache->textures;
	   texture && (texture->space_remaining < band_height
		       || texture->texture_size < width);
	   texture = texture->next);
      if (texture == NULL)
	{
          CoglTextureFlags flags = COGL_TEXTURE_NONE;
	  guchar *clear_data;

	  /* Allocate a new texture that is the nearest power of two
	     greater than the band height or the minimum size,
	     whichever is lower */
	  texture = g_slice_new (CoglPangoGlyphCacheTexture);

	  texture->texture_size = MIN_TEXTURE_SIZE;
	  while (texture->texture_size < band_height ||
                 texture->texture_size < width)
            {
	      texture->texture_size *= 2;
            }

	  /* Allocate an empty buffer to clear the texture */
	  clear_data =
            g_malloc0 (texture->texture_size * texture->texture_size);

          if (cache->use_mipmapping)
            flags |= COGL_TEXTURE_AUTO_MIPMAP;

	  texture->texture =
            cogl_texture_new_from_data (texture->texture_size,
                                        texture->texture_size,
                                        32, flags,
                                        COGL_PIXEL_FORMAT_A_8,
                                        COGL_PIXEL_FORMAT_A_8,
                                        texture->texture_size,
                                        clear_data);

	  g_free (clear_data);

	  texture->space_remaining = texture->texture_size;
	  texture->next = cache->textures;
	  cache->textures = texture;

	  if (cache->use_mipmapping)
	    cogl_texture_set_filters (texture->texture,
				      CGL_LINEAR_MIPMAP_LINEAR,
				      CGL_LINEAR);
	  else
	    cogl_texture_set_filters (texture->texture,
				      CGL_LINEAR,
				      CGL_LINEAR);
	}

      band = g_slice_new (CoglPangoGlyphCacheBand);
      band->top = texture->texture_size - texture->space_remaining;
      band->height = band_height;
      band->space_remaining = texture->texture_size;
      band->texture = cogl_texture_ref (texture->texture);
      band->texture_size = texture->texture_size;
      band->next = cache->bands;
      cache->bands = band;
      texture->space_remaining -= band_height;
    }

  band->space_remaining -= width;

  width--;
  height--;

  cogl_texture_set_region (band->texture,
			   0, 0,
			   band->space_remaining,
			   band->top,
			   width, height,
			   width, height,
			   COGL_PIXEL_FORMAT_A_8,
			   stride,
			   pixels);

  key = g_slice_new (CoglPangoGlyphCacheKey);
  key->font = g_object_ref (font);
  key->glyph = glyph;

  value = g_slice_new (CoglPangoGlyphCacheValue);
  value->texture = cogl_texture_ref (band->texture);
  value->tx1 = (float)(band->space_remaining)
             / band->texture_size;
  value->tx2 = (float)(band->space_remaining + width)
             / band->texture_size;
  value->ty1 = (float)(band->top)
             / band->texture_size;
  value->ty2 = (float)(band->top + height)
             / band->texture_size;
  value->draw_x = draw_x;
  value->draw_y = draw_y;
  value->draw_width = width;
  value->draw_height = height;

  g_hash_table_insert (cache->hash_table, key, value);

  return value;
}
