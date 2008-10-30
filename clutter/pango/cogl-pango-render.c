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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_BACKEND 1
#endif

#include <pango/pango-fontmap.h>
#include <pango/pangocairo.h>
#include <pango/pango-renderer.h>
#include <cairo.h>

#include "cogl-pango-private.h"
#include "cogl-pango-glyph-cache.h"
#include "../clutter-debug.h"

struct _CoglPangoRenderer
{
  PangoRenderer parent_instance;

  /* The color to draw the glyphs with */
  CoglColor color;

  /* Two caches of glyphs as textures, one with mipmapped textures and
     one without */
  CoglPangoGlyphCache *glyph_cache;
  CoglPangoGlyphCache *mipmapped_glyph_cache;

  gboolean use_mipmapping;
};

struct _CoglPangoRendererClass
{
  PangoRendererClass class_instance;
};

#define COGL_PANGO_UNIT_TO_FIXED(x) ((x) << (COGL_FIXED_Q - 10))

static void cogl_pango_renderer_finalize (GObject *object);
static void cogl_pango_renderer_draw_glyphs (PangoRenderer    *renderer,
						PangoFont        *font,
						PangoGlyphString *glyphs,
						int               x,
						int               y);
static void cogl_pango_renderer_draw_rectangle (PangoRenderer    *renderer,
						   PangoRenderPart   part,
						   int               x,
						   int               y,
						   int               width,
						   int               height);
static void cogl_pango_renderer_draw_trapezoid (PangoRenderer    *renderer,
						   PangoRenderPart   part,
						   double            y1,
						   double            x11,
						   double            x21,
						   double            y2,
						   double            x12,
						   double            x22);

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (CoglPangoRenderer, cogl_pango_renderer,
	       PANGO_TYPE_RENDERER);

static void
cogl_pango_renderer_init (CoglPangoRenderer *priv)
{
  priv->glyph_cache = cogl_pango_glyph_cache_new (FALSE);
  priv->mipmapped_glyph_cache = cogl_pango_glyph_cache_new (TRUE);
  priv->use_mipmapping = FALSE;
}

static void
cogl_pango_renderer_class_init (CoglPangoRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = cogl_pango_renderer_finalize;

  renderer_class->draw_glyphs = cogl_pango_renderer_draw_glyphs;
  renderer_class->draw_rectangle = cogl_pango_renderer_draw_rectangle;
  renderer_class->draw_trapezoid = cogl_pango_renderer_draw_trapezoid;
}

static void
cogl_pango_renderer_finalize (GObject *object)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (object);

  cogl_pango_glyph_cache_free (priv->mipmapped_glyph_cache);
  cogl_pango_glyph_cache_free (priv->glyph_cache);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
cogl_pango_render_layout_subpixel (PangoLayout     *layout,
				      int              x,
				      int              y,
				      const CoglColor *color,
				      int              flags)
{
  PangoContext         *context;
  PangoFontMap         *font_map;
  PangoRenderer        *renderer;
  CoglPangoRenderer *priv;

  context = pango_layout_get_context (layout);
  font_map = pango_context_get_font_map (context);
  g_return_if_fail (COGL_PANGO_IS_FONT_MAP (font_map));
  renderer = cogl_pango_font_map_get_renderer
    (COGL_PANGO_FONT_MAP (font_map));
  priv = COGL_PANGO_RENDERER (renderer);

  priv->color = *color;

  pango_renderer_draw_layout (renderer, layout, x, y);
}

void
cogl_pango_render_layout (PangoLayout     *layout,
			     int              x,
			     int              y,
			     const CoglColor *color,
			     int              flags)
{
  cogl_pango_render_layout_subpixel (layout,
                                        x * PANGO_SCALE,
                                        y * PANGO_SCALE,
                                        color,
                                        flags);
}

void
cogl_pango_render_layout_line (PangoLayoutLine *line,
				  int              x,
				  int              y,
				  const CoglColor *color)
{
  PangoContext         *context;
  PangoFontMap         *font_map;
  PangoRenderer        *renderer;
  CoglPangoRenderer *priv;

  context = pango_layout_get_context (line->layout);
  font_map = pango_context_get_font_map (context);
  g_return_if_fail (COGL_PANGO_IS_FONT_MAP (font_map));
  renderer = cogl_pango_font_map_get_renderer
    (COGL_PANGO_FONT_MAP (font_map));
  priv = COGL_PANGO_RENDERER (renderer);

  priv->color = *color;

  pango_renderer_draw_layout_line (renderer, line, x, y);
}

void
_cogl_pango_renderer_clear_glyph_cache (CoglPangoRenderer *renderer)
{
  cogl_pango_glyph_cache_clear (renderer->glyph_cache);
  cogl_pango_glyph_cache_clear (renderer->mipmapped_glyph_cache);
}

void
_cogl_pango_renderer_set_use_mipmapping (CoglPangoRenderer *renderer,
					    gboolean value)
{
  renderer->use_mipmapping = value;
}

gboolean
_cogl_pango_renderer_get_use_mipmapping (CoglPangoRenderer *renderer)
{
  return renderer->use_mipmapping;
}

static CoglPangoGlyphCacheValue *
cogl_pango_renderer_get_cached_glyph (PangoRenderer *renderer,
					 PangoFont     *font,
					 PangoGlyph     glyph)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (renderer);
  CoglPangoGlyphCacheValue *value;
  CoglPangoGlyphCache *glyph_cache;

  glyph_cache = priv->use_mipmapping
    ? priv->mipmapped_glyph_cache : priv->glyph_cache;

  if ((value = cogl_pango_glyph_cache_lookup (glyph_cache,
						 font,
						 glyph)) == NULL)
    {
      cairo_surface_t *surface;
      cairo_t *cr;
      cairo_scaled_font_t *scaled_font;
      PangoRectangle ink_rect;
      cairo_glyph_t cairo_glyph;

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      surface = cairo_image_surface_create (CAIRO_FORMAT_A8,
					    ink_rect.width,
					    ink_rect.height);
      cr = cairo_create (surface);

      scaled_font = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
      cairo_set_scaled_font (cr, scaled_font);

      cairo_glyph.x = -ink_rect.x;
      cairo_glyph.y = -ink_rect.y;
      /* The PangoCairo glyph numbers directly map to Cairo glyph
	 numbers */
      cairo_glyph.index = glyph;
      cairo_show_glyphs (cr, &cairo_glyph, 1);

      cairo_destroy (cr);
      cairo_surface_flush (surface);

      /* Copy the glyph to the cache */
      value = cogl_pango_glyph_cache_set
	(glyph_cache, font, glyph,
	 cairo_image_surface_get_data (surface),
	 cairo_image_surface_get_width (surface),
	 cairo_image_surface_get_height (surface),
	 cairo_image_surface_get_stride (surface),
	 ink_rect.x, ink_rect.y);

      cairo_surface_destroy (surface);

      CLUTTER_NOTE (PANGO, "cache fail    %i", glyph);
    }
  else
    CLUTTER_NOTE (PANGO, "cache success %i", glyph);

  return value;
}

void
cogl_pango_ensure_glyph_cache_for_layout (PangoLayout *layout)
{
  PangoContext    *context;
  PangoFontMap    *fontmap;
  PangoRenderer   *renderer;
  PangoLayoutIter *iter;
 
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
 
  context = pango_layout_get_context (layout);
  fontmap = pango_context_get_font_map (context);
  g_return_if_fail (COGL_PANGO_IS_FONT_MAP (fontmap));
  renderer = cogl_pango_font_map_get_renderer
    (COGL_PANGO_FONT_MAP (fontmap));
 
  if ((iter = pango_layout_get_iter (layout)) == NULL)
    return;
 
  do
    {
      PangoLayoutLine *line;
      GSList *l;
 
      line = pango_layout_iter_get_line_readonly (iter);
 
      for (l = line->runs; l; l = l->next)
        {
          PangoLayoutRun *run = l->data;
          PangoGlyphString *glyphs = run->glyphs;
	  int i;
 
          for (i = 0; i < glyphs->num_glyphs; i++)
            {
              PangoGlyphInfo *gi = &glyphs->glyphs[i];
 
	      cogl_pango_renderer_get_cached_glyph (renderer,
						       run->item->analysis.font,
						       gi->glyph);
            }
        }
    }
  while (pango_layout_iter_next_line (iter));
 
  pango_layout_iter_free (iter);
}

static void
cogl_pango_renderer_set_color_for_part (PangoRenderer   *renderer,
					   PangoRenderPart  part)
{
  PangoColor *pango_color = pango_renderer_get_color (renderer, part);
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (renderer);
  CoglColor color;

  if (pango_color)
    {
      cogl_color_set_from_4ub (&color,
                               pango_color->red >> 8,
                               pango_color->green >> 8,
                               pango_color->blue >> 8,
                               cogl_color_get_alpha_byte (&priv->color));
    }
  else
    color = priv->color;

  cogl_color (&color);
}

static void
cogl_pango_renderer_draw_box (int x,     int y,
                              int width, int height)
{
  cogl_path_rectangle (COGL_FIXED_FROM_INT (x),
		       COGL_FIXED_FROM_INT (y - height),
		       COGL_FIXED_FROM_INT (width),
		       COGL_FIXED_FROM_INT (height));
  cogl_path_stroke ();
}

static void
cogl_pango_renderer_get_device_units (PangoRenderer *renderer,
					 int            xin,
					 int            yin,
					 CoglFixed     *xout,
					 CoglFixed     *yout)
{
  const PangoMatrix *matrix;

  if ((matrix = pango_renderer_get_matrix (renderer)))
    {
      /* Convert user-space coords to device coords */
      *xout = COGL_FIXED_FROM_FLOAT ((xin * matrix->xx + yin * matrix->xy)
				     / PANGO_SCALE + matrix->x0);
      *yout = COGL_FIXED_FROM_FLOAT ((yin * matrix->yy + xin * matrix->yx)
				     / PANGO_SCALE + matrix->y0);
    }
  else
    {
      *xout = COGL_PANGO_UNIT_TO_FIXED (xin);
      *yout = COGL_PANGO_UNIT_TO_FIXED (yin);
    }
}

static void
cogl_pango_renderer_draw_rectangle (PangoRenderer    *renderer,
				       PangoRenderPart   part,
				       int               x,
				       int               y,
				       int               width,
				       int               height)
{
  CoglFixed x1, x2, y1, y2;

  cogl_pango_renderer_set_color_for_part (renderer, part);

  cogl_pango_renderer_get_device_units (renderer, x, y,
					   &x1, &y1);
  cogl_pango_renderer_get_device_units (renderer, x + width, y + height,
					   &x2, &y2);

  cogl_rectanglex (x1, y1, x2 - x1, y2 - y1);
}

static void
cogl_pango_renderer_draw_trapezoid (PangoRenderer    *renderer,
				       PangoRenderPart   part,
				       double            y1,
				       double            x11,
				       double            x21,
				       double            y2,
				       double            x12,
				       double            x22)
{
  CoglFixed points[8];

  points[0] = COGL_FIXED_FROM_FLOAT (x11);
  points[1] = COGL_FIXED_FROM_FLOAT (y1);
  points[2] = COGL_FIXED_FROM_FLOAT (x12);
  points[3] = COGL_FIXED_FROM_FLOAT (y2);
  points[4] = COGL_FIXED_FROM_FLOAT (x22);
  points[5] = points[3];
  points[6] = COGL_FIXED_FROM_FLOAT (x21);
  points[7] = points[1];

  cogl_pango_renderer_set_color_for_part (renderer, part);

  cogl_path_polygon (points, 4);
  cogl_path_fill ();
}

static void
cogl_pango_renderer_draw_glyphs (PangoRenderer    *renderer,
				    PangoFont        *font,
				    PangoGlyphString *glyphs,
				    int               xi,
				    int               yi)
{
  CoglPangoGlyphCacheValue *cache_value;
  int i;

  cogl_pango_renderer_set_color_for_part (renderer,
					  PANGO_RENDER_PART_FOREGROUND);

  for (i = 0; i < glyphs->num_glyphs; i++)
    {
      PangoGlyphInfo *gi = glyphs->glyphs + i;
      CoglFixed x, y;

      cogl_pango_renderer_get_device_units (renderer,
					    xi + gi->geometry.x_offset,
					    yi + gi->geometry.y_offset,
					    &x, &y);

      if ((gi->glyph & PANGO_GLYPH_UNKNOWN_FLAG))
	{
	  PangoFontMetrics *metrics;

	  if (font == NULL ||
              (metrics = pango_font_get_metrics (font, NULL)) == NULL)
            {
	      cogl_pango_renderer_draw_box (COGL_FIXED_TO_INT (x),
                                            COGL_FIXED_TO_INT (y),
                                            PANGO_UNKNOWN_GLYPH_WIDTH,
                                            PANGO_UNKNOWN_GLYPH_HEIGHT);
            }
	  else
	    {
	      cogl_pango_renderer_draw_box (COGL_FIXED_TO_INT (x),
					    COGL_FIXED_TO_INT (y),
					    metrics->approximate_char_width
					    / PANGO_SCALE,
					    metrics->ascent / PANGO_SCALE);

	      pango_font_metrics_unref (metrics);
	    }
	}
      else
	{
	  /* Get the texture containing the glyph. This will create
	     the cache entry if there isn't already one */
	  cache_value =
            cogl_pango_renderer_get_cached_glyph (renderer,
                                                  font,
                                                  gi->glyph);

	  if (cache_value == NULL)
	    cogl_pango_renderer_draw_box (COGL_FIXED_TO_INT (x),
					  COGL_FIXED_TO_INT (y),
					  PANGO_UNKNOWN_GLYPH_WIDTH,
					  PANGO_UNKNOWN_GLYPH_HEIGHT);
	  else
	    {
              CoglFixed width, height;

	      x += COGL_FIXED_FROM_INT (cache_value->draw_x);
	      y += COGL_FIXED_FROM_INT (cache_value->draw_y);

              width = x + COGL_FIXED_FROM_INT (cache_value->draw_width);
              height = y + COGL_FIXED_FROM_INT (cache_value->draw_height);

	      /* Render the glyph from the texture */
	      cogl_texture_rectangle (cache_value->texture,
                                      x, y,
                                      width, height,
				      cache_value->tx1, cache_value->ty1,
				      cache_value->tx2, cache_value->ty2);
	    }
	}

      xi += gi->geometry.width;
    }
}
