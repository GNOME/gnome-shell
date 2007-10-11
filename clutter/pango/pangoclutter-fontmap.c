/* Pango
 * Clutter fonts handling
 *
 * Copyright (C) 2000 Red Hat Software
 * Copyright (C) 2000 Tor Lillqvist
 * Copyright (C) 2006 Marc Lehmann <pcg@goof.com>
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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pangoclutter.h"
#include "pangoclutter-private.h"


#include <pango/pangofc-font.h>
#include <pango/pangofc-fontmap.h>

struct _PangoClutterFontMap
{
  PangoFcFontMap parent_instance;

  FT_Library library;

  double dpi;

  /* Function to call on prepared patterns to do final
   * config tweaking.
   */
  PangoClutterSubstituteFunc substitute_func;
  gpointer substitute_data;
  GDestroyNotify substitute_destroy;

  PangoRenderer *renderer;
};

struct _PangoClutterFontMapClass
{
  PangoFcFontMapClass parent_class;
};

G_DEFINE_TYPE (PangoClutterFontMap, pango_clutter_font_map, PANGO_TYPE_FC_FONT_MAP)

static void
pango_clutter_font_map_finalize (GObject *object)
{
  PangoClutterFontMap *fontmap = PANGO_CLUTTER_FONT_MAP (object);
  
  if (fontmap->renderer)
    g_object_unref (fontmap->renderer);

  if (fontmap->substitute_destroy)
    fontmap->substitute_destroy (fontmap->substitute_data);

  FT_Done_FreeType (fontmap->library);

  G_OBJECT_CLASS (pango_clutter_font_map_parent_class)->finalize (object);
}

PangoFontMap *
pango_clutter_font_map_new (void)
{
  PangoClutterFontMap *fontmap;
  FT_Error error;
  
  /* Make sure that the type system is initialized */
  g_type_init ();
  
  fontmap = g_object_new (PANGO_TYPE_CLUTTER_FONT_MAP, NULL);
  
  error = FT_Init_FreeType (&fontmap->library);
  if (error != FT_Err_Ok)
    g_critical ("pango_clutter_font_map_new: Could not initialize freetype");

  return (PangoFontMap *)fontmap;
}

void
pango_clutter_font_map_set_default_substitute (PangoClutterFontMap        *fontmap,
					   PangoClutterSubstituteFunc  func,
					   gpointer                data,
					   GDestroyNotify          notify)
{
  if (fontmap->substitute_destroy)
    fontmap->substitute_destroy (fontmap->substitute_data);

  fontmap->substitute_func = func;
  fontmap->substitute_data = data;
  fontmap->substitute_destroy = notify;
  
  pango_fc_font_map_cache_clear (PANGO_FC_FONT_MAP (fontmap));
}

/**
 * pango_clutter_font_map_substitute_changed:
 * @fontmap: a #PangoClutterFontmap
 * 
 * Call this function any time the results of the
 * default substitution function set with
 * pango_clutter_font_map_set_default_substitute() change.
 * That is, if your subsitution function will return different
 * results for the same input pattern, you must call this function.
 *
 * Since: 1.2
 **/
void
pango_clutter_font_map_substitute_changed (PangoClutterFontMap *fontmap)
{
  pango_fc_font_map_cache_clear (PANGO_FC_FONT_MAP (fontmap));
}

/**
 * pango_clutter_font_map_create_context:
 * @fontmap: a #PangoClutterFontmap
 * 
 * Create a #PangoContext for the given fontmap.
 * 
 * Return value: the newly created context; free with g_object_unref().
 *
 * Since: 1.2
 **/
PangoContext *
pango_clutter_font_map_create_context (PangoClutterFontMap *fontmap)
{
  g_return_val_if_fail (PANGO_CLUTTER_IS_FONT_MAP (fontmap), NULL);
  
  return pango_fc_font_map_create_context (PANGO_FC_FONT_MAP (fontmap));
}

FT_Library
_pango_clutter_font_map_get_library (PangoFontMap *fontmap_)
{
  PangoClutterFontMap *fontmap = (PangoClutterFontMap *)fontmap_;
  
  return fontmap->library;
}

void
pango_clutter_font_map_set_resolution (PangoClutterFontMap *fontmap,
                                       double               dpi)
{
  g_return_if_fail (PANGO_CLUTTER_IS_FONT_MAP (fontmap));

  fontmap->dpi = dpi;

  pango_clutter_font_map_substitute_changed (fontmap);
}

/**
 * _pango_clutter_font_map_get_renderer:
 * @fontmap: a #PangoClutterFontmap
 * 
 * Gets the singleton PangoClutterRenderer for this fontmap.
 * 
 * Return value: 
 **/
PangoRenderer *
_pango_clutter_font_map_get_renderer (PangoClutterFontMap *fontmap)
{
  if (!fontmap->renderer)
    fontmap->renderer = g_object_new (PANGO_TYPE_CLUTTER_RENDERER, NULL);

  return fontmap->renderer;
}

static void
pango_clutter_font_map_default_substitute (PangoFcFontMap *fcfontmap,
				       FcPattern      *pattern)
{
  PangoClutterFontMap *fontmap = PANGO_CLUTTER_FONT_MAP (fcfontmap);

  FcConfigSubstitute (NULL, pattern, FcMatchPattern);

  if (fontmap->substitute_func)
    fontmap->substitute_func (pattern, fontmap->substitute_data);

#if 0
  FcValue v;
  if (FcPatternGet (pattern, FC_DPI, 0, &v) == FcResultNoMatch)
    FcPatternAddDouble (pattern, FC_DPI, fontmap->dpi_y);
#endif

   /* Turn off hinting, since we most of the time are not using the glyphs
    * from our cache at their nativly rendered resolution
    */
   FcPatternDel (pattern, FC_HINTING);
   FcPatternAddBool (pattern, FC_HINTING, FALSE);

  FcDefaultSubstitute (pattern);
}

static PangoFcFont *
pango_clutter_font_map_new_font (PangoFcFontMap  *fcfontmap,
			        FcPattern       *pattern)
{
  return (PangoFcFont *)_pango_clutter_font_new (PANGO_CLUTTER_FONT_MAP (fcfontmap), pattern);
}

static double
pango_clutter_font_map_get_resolution (PangoFcFontMap *fcfontmap,
                                       PangoContext   *context)
{
  return ((PangoClutterFontMap *)fcfontmap)->dpi;
}

static void
pango_clutter_font_map_class_init (PangoClutterFontMapClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  PangoFcFontMapClass *fcfontmap_class = PANGO_FC_FONT_MAP_CLASS (class);
  
  gobject_class->finalize = pango_clutter_font_map_finalize;
  fcfontmap_class->default_substitute = pango_clutter_font_map_default_substitute;
  fcfontmap_class->new_font = pango_clutter_font_map_new_font;
  fcfontmap_class->get_resolution = pango_clutter_font_map_get_resolution;
}

static void
pango_clutter_font_map_init (PangoClutterFontMap *fontmap)
{
  fontmap->library = NULL;
  fontmap->dpi = 96.0;
}

