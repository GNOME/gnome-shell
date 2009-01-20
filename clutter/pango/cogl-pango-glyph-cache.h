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
#include <cogl/cogl.h>
#include <pango/pango-font.h>

G_BEGIN_DECLS

typedef struct _CoglPangoGlyphCache      CoglPangoGlyphCache;
typedef struct _CoglPangoGlyphCacheValue CoglPangoGlyphCacheValue;

struct _CoglPangoGlyphCacheValue
{
  CoglHandle texture;

  float  tx1;
  float  ty1;
  float  tx2;
  float  ty2;

  int        draw_x;
  int        draw_y;
  int        draw_width;
  int        draw_height;
};

CoglPangoGlyphCache *
cogl_pango_glyph_cache_new (gboolean use_mipmapping);

void
cogl_pango_glyph_cache_free (CoglPangoGlyphCache *cache);

CoglPangoGlyphCacheValue *
cogl_pango_glyph_cache_lookup (CoglPangoGlyphCache *cache,
                               PangoFont           *font,
                               PangoGlyph           glyph);

CoglPangoGlyphCacheValue *
cogl_pango_glyph_cache_set (CoglPangoGlyphCache *cache,
                            PangoFont           *font,
                            PangoGlyph           glyph,
                            gconstpointer        pixels,
                            int                  width,
			    int                  height,
			    int                  stride,
			    int                  draw_x,
			    int                  draw_y);

void
cogl_pango_glyph_cache_clear (CoglPangoGlyphCache *cache);

G_END_DECLS

#endif /* __COGL_PANGO_GLYPH_CACHE_H__ */
