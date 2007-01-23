/* Pango
 * pangoclutter-private.h: private symbols for Clutter backend
 *
 * Copyright (C) 2006 Matthew Allum <mallum@o-hand.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __PANGOCLUTTER_PRIVATE_H__
#define __PANGOCLUTTER_PRIVATE_H__

#include "pangoclutter.h"
#include <pango/pango-renderer.h>
#include <glib-object.h>
#include <pango/pangofc-decoder.h>

/* Defines duped */

#define PANGO_SCALE_26_6 (PANGO_SCALE / (1<<6))

/* We only use the PANGO_SCALE_26_6 macro for scaling font size.
 * Font sizes are normally given in points with at most one single
 * decimal place fraction. If we do not do the rounding here, we will
 * be suffering from an error < 0.016pt, which is entirely negligeable
 * as far as font sizes are concerned.
 */
#if 0
#define PANGO_PIXELS_26_6(d)                            \
  (((d) >= 0) ?                                         \
   ((d) + PANGO_SCALE_26_6 / 2) / PANGO_SCALE_26_6 :    \
   ((d) - PANGO_SCALE_26_6 / 2) / PANGO_SCALE_26_6)
#else
#define PANGO_PIXELS_26_6(d)                            \
   (d / PANGO_SCALE_26_6)
#endif

#define PANGO_UNITS_26_6(d) (PANGO_SCALE_26_6 * (d))

#define PANGO_TYPE_CLUTTER_FONT (pango_clutter_font_get_type ())

#define PANGO_CLUTTER_FONT(object)                                   \
               (G_TYPE_CHECK_INSTANCE_CAST ((object),                \
					    PANGO_TYPE_CLUTTER_FONT, \
					    PangoClutterFont))
#define PANGO_CLUTTER_IS_FONT(object)                                  \
               (G_TYPE_CHECK_INSTANCE_TYPE ((object), PANGO_TYPE_CLUTTER_FONT))

typedef struct _PangoClutterFont       PangoClutterFont;
typedef struct _PangoClutterGlyphInfo  PangoClutterGlyphInfo;

struct _PangoClutterFont
{
  PangoFcFont    font;
  FT_Face        face;
  int            load_flags;
  int            size;
  GSList        *metrics_by_lang;
  GHashTable    *glyph_info;
  GDestroyNotify glyph_cache_destroy;
};

struct _PangoClutterGlyphInfo
{
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
  void          *cached_glyph;
};

PangoGlyph
pango_clutter_get_unknown_glyph (PangoFont *font);

GType pango_clutter_font_get_type (void);

PangoClutterFont *
_pango_clutter_font_new (PangoClutterFontMap *fontmap, 
			 FcPattern           *pattern);
FT_Face
pango_clutter_font_get_face (PangoFont *font);

FT_Library 
_pango_clutter_font_map_get_library (PangoFontMap *fontmap);

void *
_pango_clutter_font_get_cache_glyph_data (PangoFont *font, 
					  int        glyph_index);
void 
_pango_clutter_font_set_cache_glyph_data (PangoFont *font, 
					  int        glyph_index, 
					  void      *cached_glyph);
void 
_pango_clutter_font_set_glyph_cache_destroy (PangoFont    *font, 
					     GDestroyNotify destroy_notify);

/* Renderer  */

typedef struct _PangoClutterRenderer     PangoClutterRenderer;

#define PANGO_TYPE_CLUTTER_RENDERER  (pango_clutter_renderer_get_type())

#define PANGO_CLUTTER_RENDERER(object)                               \
           (G_TYPE_CHECK_INSTANCE_CAST ((object),                    \
                                        PANGO_TYPE_CLUTTER_RENDERER, \
					PangoClutterRenderer))

#define PANGO_IS_CLUTTER_RENDERER(object)                            \
           (G_TYPE_CHECK_INSTANCE_TYPE ((object), PANGO_TYPE_CLUTTER_RENDERER))

GType pango_clutter_renderer_get_type (void);

PangoRenderer *
_pango_clutter_font_map_get_renderer (PangoClutterFontMap *fontmap);


/* HACK make this public to avoid a mass of re-implementation*/
void 
pango_fc_font_get_raw_extents (PangoFcFont    *font, 
			       FT_Int32        load_flags, 
			       PangoGlyph      glyph, 
			       PangoRectangle *ink_rect, 
			       PangoRectangle *logical_rect);

#endif
