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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __COGL_PANGO_GLYPH_CACHE_H__
#define __COGL_PANGO_GLYPH_CACHE_H__

#include <glib.h>
#include <pango/pango-font.h>

#include "cogl/cogl-texture.h"

COGL_BEGIN_DECLS

typedef struct _CoglPangoGlyphCache      CoglPangoGlyphCache;
typedef struct _CoglPangoGlyphCacheValue CoglPangoGlyphCacheValue;

struct _CoglPangoGlyphCacheValue
{
  CoglTexture *texture;

  float tx1;
  float ty1;
  float tx2;
  float ty2;

  int tx_pixel;
  int ty_pixel;

  int draw_x;
  int draw_y;
  int draw_width;
  int draw_height;

  /* This will be set to TRUE when the glyph atlas is reorganized
     which means the glyph will need to be redrawn */
  CoglBool   dirty;
};

typedef void (* CoglPangoGlyphCacheDirtyFunc) (PangoFont *font,
                                               PangoGlyph glyph,
                                               CoglPangoGlyphCacheValue *value);

CoglPangoGlyphCache *
cogl_pango_glyph_cache_new (CoglContext *ctx,
                            CoglBool use_mipmapping);

void
cogl_pango_glyph_cache_free (CoglPangoGlyphCache *cache);

CoglPangoGlyphCacheValue *
cogl_pango_glyph_cache_lookup (CoglPangoGlyphCache *cache,
                               CoglBool             create,
                               PangoFont           *font,
                               PangoGlyph           glyph);

void
cogl_pango_glyph_cache_clear (CoglPangoGlyphCache *cache);

void
_cogl_pango_glyph_cache_add_reorganize_callback (CoglPangoGlyphCache *cache,
                                                 GHookFunc func,
                                                 void *user_data);

void
_cogl_pango_glyph_cache_remove_reorganize_callback (CoglPangoGlyphCache *cache,
                                                    GHookFunc func,
                                                    void *user_data);

void
_cogl_pango_glyph_cache_set_dirty_glyphs (CoglPangoGlyphCache *cache,
                                          CoglPangoGlyphCacheDirtyFunc func);

COGL_END_DECLS

#endif /* __COGL_PANGO_GLYPH_CACHE_H__ */
