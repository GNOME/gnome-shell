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

#ifndef _HAVE_PANGO_CLUTTER_H
#define _HAVE_PANGO_CLUTTER_H

#include <glib-object.h>
#include <pango/pangocairo.h>
#include <clutter/clutter-color.h>

G_BEGIN_DECLS

/* It's too difficult to actually subclass the pango cairo font
   map. Instead we just make a fake set of macros that actually just
   directly use the original type */
#define PANGO_CLUTTER_TYPE_FONT_MAP PANGO_TYPE_CAIRO_FONT_MAP

#define PANGO_CLUTTER_FONT_MAP(obj)				\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),				\
			       PANGO_CLUTTER_TYPE_FONT_MAP,	\
			       PangoClutterFontMap))
#define PANGO_CLUTTER_IS_FONT_MAP(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),				\
			       PANGO_CLUTTER_TYPE_FONT_MAP))

typedef PangoCairoFontMap PangoClutterFontMap;

PangoFontMap *pango_clutter_font_map_new (void);

PangoContext *pango_clutter_font_map_create_context (PangoClutterFontMap *fm);

void pango_clutter_font_map_set_resolution (PangoClutterFontMap *font_map,
					    double               dpi);

void pango_clutter_font_map_clear_glyph_cache (PangoClutterFontMap *fm);

void pango_clutter_font_map_set_use_mipmapping (PangoClutterFontMap *fm,
						gboolean             value);

void pango_clutter_ensure_glyph_cache_for_layout (PangoLayout *layout);

#define PANGO_CLUTTER_TYPE_RENDERER (pango_clutter_renderer_get_type ())

#define PANGO_CLUTTER_RENDERER(obj)				\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),				\
			       PANGO_CLUTTER_TYPE_RENDERER,	\
			       PangoClutterRenderer))
#define PANGO_CLUTTER_RENDERER_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_CAST ((klass),				\
			    PANGO_CLUTTER_TYPE_RENDERER,	\
			    PangoClutterRendererClass))
#define PANGO_CLUTTER_IS_RENDERER(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),				\
			       PANGO_CLUTTER_TYPE_RENDERER))
#define PANGO_CLUTTER_IS_RENDERER_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE ((klass),				\
			    PANGO_CLUTTER_TYPE_RENDERER))
#define PANGO_CLUTTER_RENDERER_GET_CLASS(obj)			\
  (G_TYPE_INSTANCE_GET_CLASS ((obj),				\
			      PANGO_CLUTTER_TYPE_RENDERER,	\
			      PangoClutterRendererClass))

typedef struct _PangoClutterRenderer      PangoClutterRenderer;
typedef struct _PangoClutterRendererClass PangoClutterRendererClass;

GType pango_clutter_renderer_get_type (void) G_GNUC_CONST;

void pango_clutter_render_layout_subpixel (PangoLayout  *layout,
					   int           x,
					   int           y,
					   ClutterColor *color,
					   int           flags);

void pango_clutter_render_layout (PangoLayout  *layout,
				  int           x,
				  int           y,
				  ClutterColor *color,
				  int           flags);

void pango_clutter_render_layout_line (PangoLayoutLine  *line,
				       int               x,
				       int               y,
				       ClutterColor     *color);

G_END_DECLS

#endif /* _HAVE_PANGO_CLUTTER_H */
