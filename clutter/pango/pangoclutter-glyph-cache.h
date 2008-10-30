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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HAVE_PANGO_CLUTTER_GLYPH_CACHE_H
#define _HAVE_PANGO_CLUTTER_GLYPH_CACHE_H

#include <glib.h>
#include <cogl/cogl.h>
#include <pango/pango-font.h>

G_BEGIN_DECLS

typedef struct _PangoClutterGlyphCache      PangoClutterGlyphCache;
typedef struct _PangoClutterGlyphCacheValue PangoClutterGlyphCacheValue;

struct _PangoClutterGlyphCacheValue
{
  CoglHandle texture;
  CoglFixed  tx1, ty1, tx2, ty2;
  int        draw_x, draw_y, draw_width, draw_height;
};

PangoClutterGlyphCache *pango_clutter_glyph_cache_new (gboolean use_mipmapping);

void pango_clutter_glyph_cache_free (PangoClutterGlyphCache *cache);

PangoClutterGlyphCacheValue *
pango_clutter_glyph_cache_lookup (PangoClutterGlyphCache *cache,
				  PangoFont              *font,
				  PangoGlyph              glyph);

PangoClutterGlyphCacheValue *
pango_clutter_glyph_cache_set (PangoClutterGlyphCache *cache,
			       PangoFont              *font,
			       PangoGlyph              glyph,
			       gconstpointer           pixels,
			       int                     width,
			       int                     height,
			       int                     stride,
			       int                     draw_x,
			       int                     draw_y);

void pango_clutter_glyph_cache_clear (PangoClutterGlyphCache *cache);

G_END_DECLS

#endif /* _HAVE_PANGO_CLUTTER_GLYPH_CACHE_H */
