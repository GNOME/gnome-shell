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

/**
 * SECTION:cogl-pango
 * @short_description: COGL-based text rendering using Pango
 *
 * FIXME
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* This is needed to get the Pango headers to export stuff needed to
   subclass */
#ifndef PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_BACKEND 1
#endif

#include <pango/pango-fontmap.h>
#include <pango/pangocairo.h>
#include <pango/pango-renderer.h>

#include "cogl-pango.h"
#include "cogl-pango-private.h"

static GQuark cogl_pango_font_map_get_renderer_key (void) G_GNUC_CONST;

/**
 * cogl_pango_font_map_new:
 *
 * Creates a new font map.
 *
 * Return value: the newly created #PangoFontMap
 *
 * Since: 1.0
 */
PangoFontMap *
cogl_pango_font_map_new (void)
{
  return pango_cairo_font_map_new ();
}

/**
 * cogl_pango_font_map_create_context:
 * @fm: a #CoglPangoFontMap
 *
 * Creates a new #PangoContext from the passed font map.
 *
 * Return value: the newly created #PangoContext
 *
 * Since: 1.0
 */
PangoContext *
cogl_pango_font_map_create_context (CoglPangoFontMap *fm)
{
  g_return_val_if_fail (COGL_PANGO_IS_FONT_MAP (fm), NULL);

  /* We can just directly use the pango context from the Cairo font
     map */
  return pango_cairo_font_map_create_context (PANGO_CAIRO_FONT_MAP (fm));
}

/**
 * cogl_pango_font_map_get_renderer:
 * @fm: a #CoglPangoFontMap
 *
 * Retrieves the #CoglPangoRenderer for the passed font map.
 *
 * Return value: a #PangoRenderer
 *
 * Since: 1.0
 */
PangoRenderer *
cogl_pango_font_map_get_renderer (CoglPangoFontMap *fm)
{
  PangoRenderer *renderer;

  g_return_val_if_fail (COGL_PANGO_IS_FONT_MAP (fm), NULL);

  /* We want to keep a cached pointer to the renderer from the font
     map instance but as we don't have a proper subclass we have to
     store it in the object data instead */

  renderer = g_object_get_qdata (G_OBJECT (fm),
				 cogl_pango_font_map_get_renderer_key ());

  if (G_UNLIKELY (renderer == NULL))
    {
      renderer = g_object_new (COGL_PANGO_TYPE_RENDERER, NULL);
      g_object_set_qdata_full (G_OBJECT (fm),
			       cogl_pango_font_map_get_renderer_key (),
			       renderer,
			       g_object_unref);
    }

  return renderer;
}

/**
 * cogl_pango_font_map_set_resolution:
 * @font_map: a #CoglPangoFontMap
 * @dpi: DPI to set
 *
 * Sets the resolution to be used by @font_map at @dpi.
 *
 * Since: 1.0
 */
void
cogl_pango_font_map_set_resolution (CoglPangoFontMap *font_map,
				    double            dpi)
{
  g_return_if_fail (COGL_PANGO_IS_FONT_MAP (font_map));

  pango_cairo_font_map_set_resolution (PANGO_CAIRO_FONT_MAP (font_map), dpi);
}

/**
 * cogl_pango_font_map_clear_glyph_cache:
 * @fm: a #CoglPangoFontMap
 *
 * Clears the glyph cache for @fm.
 *
 * Since: 1.0
 */
void
cogl_pango_font_map_clear_glyph_cache (CoglPangoFontMap *fm)
{
  PangoRenderer *renderer;

  renderer = cogl_pango_font_map_get_renderer (fm);

  _cogl_pango_renderer_clear_glyph_cache (COGL_PANGO_RENDERER (renderer));
}

/**
 * cogl_pango_font_map_set_use_mipmapping:
 * @fm: a #CoglPangoFontMap
 * @value: %TRUE to enable the use of mipmapping
 *
 * Sets whether the renderer for the passed font map should use
 * mipmapping when rendering a #PangoLayout.
 *
 * Since: 1.0
 */
void
cogl_pango_font_map_set_use_mipmapping (CoglPangoFontMap *fm,
                                        gboolean          value)
{
  CoglPangoRenderer *renderer;

  renderer = COGL_PANGO_RENDERER (cogl_pango_font_map_get_renderer (fm));

  _cogl_pango_renderer_set_use_mipmapping (renderer, value);
}

/**
 * cogl_pango_font_map_get_use_mipmapping:
 * @fm: a #CoglPangoFontMap
 *
 * Retrieves whether the #CoglPangoRenderer used by @fm will
 * use mipmapping when rendering the glyphs.
 *
 * Return value: %TRUE if mipmapping is used, %FALSE otherwise.
 *
 * Since: 1.0
 */
gboolean
cogl_pango_font_map_get_use_mipmapping (CoglPangoFontMap *fm)
{
  CoglPangoRenderer *renderer;

  renderer = COGL_PANGO_RENDERER (cogl_pango_font_map_get_renderer (fm));

  return _cogl_pango_renderer_get_use_mipmapping (renderer);
}

static GQuark
cogl_pango_font_map_get_renderer_key (void)
{
  static GQuark renderer_key = 0;

  if (G_UNLIKELY (renderer_key == 0))
      renderer_key = g_quark_from_static_string ("CoglPangoFontMap");

  return renderer_key;
}
