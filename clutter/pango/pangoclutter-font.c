/* Pango
 * Clutter Freetype2 handling
 *
 * Copyright (C) 1999 Red Hat Software
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

#define PANGO_ENABLE_BACKEND

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "pangoclutter.h"
#include "pangoclutter-private.h"
#include <pango/pangofc-font.h>
#include <pango/pangofc-fontmap.h>

#define PANGO_CLUTTER_FONT_CLASS(klass)                 \
    (G_TYPE_CHECK_CLASS_CAST ((klass),                 \
                               PANGO_TYPE_CLUTTER_FONT, \
			       PangoClutterFontClass))

#define PANGO_CLUTTER_IS_FONT_CLASS(klass)              \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), PANGO_TYPE_CLUTTER_FONT))

#define PANGO_CLUTTER_FONT_GET_CLASS(obj)               \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),                 \
                                PANGO_TYPE_CLUTTER_FONT, \
                                PangoClutterFontClass))

typedef struct _PangoClutterFontClass   PangoClutterFontClass;

struct _PangoClutterFontClass
{
  PangoFcFontClass parent_class;
};

static void pango_clutter_font_finalize (GObject *object);

static void pango_clutter_font_get_glyph_extents (PangoFont *font,
		           	                 PangoGlyph glyph,
		           	                 PangoRectangle *ink_rect,
		           			 PangoRectangle *logical_rect);

static FT_Face pango_clutter_font_real_lock_face   (PangoFcFont      *font);
static void    pango_clutter_font_real_unlock_face (PangoFcFont      *font);

PangoClutterFont *
_pango_clutter_font_new (PangoClutterFontMap *fontmap_, FcPattern *pattern)
{
  PangoFontMap *fontmap = PANGO_FONT_MAP (fontmap_);
  PangoClutterFont *font;
  double d;

  g_return_val_if_fail (fontmap != NULL, NULL);
  g_return_val_if_fail (pattern != NULL, NULL);

  font = (PangoClutterFont *)g_object_new (PANGO_TYPE_CLUTTER_FONT,
					  "pattern", pattern,
					  NULL);

  if (FcPatternGetDouble (pattern, FC_PIXEL_SIZE, 0, &d) == FcResultMatch)
    font->size = d * PANGO_SCALE;

  return font;
}

static void
load_fallback_face (PangoClutterFont *font, const char *original_file)
{
  PangoFcFont *fcfont = PANGO_FC_FONT (font);
  FcPattern *sans;
  FcPattern *matched;
  FcResult result;
  FT_Error error;
  FcChar8 *filename2 = NULL;
  gchar *name;
  int id;
  
  sans = FcPatternBuild 
            (NULL,
	     FC_FAMILY, FcTypeString, "sans",
	     FC_PIXEL_SIZE, FcTypeDouble, (double)font->size / PANGO_SCALE,
	     NULL);
  
  matched = FcFontMatch (NULL, sans, &result);
  
  if (FcPatternGetString (matched, FC_FILE, 0, &filename2) != FcResultMatch)
    goto bail1;
  
  if (FcPatternGetInteger (matched, FC_INDEX, 0, &id) != FcResultMatch)
    goto bail1;
  
  error = FT_New_Face (_pango_clutter_font_map_get_library (fcfont->fontmap),
		       (char *) filename2, id, &font->face);
  
  if (error)
    {
    bail1:
      name = pango_font_description_to_string (fcfont->description);
      g_warning ("Unable to open font file %s for font %s, exiting\n", 
		 filename2, name);
      exit (1);
    }
  else
    {
      name = pango_font_description_to_string (fcfont->description);
      g_warning ("Unable to open font file %s for font %s, falling back to %s\n", original_file, name, filename2);
      g_free (name);
    }

  FcPatternDestroy (sans);
  FcPatternDestroy (matched);
}

static void
set_transform (PangoClutterFont *font)
{
  PangoFcFont *fcfont = (PangoFcFont *)font;
  FcMatrix    *fc_matrix;

  if (FcPatternGetMatrix (fcfont->font_pattern, 
			  FC_MATRIX, 
			  0, 
			  &fc_matrix) == FcResultMatch)
    {
      FT_Matrix ft_matrix;
      
      ft_matrix.xx = 0x10000L * fc_matrix->xx;
      ft_matrix.yy = 0x10000L * fc_matrix->yy;
      ft_matrix.xy = 0x10000L * fc_matrix->xy;
      ft_matrix.yx = 0x10000L * fc_matrix->yx;

      FT_Set_Transform (font->face, &ft_matrix, NULL);
    }
}

FT_Face
pango_clutter_font_get_face (PangoFont *font)
{
  PangoClutterFont *glfont = (PangoClutterFont *)font;
  PangoFcFont      *fcfont = (PangoFcFont *)font;
  FcPattern        *pattern;
  FcChar8          *filename;
  FcBool            antialias, hinting, autohint;
  FT_Error          error;
  int id;

  pattern = fcfont->font_pattern;

  if (!glfont->face)
    {
      glfont->load_flags = 0;

      /* disable antialiasing if requested */
      if (FcPatternGetBool (pattern, FC_ANTIALIAS, 
			    0, &antialias) != FcResultMatch)
	antialias = FcTrue;

      glfont->load_flags |= FT_LOAD_NO_BITMAP;

      /* disable hinting if requested */
      if (FcPatternGetBool (pattern, FC_HINTING, 
			    0, &hinting) != FcResultMatch)
	hinting = FcTrue;

      if (!hinting)
        glfont->load_flags |= FT_LOAD_NO_HINTING;

      /* force autohinting if requested */
      if (FcPatternGetBool (pattern, FC_AUTOHINT, 
			    0, &autohint) != FcResultMatch)
	autohint = FcFalse;

      if (autohint)
        glfont->load_flags |= FT_LOAD_FORCE_AUTOHINT;

      if (FcPatternGetString (pattern, FC_FILE, 0, &filename) != FcResultMatch)
	goto bail0;
      
      if (FcPatternGetInteger (pattern, FC_INDEX, 0, &id) != FcResultMatch)
	goto bail0;

      error 
	= FT_New_Face (_pango_clutter_font_map_get_library (fcfont->fontmap),
		       (char *)filename, id, &glfont->face);

      if (error != FT_Err_Ok)
	{
	bail0:
	  load_fallback_face (glfont, (char *)filename);
	}

      g_assert (glfont->face);

      set_transform (glfont);

      error = FT_Set_Char_Size (glfont->face,
				PANGO_PIXELS_26_6 (glfont->size),
				PANGO_PIXELS_26_6 (glfont->size),
				0, 0);
      if (error)
	g_warning ("Error in FT_Set_Char_Size: %d", error);
    }
  
  return glfont->face;
}

G_DEFINE_TYPE (PangoClutterFont, pango_clutter_font, PANGO_TYPE_FC_FONT)

static void 
pango_clutter_font_init (PangoClutterFont *font)
{
  font->face = NULL;
  font->size = 0;
  font->glyph_info = g_hash_table_new (NULL, NULL);
}

static void
pango_clutter_font_class_init (PangoClutterFontClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PangoFontClass *font_class = PANGO_FONT_CLASS (class);
  PangoFcFontClass *fc_font_class = PANGO_FC_FONT_CLASS (class);
  
  object_class->finalize        = pango_clutter_font_finalize;
  
  font_class->get_glyph_extents = pango_clutter_font_get_glyph_extents;
  
  fc_font_class->lock_face      = pango_clutter_font_real_lock_face;
  fc_font_class->unlock_face    = pango_clutter_font_real_unlock_face;
}

static PangoClutterGlyphInfo *
pango_clutter_font_get_glyph_info (PangoFont *font_, 
				   PangoGlyph glyph, 
				   gboolean   create)
{
  PangoClutterFont *font = (PangoClutterFont *)font_;
  PangoFcFont *fcfont = (PangoFcFont *)font;
  PangoClutterGlyphInfo *info;

  info = g_hash_table_lookup (font->glyph_info, GUINT_TO_POINTER (glyph));

  if ((info == NULL) && create)
    {
      info = g_slice_new0 (PangoClutterGlyphInfo);

      pango_fc_font_get_raw_extents (fcfont, font->load_flags,
				     glyph,
				     &info->ink_rect,
				     &info->logical_rect);

      g_hash_table_insert (font->glyph_info, GUINT_TO_POINTER(glyph), info);
    }

  return info;
}

PangoGlyph
pango_clutter_get_unknown_glyph (PangoFont *font)
{
  FT_Face face = pango_clutter_font_get_face (font);

  if (face && FT_IS_SFNT (face))
    /* TrueType fonts have an 'unknown glyph' box on glyph index 0 */
    return 0;
  else
    return PANGO_GLYPH_EMPTY;
}

static void
pango_clutter_font_get_glyph_extents (PangoFont      *font,
				     PangoGlyph      glyph,
				     PangoRectangle *ink_rect,
				     PangoRectangle *logical_rect)
{
  PangoClutterGlyphInfo *info;

  if (glyph == PANGO_GLYPH_EMPTY)
    {
      if (ink_rect)
	ink_rect->x = ink_rect->y = 
	  ink_rect->height = ink_rect->width = 0;
      if (logical_rect)
	logical_rect->x = logical_rect->y = 
	  logical_rect->height = logical_rect->width = 0;
      return;
    }

  if (glyph & PANGO_GLYPH_UNKNOWN_FLAG)
    {
      glyph = pango_clutter_get_unknown_glyph (font);
      if (glyph == PANGO_GLYPH_EMPTY)
        {
	  /* No unknown glyph found for the font, draw a box */
	  PangoFontMetrics *metrics = pango_font_get_metrics (font, NULL);

	  if (metrics)
	    {
	      if (ink_rect)
		{
		  ink_rect->x = PANGO_SCALE;
		  ink_rect->width = metrics->approximate_char_width - 2 * PANGO_SCALE;
		  ink_rect->y = - (metrics->ascent - PANGO_SCALE);
		  ink_rect->height = metrics->ascent + metrics->descent - 2 * PANGO_SCALE;
		}
	      if (logical_rect)
		{
		  logical_rect->x = 0;
		  logical_rect->width = metrics->approximate_char_width;
		  logical_rect->y = -metrics->ascent;
		  logical_rect->height = metrics->ascent + metrics->descent;
		}

	      pango_font_metrics_unref (metrics);
	    }
	  else
	    {
	      if (ink_rect)
		ink_rect->x = ink_rect->y = 
		  ink_rect->height = ink_rect->width = 0;
	      if (logical_rect)
		logical_rect->x = logical_rect->y = 
		  logical_rect->height = logical_rect->width = 0;
	    }
	  return;
	}
    }

  info = pango_clutter_font_get_glyph_info (font, glyph, TRUE);

  if (ink_rect)
    *ink_rect = info->ink_rect;
  if (logical_rect)
    *logical_rect = info->logical_rect;
}

int
pango_clutter_font_get_kerning (PangoFont *font,
				PangoGlyph left,
				PangoGlyph right)
{
  PangoFcFont *fc_font = PANGO_FC_FONT (font);
  
  FT_Face face;
  FT_Error error;
  FT_Vector kerning;

  face = pango_fc_font_lock_face (fc_font);
  if (!face)
    return 0;

  if (!FT_HAS_KERNING (face))
    {
      pango_fc_font_unlock_face (fc_font);
      return 0;
    }

  error = FT_Get_Kerning (face, left, right, ft_kerning_default, &kerning);
  if (error != FT_Err_Ok)
    {
      pango_fc_font_unlock_face (fc_font);
      return 0;
    }

  pango_fc_font_unlock_face (fc_font);
  return PANGO_UNITS_26_6 (kerning.x);
}

static FT_Face
pango_clutter_font_real_lock_face (PangoFcFont *font)
{
  return pango_clutter_font_get_face ((PangoFont *)font);
}

static void
pango_clutter_font_real_unlock_face (PangoFcFont *font)
{
}

static gboolean
pango_clutter_free_glyph_info_callback (gpointer key, 
					gpointer value, 
					gpointer data)
{
  PangoClutterFont *font = PANGO_CLUTTER_FONT (data);
  PangoClutterGlyphInfo *info = value;
  
  if (font->glyph_cache_destroy && info->cached_glyph)
    (*font->glyph_cache_destroy) (info->cached_glyph);

  g_slice_free (PangoClutterGlyphInfo, info);
  return TRUE;
}

static void
pango_clutter_font_finalize (GObject *object)
{
  PangoClutterFont *font = (PangoClutterFont *)object;

  if (font->face)
    {
      FT_Done_Face (font->face);
      font->face = NULL;
    }

  g_hash_table_foreach_remove (font->glyph_info, 
			       pango_clutter_free_glyph_info_callback, object);
  g_hash_table_destroy (font->glyph_info);
  
  G_OBJECT_CLASS (pango_clutter_font_parent_class)->finalize (object);
}

PangoCoverage*
pango_clutter_font_get_coverage (PangoFont *font, PangoLanguage *language)
{
  return pango_font_get_coverage (font, language);
}

void*
_pango_clutter_font_get_cache_glyph_data (PangoFont *font, int glyph_index)
{
  PangoClutterGlyphInfo *info;

  info = pango_clutter_font_get_glyph_info (font, glyph_index, FALSE);

  return info ? info->cached_glyph : 0;
}

void
_pango_clutter_font_set_cache_glyph_data (PangoFont *font, 
					  int        glyph_index, 
					  void      *cached_glyph)
{
  PangoClutterGlyphInfo *info;

  info = pango_clutter_font_get_glyph_info (font, glyph_index, TRUE);

  info->cached_glyph = cached_glyph;
}

void
_pango_clutter_font_set_glyph_cache_destroy (PangoFont     *font, 
					     GDestroyNotify destroy_notify)
{
  PANGO_CLUTTER_FONT (font)->glyph_cache_destroy = destroy_notify;
}
