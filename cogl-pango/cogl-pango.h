/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008 OpenedHand
 * Copyright (C) 2012 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 *   Matthew Allum  <mallum@openedhand.com>
 */

#ifndef __COGL_PANGO_H__
#define __COGL_PANGO_H__

#include <glib-object.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <cogl/cogl.h>

COGL_BEGIN_DECLS

/* It's too difficult to actually subclass the pango cairo font
 * map. Instead we just make a fake set of macros that actually just
 * directly use the original type
 */
#define COGL_PANGO_TYPE_FONT_MAP        PANGO_TYPE_CAIRO_FONT_MAP
#define COGL_PANGO_FONT_MAP(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), COGL_PANGO_TYPE_FONT_MAP, CoglPangoFontMap))
#define COGL_PANGO_IS_FONT_MAP(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COGL_PANGO_TYPE_FONT_MAP))

typedef PangoCairoFontMap CoglPangoFontMap;

/**
 * cogl_pango_font_map_new:
 *
 * Creates a new font map.
 *
 * Return value: (transfer full): the newly created #PangoFontMap
 *
 * Since: 1.14
 */
PangoFontMap *
cogl_pango_font_map_new (void);

/**
 * cogl_pango_font_map_create_context:
 * @font_map: a #CoglPangoFontMap
 *
 * Create a #PangoContext for the given @font_map.
 *
 * Returns: (transfer full): the newly created context: free with g_object_unref().
 */
PangoContext *
cogl_pango_font_map_create_context (CoglPangoFontMap *font_map);

/**
 * cogl_pango_font_map_set_resolution:
 * @font_map: a #CoglPangoFontMap
 * @dpi: The resolution in "dots per inch". (Physical inches aren't
 *       actually involved; the terminology is conventional.)
 *
 * Sets the resolution for the @font_map. This is a scale factor
 * between points specified in a #PangoFontDescription and Cogl units.
 * The default value is %96, meaning that a 10 point font will be 13
 * units high. (10 * 96. / 72. = 13.3).
 *
 * Since: 1.14
 */
void
cogl_pango_font_map_set_resolution (CoglPangoFontMap *font_map,
                                    double dpi);

/**
 * cogl_pango_font_map_clear_glyph_cache:
 * @font_map: a #CoglPangoFontMap
 *
 * Clears the glyph cache for @font_map.
 *
 * Since: 1.0
 */
void
cogl_pango_font_map_clear_glyph_cache (CoglPangoFontMap *font_map);

/**
 * cogl_pango_ensure_glyph_cache_for_layout:
 * @layout: A #PangoLayout
 *
 * This updates any internal glyph cache textures as necessary to be
 * able to render the given @layout.
 *
 * This api should be used to avoid mid-scene modifications of
 * glyph-cache textures which can lead to undefined rendering results.
 *
 * Since: 1.0
 */
void
cogl_pango_ensure_glyph_cache_for_layout (PangoLayout *layout);

/**
 * cogl_pango_font_map_set_use_mipmapping:
 * @font_map: a #CoglPangoFontMap
 * @value: %TRUE to enable the use of mipmapping
 *
 * Sets whether the renderer for the passed font map should use
 * mipmapping when rendering a #PangoLayout.
 *
 * Since: 1.0
 */
void
cogl_pango_font_map_set_use_mipmapping (CoglPangoFontMap *font_map,
                                        CoglBool value);

/**
 * cogl_pango_font_map_get_use_mipmapping:
 * @font_map: a #CoglPangoFontMap
 *
 * Retrieves whether the #CoglPangoRenderer used by @font_map will use
 * mipmapping when rendering the glyphs.
 *
 * Return value: %TRUE if mipmapping is used, %FALSE otherwise.
 *
 * Since: 1.0
 */
CoglBool
cogl_pango_font_map_get_use_mipmapping (CoglPangoFontMap *font_map);

/**
 * cogl_pango_font_map_get_renderer:
 * @font_map: a #CoglPangoFontMap
 *
 * Retrieves the #CoglPangoRenderer for the passed @font_map.
 *
 * Return value: (transfer none): a #PangoRenderer
 *
 * Since: 1.0
 */
PangoRenderer *
cogl_pango_font_map_get_renderer (CoglPangoFontMap *font_map);

/**
 * cogl_pango_show_layout:
 * @framebuffer: A #CoglFramebuffer to draw too.
 * @layout: a #PangoLayout
 * @x: X coordinate to render the layout at
 * @y: Y coordinate to render the layout at
 * @color: color to use when rendering the layout
 *
 * Draws a solidly coloured @layout on the given @framebuffer at (@x,
 * @y) within the @framebuffer<!-- -->'s current model-view coordinate
 * space.
 *
 * Since: 1.14
 */
void
cogl_pango_show_layout (CoglFramebuffer *framebuffer,
                        PangoLayout *layout,
                        float x,
                        float y,
                        const CoglColor *color);

/**
 * cogl_pango_show_layout_line:
 * @framebuffer: A #CoglFramebuffer to draw too.
 * @line: a #PangoLayoutLine
 * @x: X coordinate to render the line at
 * @y: Y coordinate to render the line at
 * @color: color to use when rendering the line
 *
 * Draws a solidly coloured @line on the given @framebuffer at (@x,
 * @y) within the @framebuffer<!-- -->'s current model-view coordinate
 * space.
 *
 * Since: 1.14
 */
void
cogl_pango_show_layout_line (CoglFramebuffer *framebuffer,
                             PangoLayoutLine *line,
                             float x,
                             float y,
                             const CoglColor *color);


#define COGL_PANGO_TYPE_RENDERER                (cogl_pango_renderer_get_type ())
#define COGL_PANGO_RENDERER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), COGL_PANGO_TYPE_RENDERER, CoglPangoRenderer))
#define COGL_PANGO_RENDERER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), COGL_PANGO_TYPE_RENDERER, CoglPangoRendererClass))
#define COGL_PANGO_IS_RENDERER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COGL_PANGO_TYPE_RENDERER))
#define COGL_PANGO_IS_RENDERER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass),  COGL_PANGO_TYPE_RENDERER))
#define COGL_PANGO_RENDERER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), COGL_PANGO_TYPE_RENDERER, CoglPangoRendererClass))

/* opaque types */
typedef struct _CoglPangoRenderer      CoglPangoRenderer;
typedef struct _CoglPangoRendererClass CoglPangoRendererClass;

GType cogl_pango_renderer_get_type (void) G_GNUC_CONST;

/**
 * cogl_pango_render_layout_subpixel:
 * @layout: a #PangoLayout
 * @x: X coordinate (in Pango units) to render the layout at
 * @y: Y coordinate (in Pango units) to render the layout at
 * @color: color to use when rendering the layout
 * @flags:
 *
 * Draws a solidly coloured @layout on the given @framebuffer at (@x,
 * @y) within the @framebuffer<!-- -->'s current model-view coordinate
 * space.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pango_show_layout() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pango_show_layout)
void
cogl_pango_render_layout_subpixel (PangoLayout *layout,
                                   int x,
                                   int y,
                                   const CoglColor *color,
                                   int flags);

/**
 * cogl_pango_render_layout:
 * @layout: a #PangoLayout
 * @x: X coordinate to render the layout at
 * @y: Y coordinate to render the layout at
 * @color: color to use when rendering the layout
 * @flags:
 *
 * Draws a solidly coloured @layout on the given @framebuffer at (@x,
 * @y) within the @framebuffer<!-- -->'s current model-view coordinate
 * space.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pango_show_layout() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pango_show_layout)
void
cogl_pango_render_layout (PangoLayout *layout,
                          int x,
                          int y,
                          const CoglColor *color,
                          int flags);

/**
 * cogl_pango_render_layout_line:
 * @line: a #PangoLayoutLine
 * @x: X coordinate to render the line at
 * @y: Y coordinate to render the line at
 * @color: color to use when rendering the line
 *
 * Renders @line at the given coordinates using the given color.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pango_show_layout() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pango_show_layout_line)
void
cogl_pango_render_layout_line (PangoLayoutLine *line,
                               int x,
                               int y,
                               const CoglColor *color);

COGL_END_DECLS

#endif /* __COGL_PANGO_H__ */
