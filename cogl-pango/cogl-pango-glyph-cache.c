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
#include "cogl/cogl-atlas.h"
#include "cogl/cogl-atlas-texture-private.h"

typedef struct _CoglPangoGlyphCacheKey     CoglPangoGlyphCacheKey;

struct _CoglPangoGlyphCache
{
  CoglContext *ctx;

  /* Hash table to quickly check whether a particular glyph in a
     particular font is already cached */
  GHashTable       *hash_table;

  /* List of CoglAtlases */
  GSList           *atlases;

  /* List of callbacks to invoke when an atlas is reorganized */
  GHookList         reorganize_callbacks;

  /* TRUE if we've ever stored a texture in the global atlas. This is
     used to make sure we only register one callback to listen for
     global atlas reorganizations */
  CoglBool          using_global_atlas;

  /* True if some of the glyphs are dirty. This is used as an
     optimization in _cogl_pango_glyph_cache_set_dirty_glyphs to avoid
     iterating the hash table if we know none of them are dirty */
  CoglBool          has_dirty_glyphs;

  /* Whether mipmapping is being used for this cache. This only
     affects whether we decide to put the glyph in the global atlas */
  CoglBool          use_mipmapping;
};

struct _CoglPangoGlyphCacheKey
{
  PangoFont  *font;
  PangoGlyph  glyph;
};

static void
cogl_pango_glyph_cache_value_free (CoglPangoGlyphCacheValue *value)
{
  if (value->texture)
    cogl_object_unref (value->texture);
  g_slice_free (CoglPangoGlyphCacheValue, value);
}

static void
cogl_pango_glyph_cache_key_free (CoglPangoGlyphCacheKey *key)
{
  g_object_unref (key->font);
  g_slice_free (CoglPangoGlyphCacheKey, key);
}

static unsigned int
cogl_pango_glyph_cache_hash_func (const void *key)
{
  const CoglPangoGlyphCacheKey *cache_key
    = (const CoglPangoGlyphCacheKey *) key;

  /* Generate a number affected by both the font and the glyph
     number. We can safely directly compare the pointers because the
     key holds a reference to the font so it is not possible that a
     different font will have the same memory address */
  return GPOINTER_TO_UINT (cache_key->font) ^ cache_key->glyph;
}

static CoglBool
cogl_pango_glyph_cache_equal_func (const void *a, const void *b)
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

CoglPangoGlyphCache *
cogl_pango_glyph_cache_new (CoglContext *ctx,
                            CoglBool use_mipmapping)
{
  CoglPangoGlyphCache *cache;

  cache = g_malloc (sizeof (CoglPangoGlyphCache));

  /* Note: as a rule we don't take references to a CoglContext
   * internally since */
  cache->ctx = ctx;

  cache->hash_table = g_hash_table_new_full
    (cogl_pango_glyph_cache_hash_func,
     cogl_pango_glyph_cache_equal_func,
     (GDestroyNotify) cogl_pango_glyph_cache_key_free,
     (GDestroyNotify) cogl_pango_glyph_cache_value_free);

  cache->atlases = NULL;
  g_hook_list_init (&cache->reorganize_callbacks, sizeof (GHook));

  cache->has_dirty_glyphs = FALSE;

  cache->using_global_atlas = FALSE;

  cache->use_mipmapping = use_mipmapping;

  return cache;
}

static void
cogl_pango_glyph_cache_reorganize_cb (void *user_data)
{
  CoglPangoGlyphCache *cache = user_data;

  g_hook_list_invoke (&cache->reorganize_callbacks, FALSE);
}

void
cogl_pango_glyph_cache_clear (CoglPangoGlyphCache *cache)
{
  g_slist_foreach (cache->atlases, (GFunc) cogl_object_unref, NULL);
  g_slist_free (cache->atlases);
  cache->atlases = NULL;
  cache->has_dirty_glyphs = FALSE;

  g_hash_table_remove_all (cache->hash_table);
}

void
cogl_pango_glyph_cache_free (CoglPangoGlyphCache *cache)
{
  if (cache->using_global_atlas)
    {
      _cogl_atlas_texture_remove_reorganize_callback (
                                  cache->ctx,
                                  cogl_pango_glyph_cache_reorganize_cb, cache);
    }

  cogl_pango_glyph_cache_clear (cache);

  g_hash_table_unref (cache->hash_table);

  g_hook_list_clear (&cache->reorganize_callbacks);

  g_free (cache);
}

static void
cogl_pango_glyph_cache_update_position_cb (void *user_data,
                                           CoglTexture *new_texture,
                                           const CoglRectangleMapEntry *rect)
{
  CoglPangoGlyphCacheValue *value = user_data;
  float tex_width, tex_height;

  if (value->texture)
    cogl_object_unref (value->texture);
  value->texture = cogl_object_ref (new_texture);

  tex_width = cogl_texture_get_width (new_texture);
  tex_height = cogl_texture_get_height (new_texture);

  value->tx1 = rect->x / tex_width;
  value->ty1 = rect->y / tex_height;
  value->tx2 = (rect->x + value->draw_width) / tex_width;
  value->ty2 = (rect->y + value->draw_height) / tex_height;

  value->tx_pixel = rect->x;
  value->ty_pixel = rect->y;

  /* The glyph has changed position so it will need to be redrawn */
  value->dirty = TRUE;
}

static CoglBool
cogl_pango_glyph_cache_add_to_global_atlas (CoglPangoGlyphCache *cache,
                                            PangoFont *font,
                                            PangoGlyph glyph,
                                            CoglPangoGlyphCacheValue *value)
{
  CoglAtlasTexture *texture;
  CoglError *ignore_error = NULL;

  if (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_SHARED_ATLAS))
    return FALSE;

  /* If the cache is using mipmapping then we can't use the global
     atlas because it would just get migrated back out */
  if (cache->use_mipmapping)
    return FALSE;

  texture = cogl_atlas_texture_new_with_size (cache->ctx,
                                              value->draw_width,
                                              value->draw_height);
  if (!cogl_texture_allocate (COGL_TEXTURE (texture), &ignore_error))
    {
      cogl_error_free (ignore_error);
      return FALSE;
    }

  value->texture = COGL_TEXTURE (texture);
  value->tx1 = 0;
  value->ty1 = 0;
  value->tx2 = 1;
  value->ty2 = 1;
  value->tx_pixel = 0;
  value->ty_pixel = 0;

  /* The first time we store a texture in the global atlas we'll
     register for notifications when the global atlas is reorganized
     so we can forward the notification on as a glyph
     reorganization */
  if (!cache->using_global_atlas)
    {
      _cogl_atlas_texture_add_reorganize_callback
        (cache->ctx,
         cogl_pango_glyph_cache_reorganize_cb, cache);
      cache->using_global_atlas = TRUE;
    }

  return TRUE;
}

static CoglBool
cogl_pango_glyph_cache_add_to_local_atlas (CoglPangoGlyphCache *cache,
                                           PangoFont *font,
                                           PangoGlyph glyph,
                                           CoglPangoGlyphCacheValue *value)
{
  CoglAtlas *atlas = NULL;
  GSList *l;

  /* Look for an atlas that can reserve the space */
  for (l = cache->atlases; l; l = l->next)
    if (_cogl_atlas_reserve_space (l->data,
                                   value->draw_width + 1,
                                   value->draw_height + 1,
                                   value))
      {
        atlas = l->data;
        break;
      }

  /* If we couldn't find one then start a new atlas */
  if (atlas == NULL)
    {
      atlas = _cogl_atlas_new (COGL_PIXEL_FORMAT_A_8,
                               COGL_ATLAS_CLEAR_TEXTURE |
                               COGL_ATLAS_DISABLE_MIGRATION,
                               cogl_pango_glyph_cache_update_position_cb);
      COGL_NOTE (ATLAS, "Created new atlas for glyphs: %p", atlas);
      /* If we still can't reserve space then something has gone
         seriously wrong so we'll just give up */
      if (!_cogl_atlas_reserve_space (atlas,
                                      value->draw_width + 1,
                                      value->draw_height + 1,
                                      value))
        {
          cogl_object_unref (atlas);
          return FALSE;
        }

      _cogl_atlas_add_reorganize_callback
        (atlas, cogl_pango_glyph_cache_reorganize_cb, NULL, cache);

      cache->atlases = g_slist_prepend (cache->atlases, atlas);
    }

  return TRUE;
}

CoglPangoGlyphCacheValue *
cogl_pango_glyph_cache_lookup (CoglPangoGlyphCache *cache,
                               CoglBool             create,
                               PangoFont           *font,
                               PangoGlyph           glyph)
{
  CoglPangoGlyphCacheKey lookup_key;
  CoglPangoGlyphCacheValue *value;

  lookup_key.font = font;
  lookup_key.glyph = glyph;

  value = g_hash_table_lookup (cache->hash_table, &lookup_key);

  if (create && value == NULL)
    {
      CoglPangoGlyphCacheKey *key;
      PangoRectangle ink_rect;

      value = g_slice_new (CoglPangoGlyphCacheValue);
      value->texture = NULL;

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;

      /* If the glyph is zero-sized then we don't need to reserve any
         space for it and we can just avoid painting anything */
      if (ink_rect.width < 1 || ink_rect.height < 1)
        value->dirty = FALSE;
      else
        {
          /* Try adding the glyph to the global atlas... */
          if (!cogl_pango_glyph_cache_add_to_global_atlas (cache,
                                                           font,
                                                           glyph,
                                                           value) &&
              /* If it fails try the local atlas */
              !cogl_pango_glyph_cache_add_to_local_atlas (cache,
                                                          font,
                                                          glyph,
                                                          value))
            {
              cogl_pango_glyph_cache_value_free (value);
              return NULL;
            }

          value->dirty = TRUE;
          cache->has_dirty_glyphs = TRUE;
        }

      key = g_slice_new (CoglPangoGlyphCacheKey);
      key->font = g_object_ref (font);
      key->glyph = glyph;

      g_hash_table_insert (cache->hash_table, key, value);
    }

  return value;
}

static void
_cogl_pango_glyph_cache_set_dirty_glyphs_cb (void *key_ptr,
                                             void *value_ptr,
                                             void *user_data)
{
  CoglPangoGlyphCacheKey *key = key_ptr;
  CoglPangoGlyphCacheValue *value = value_ptr;
  CoglPangoGlyphCacheDirtyFunc func = user_data;

  if (value->dirty)
    {
      func (key->font, key->glyph, value);

      value->dirty = FALSE;
    }
}

void
_cogl_pango_glyph_cache_set_dirty_glyphs (CoglPangoGlyphCache *cache,
                                          CoglPangoGlyphCacheDirtyFunc func)
{
  /* If we know that there are no dirty glyphs then we can shortcut
     out early */
  if (!cache->has_dirty_glyphs)
    return;

  g_hash_table_foreach (cache->hash_table,
                        _cogl_pango_glyph_cache_set_dirty_glyphs_cb,
                        func);

  cache->has_dirty_glyphs = FALSE;
}

void
_cogl_pango_glyph_cache_add_reorganize_callback (CoglPangoGlyphCache *cache,
                                                 GHookFunc func,
                                                 void *user_data)
{
  GHook *hook = g_hook_alloc (&cache->reorganize_callbacks);
  hook->func = func;
  hook->data = user_data;
  g_hook_prepend (&cache->reorganize_callbacks, hook);
}

void
_cogl_pango_glyph_cache_remove_reorganize_callback (CoglPangoGlyphCache *cache,
                                                    GHookFunc func,
                                                    void *user_data)
{
  GHook *hook = g_hook_find_func_data (&cache->reorganize_callbacks,
                                       FALSE,
                                       func,
                                       user_data);

  if (hook)
    g_hook_destroy_link (&cache->reorganize_callbacks, hook);
}
