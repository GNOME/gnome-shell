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

#include "pangoclutter.h"
#include "pangoclutter-private.h"

static GQuark pango_clutter_font_map_get_renderer_key (void) G_GNUC_CONST;

PangoFontMap *
pango_clutter_font_map_new (void)
{
  return pango_cairo_font_map_new ();
}

PangoContext *
pango_clutter_font_map_create_context (PangoClutterFontMap *fm)
{
  g_return_val_if_fail (PANGO_CLUTTER_IS_FONT_MAP (fm), NULL);

  /* We can just directly use the pango context from the Cairo font
     map */
  return pango_cairo_font_map_create_context (PANGO_CAIRO_FONT_MAP (fm));
}

PangoRenderer *
_pango_clutter_font_map_get_renderer (PangoClutterFontMap *fm)
{
  PangoRenderer *renderer;

  /* We want to keep a cached pointer to the renderer from the font
     map instance but as we don't have a proper subclass we have to
     store it in the object data instead */

  renderer = g_object_get_qdata (G_OBJECT (fm),
				 pango_clutter_font_map_get_renderer_key ());

  if (G_UNLIKELY (renderer == NULL))
    {
      renderer = g_object_new (PANGO_CLUTTER_TYPE_RENDERER, NULL);
      g_object_set_qdata_full (G_OBJECT (fm),
			       pango_clutter_font_map_get_renderer_key (),
			       renderer,
			       g_object_unref);
    }

  return renderer;
}

void
pango_clutter_font_map_set_resolution (PangoClutterFontMap *font_map,
				       double               dpi)
{
  g_return_if_fail (PANGO_CLUTTER_IS_FONT_MAP (font_map));

  pango_cairo_font_map_set_resolution (PANGO_CAIRO_FONT_MAP (font_map), dpi);
}

void
pango_clutter_font_map_clear_glyph_cache (PangoClutterFontMap *fm)
{
  PangoRenderer *renderer;

  renderer = _pango_clutter_font_map_get_renderer (fm);

  _pango_clutter_renderer_clear_glyph_cache (PANGO_CLUTTER_RENDERER (renderer));
}

void
pango_clutter_font_map_set_use_mipmapping (PangoClutterFontMap *fm,
					   gboolean             value)
{
  PangoClutterRenderer *renderer;

  renderer = PANGO_CLUTTER_RENDERER (_pango_clutter_font_map_get_renderer (fm));

  _pango_clutter_renderer_set_use_mipmapping (renderer, value);
}

gboolean
pango_clutter_font_map_get_use_mipmapping (PangoClutterFontMap *fm)
{
  PangoClutterRenderer *renderer;

  renderer = PANGO_CLUTTER_RENDERER (_pango_clutter_font_map_get_renderer (fm));

  return _pango_clutter_renderer_get_use_mipmapping (renderer);
}

static GQuark
pango_clutter_font_map_get_renderer_key (void)
{
  static GQuark renderer_key = 0;

  if (G_UNLIKELY (renderer_key == 0))
      renderer_key = g_quark_from_static_string ("PangoClutterFontMap");

  return renderer_key;
}
