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

#ifndef __PANGO_CLUTTER_H__
#define __PANGO_CLUTTER_H__

#include <glib-object.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

/* It's too difficult to actually subclass the pango cairo font
 * map. Instead we just make a fake set of macros that actually just
 * directly use the original type
 */
#define COGL_PANGO_TYPE_FONT_MAP        PANGO_TYPE_CAIRO_FONT_MAP
#define COGL_PANGO_FONT_MAP(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), COGL_PANGO_TYPE_FONT_MAP, CoglPangoFontMap))
#define COGL_PANGO_IS_FONT_MAP(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COGL_PANGO_TYPE_FONT_MAP))

typedef PangoCairoFontMap CoglPangoFontMap;

PangoFontMap * cogl_pango_font_map_new                  (void);
PangoContext * cogl_pango_font_map_create_context       (CoglPangoFontMap *fm);
void           cogl_pango_font_map_set_resolution       (CoglPangoFontMap *font_map,
                                                         double            dpi);
void           cogl_pango_font_map_clear_glyph_cache    (CoglPangoFontMap *fm);
void           cogl_pango_ensure_glyph_cache_for_layout (PangoLayout      *layout);
void           cogl_pango_font_map_set_use_mipmapping   (CoglPangoFontMap *fm,
                                                         gboolean          value);
gboolean       cogl_pango_font_map_get_use_mipmapping   (CoglPangoFontMap *fm);
PangoRenderer *cogl_pango_font_map_get_renderer         (CoglPangoFontMap *fm);

#define COGL_PANGO_TYPE_RENDERER                (cogl_pango_renderer_get_type ())
#define COGL_PANGO_RENDERER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), COGL_PANGO_TYPE_RENDERER, CoglPangoRenderer))
#define COGL_PANGO_RENDERER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), COGL_PANGO_TYPE_RENDERER, CoglPangoRendererClass))
#define COGL_PANGO_IS_RENDERER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COGL_PANGO_TYPE_RENDERER))
#define COGL_PANGO_IS_RENDERER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass),  COGL_PANGO_TYPE_RENDERER))
#define COGL_PANGO_RENDERER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), COGL_PANGO_TYPE_RENDERER, CoglPangoRendererClass))

/* opaque types */
typedef struct _CoglPangoRenderer      CoglPangoRenderer;
typedef struct _CoglPangoRendererClass CoglPangoRendererClass;

GType cogl_pango_renderer_get_type (void) G_GNUC_CONST;

void cogl_pango_render_layout_subpixel (PangoLayout     *layout,
                                        int              x,
                                        int              y,
                                        const CoglColor *color,
                                        int              flags);
void cogl_pango_render_layout          (PangoLayout     *layout,
                                        int              x,
                                        int              y,
                                        const CoglColor *color,
                                        int              flags);
void cogl_pango_render_layout_line     (PangoLayoutLine *line,
                                        int              x,
                                        int              y,
                                        const CoglColor *color);

G_END_DECLS

#endif /* __PANGO_CLUTTER_H__ */
