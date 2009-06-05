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
#include "cogl-pango-display-list.h"

struct _CoglPangoRenderer
{
  PangoRenderer parent_instance;

  /* The material used to texture from the glyph cache with */
  CoglHandle glyph_material;
  /* The material used for solid fills. (boxes, rectangles + trapezoids) */
  CoglHandle solid_material;

  /* Caches of glyphs as textures */
  CoglPangoGlyphCache *glyph_cache;

  /* The current display list that is being built */
  CoglPangoDisplayList *display_list;
};

struct _CoglPangoRendererClass
{
  PangoRendererClass class_instance;
};

typedef struct _CoglPangoRendererQdata CoglPangoRendererQdata;

/* An instance of this struct gets attached to each PangoLayout to
   cache the VBO and to detect changes to the layout */
struct _CoglPangoRendererQdata
{
  /* The cache of the geometry for the layout */
  CoglPangoDisplayList *display_list;
  /* A reference to the first line of the layout. This is just used to
     detect changes */
  PangoLayoutLine *first_line;
};

static void
cogl_pango_renderer_draw_glyph (CoglPangoRenderer        *priv,
                                CoglPangoGlyphCacheValue *cache_value,
                                float                     x1,
                                float                     y1)
{
  float x2, y2;

  g_return_if_fail (priv->display_list != NULL);

  x2 = x1 + (float) cache_value->draw_width;
  y2 = y1 + (float) cache_value->draw_height;

  _cogl_pango_display_list_add_texture (priv->display_list,
                                        cache_value->texture,
                                        x1, y1, x2, y2,
                                        cache_value->tx1,
                                        cache_value->ty1,
                                        cache_value->tx2,
                                        cache_value->ty2);
}

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

G_DEFINE_TYPE (CoglPangoRenderer, cogl_pango_renderer, PANGO_TYPE_RENDERER);

static void
cogl_pango_renderer_init (CoglPangoRenderer *priv)
{
  priv->glyph_material = cogl_material_new ();

  /* The default combine mode of materials is to modulate (A x B) the texture
   * RGBA channels with the RGBA channels of the previous layer (which in our
   * case is just the solid font color)
   *
   * Since our glyph cache textures are component alpha textures, and so the
   * RGB channels are defined as (0, 0, 0) we don't want to modulate the RGB
   * channels, instead we want to simply replace them with our solid font
   * color...
   */
  cogl_material_set_layer_combine (priv->glyph_material, 0, /* layer */
                                   "RGB = REPLACE (PREVIOUS)"
                                   "A = MODULATE (PREVIOUS, TEXTURE)",
                                   NULL);

  priv->solid_material = cogl_material_new ();

  priv->glyph_cache = cogl_pango_glyph_cache_new ();

  _cogl_pango_renderer_set_use_mipmapping (priv, FALSE);
}

static void
cogl_pango_renderer_class_init (CoglPangoRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);

  object_class->finalize = cogl_pango_renderer_finalize;

  renderer_class->draw_glyphs = cogl_pango_renderer_draw_glyphs;
  renderer_class->draw_rectangle = cogl_pango_renderer_draw_rectangle;
  renderer_class->draw_trapezoid = cogl_pango_renderer_draw_trapezoid;
}

static void
cogl_pango_renderer_finalize (GObject *object)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (object);

  cogl_pango_glyph_cache_free (priv->glyph_cache);

  G_OBJECT_CLASS (cogl_pango_renderer_parent_class)->finalize (object);
}

static CoglPangoRenderer *
cogl_pango_get_renderer_from_context (PangoContext *context)
{
  PangoFontMap      *font_map;
  PangoRenderer     *renderer;
  CoglPangoFontMap  *font_map_priv;

  font_map = pango_context_get_font_map (context);
  g_return_val_if_fail (COGL_PANGO_IS_FONT_MAP (font_map), NULL);

  font_map_priv = COGL_PANGO_FONT_MAP (font_map);
  renderer = cogl_pango_font_map_get_renderer (font_map_priv);
  g_return_val_if_fail (COGL_PANGO_IS_RENDERER (renderer), NULL);

  return COGL_PANGO_RENDERER (renderer);
}

static GQuark
cogl_pango_render_get_qdata_key (void)
{
  static GQuark key = 0;

  if (G_UNLIKELY (key == 0))
    key = g_quark_from_static_string ("CoglPangoDisplayList");

  return key;
}

static void
cogl_pango_render_qdata_destroy (CoglPangoRendererQdata *qdata)
{
  if (qdata->display_list)
    _cogl_pango_display_list_free (qdata->display_list);
  if (qdata->first_line)
    pango_layout_line_unref (qdata->first_line);
  g_slice_free (CoglPangoRendererQdata, qdata);
}

/**
 * cogl_pango_render_layout_subpixel:
 * @layout: a #PangoLayout
 * @x: FIXME
 * @y: FIXME
 * @color: color to use when rendering the layout
 * @flags: flags to pass to the renderer
 *
 * FIXME
 *
 * Since: 1.0
 */
void
cogl_pango_render_layout_subpixel (PangoLayout     *layout,
				   int              x,
				   int              y,
				   const CoglColor *color,
				   int              flags)
{
  PangoContext           *context;
  CoglPangoRenderer      *priv;
  CoglPangoRendererQdata *qdata;

  context = pango_layout_get_context (layout);
  priv = cogl_pango_get_renderer_from_context (context);
  if (G_UNLIKELY (!priv))
    return;

  qdata = g_object_get_qdata (G_OBJECT (layout),
                              cogl_pango_render_get_qdata_key ());

  if (qdata == NULL)
    {
      qdata = g_slice_new0 (CoglPangoRendererQdata);
      g_object_set_qdata_full (G_OBJECT (layout),
                               cogl_pango_render_get_qdata_key (),
                               qdata,
                               (GDestroyNotify)
                               cogl_pango_render_qdata_destroy);
    }

  /* Check if the layout has changed since the last build of the
     display list. This trick was suggested by Behdad Esfahbod here:
     http://mail.gnome.org/archives/gtk-i18n-list/2009-May/msg00019.html */
  if (qdata->display_list && qdata->first_line
      && qdata->first_line->layout != layout)
    {
      _cogl_pango_display_list_free (qdata->display_list);
      qdata->display_list = NULL;
    }

  if (qdata->display_list == NULL)
    {
      qdata->display_list = _cogl_pango_display_list_new ();

      priv->display_list = qdata->display_list;
      pango_renderer_draw_layout (PANGO_RENDERER (priv), layout, 0, 0);
      priv->display_list = NULL;
    }

  cogl_push_matrix ();
  cogl_translate (x / (gfloat) PANGO_SCALE, y / (gfloat) PANGO_SCALE, 0);
  _cogl_pango_display_list_render (qdata->display_list,
                                   color,
                                   priv->glyph_material,
                                   priv->solid_material);
  cogl_pop_matrix ();

  /* Keep a reference to the first line of the layout so we can detect
     changes */
  if (qdata->first_line)
    {
      pango_layout_line_unref (qdata->first_line);
      qdata->first_line = NULL;
    }
  if (pango_layout_get_line_count (layout) > 0)
    {
      qdata->first_line = pango_layout_get_line (layout, 0);
      pango_layout_line_ref (qdata->first_line);
    }
}

/**
 * cogl_pango_render_layout:
 * @layout: a #PangoLayout
 * @x: X coordinate to render the layout at
 * @y: Y coordinate to render the layout at
 * @color: color to use when rendering the layout
 * @flags: flags to pass to the renderer
 *
 * Renders @layout.
 *
 * Since: 1.0
 */
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
 */
void
cogl_pango_render_layout_line (PangoLayoutLine *line,
                               int              x,
                               int              y,
                               const CoglColor *color)
{
  PangoContext      *context;
  CoglPangoRenderer *priv;

  context = pango_layout_get_context (line->layout);
  priv = cogl_pango_get_renderer_from_context (context);
  if (G_UNLIKELY (!priv))
    return;

  priv->display_list = _cogl_pango_display_list_new ();

  pango_renderer_draw_layout_line (PANGO_RENDERER (priv), line, x, y);

  _cogl_pango_display_list_render (priv->display_list,
                                   color,
                                   priv->glyph_material,
                                   priv->solid_material);

  _cogl_pango_display_list_free (priv->display_list);
  priv->display_list = NULL;
}

void
_cogl_pango_renderer_clear_glyph_cache (CoglPangoRenderer *renderer)
{
  cogl_pango_glyph_cache_clear (renderer->glyph_cache);
}

void
_cogl_pango_renderer_set_use_mipmapping (CoglPangoRenderer *renderer,
                                         gboolean value)
{
  if (value)
    cogl_material_set_layer_filters (renderer->glyph_material, 0,
                                     COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR,
                                     COGL_MATERIAL_FILTER_LINEAR);
  else
    cogl_material_set_layer_filters (renderer->glyph_material, 0,
                                     COGL_MATERIAL_FILTER_LINEAR,
                                     COGL_MATERIAL_FILTER_LINEAR);
}

gboolean
_cogl_pango_renderer_get_use_mipmapping (CoglPangoRenderer *renderer)
{
  const GList *layers = cogl_material_get_layers (renderer->glyph_material);

  g_return_val_if_fail (layers != NULL, FALSE);

  return (cogl_material_layer_get_min_filter (layers->data)
          == COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR);
}

static CoglPangoGlyphCacheValue *
cogl_pango_renderer_get_cached_glyph (PangoRenderer *renderer,
                                      PangoFont     *font,
                                      PangoGlyph     glyph)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (renderer);
  CoglPangoGlyphCacheValue *value;

  value = cogl_pango_glyph_cache_lookup (priv->glyph_cache, font, glyph);
  if (value == NULL)
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
      value =
        cogl_pango_glyph_cache_set (priv->glyph_cache, font, glyph,
                                    cairo_image_surface_get_data (surface),
                                    cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    cairo_image_surface_get_stride (surface),
                                    ink_rect.x, ink_rect.y);

      cairo_surface_destroy (surface);

      COGL_NOTE (PANGO, "cache fail    %i", glyph);
    }
  else
    COGL_NOTE (PANGO, "cache success %i", glyph);

  return value;
}

void
cogl_pango_ensure_glyph_cache_for_layout (PangoLayout *layout)
{
  PangoContext    *context;
  PangoRenderer   *renderer;
  PangoLayoutIter *iter;

  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  if ((iter = pango_layout_get_iter (layout)) == NULL)
    return;

  context = pango_layout_get_context (layout);
  renderer =
    PANGO_RENDERER (cogl_pango_get_renderer_from_context (context));

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

  if (pango_color)
    {
      CoglColor color;

      cogl_color_set_from_4ub (&color,
                               pango_color->red >> 8,
                               pango_color->green >> 8,
                               pango_color->blue >> 8,
                               0xff);

      _cogl_pango_display_list_set_color_override (priv->display_list, &color);
    }
  else
    _cogl_pango_display_list_remove_color_override (priv->display_list);
}

static void
cogl_pango_renderer_draw_box (PangoRenderer *renderer,
                              int            x,
                              int            y,
                              int            width,
                              int            height)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (renderer);

  g_return_if_fail (priv->display_list != NULL);

  _cogl_pango_display_list_add_rectangle (priv->display_list,
                                          x,
                                          y - height,
                                          x + width,
                                          y);
}

static void
cogl_pango_renderer_get_device_units (PangoRenderer *renderer,
                                      int            xin,
                                      int            yin,
                                      float     *xout,
                                      float     *yout)
{
  const PangoMatrix *matrix;

  if ((matrix = pango_renderer_get_matrix (renderer)))
    {
      /* Convert user-space coords to device coords */
      *xout =  ((xin * matrix->xx + yin * matrix->xy)
				     / PANGO_SCALE + matrix->x0);
      *yout =  ((yin * matrix->yy + xin * matrix->yx)
				     / PANGO_SCALE + matrix->y0);
    }
  else
    {
      *xout = PANGO_PIXELS (xin);
      *yout = PANGO_PIXELS (yin);
    }
}

static void
cogl_pango_renderer_draw_rectangle (PangoRenderer   *renderer,
                                    PangoRenderPart  part,
                                    int              x,
                                    int              y,
                                    int              width,
                                    int              height)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (renderer);
  float x1, x2, y1, y2;

  g_return_if_fail (priv->display_list != NULL);

  cogl_pango_renderer_set_color_for_part (renderer, part);

  cogl_pango_renderer_get_device_units (renderer,
                                        x, y,
                                        &x1, &y1);
  cogl_pango_renderer_get_device_units (renderer,
                                        x + width, y + height,
                                        &x2, &y2);

  _cogl_pango_display_list_add_rectangle (priv->display_list,
                                          x1, y1, x2, y2);
}

static void
cogl_pango_renderer_draw_trapezoid (PangoRenderer   *renderer,
				    PangoRenderPart  part,
				    double           y1,
				    double           x11,
				    double           x21,
				    double           y2,
				    double           x12,
				    double           x22)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (renderer);
  float points[8];

  g_return_if_fail (priv->display_list != NULL);

  points[0] =  (x11);
  points[1] =  (y1);
  points[2] =  (x12);
  points[3] =  (y2);
  points[4] =  (x22);
  points[5] = points[3];
  points[6] =  (x21);
  points[7] = points[1];

  cogl_pango_renderer_set_color_for_part (renderer, part);

  _cogl_pango_display_list_add_trapezoid (priv->display_list,
                                          y1,
                                          x11,
                                          x21,
                                          y2,
                                          x12,
                                          x22);
}

static void
cogl_pango_renderer_draw_glyphs (PangoRenderer    *renderer,
				 PangoFont        *font,
				 PangoGlyphString *glyphs,
				 int               xi,
				 int               yi)
{
  CoglPangoRenderer *priv = (CoglPangoRenderer *) renderer;
  CoglPangoGlyphCacheValue *cache_value;
  int i;

  cogl_pango_renderer_set_color_for_part (renderer,
					  PANGO_RENDER_PART_FOREGROUND);

  for (i = 0; i < glyphs->num_glyphs; i++)
    {
      PangoGlyphInfo *gi = glyphs->glyphs + i;
      float x, y;

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
	      cogl_pango_renderer_draw_box (renderer,
                                            x,
                                            y,
                                            PANGO_UNKNOWN_GLYPH_WIDTH,
                                            PANGO_UNKNOWN_GLYPH_HEIGHT);
            }
	  else
	    {
	      cogl_pango_renderer_draw_box (renderer,
                                            x,
					    y,
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
            cogl_pango_renderer_draw_box (renderer,
                                          x,
                                          y,
                                          PANGO_UNKNOWN_GLYPH_WIDTH,
                                          PANGO_UNKNOWN_GLYPH_HEIGHT);
	  else
	    {
              float width, height;

	      x += (float)(cache_value->draw_x);
	      y += (float)(cache_value->draw_y);

              width = x + (float)(cache_value->draw_width);
              height = y + (float)(cache_value->draw_height);

              cogl_pango_renderer_draw_glyph (priv, cache_value, x, y);
	    }
	}

      xi += gi->geometry.width;
    }
}
