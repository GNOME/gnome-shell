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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 *   Matthew Allum  <mallum@openedhand.com>
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

#include "cogl/cogl-debug.h"
#include "cogl/cogl-context-private.h"
#include "cogl/cogl-texture-private.h"
#include "cogl-pango-private.h"
#include "cogl-pango-glyph-cache.h"
#include "cogl-pango-display-list.h"

enum
{
  PROP_0,

  PROP_COGL_CONTEXT,
  PROP_LAST
};

typedef struct
{
  CoglPangoGlyphCache *glyph_cache;
  CoglPangoPipelineCache *pipeline_cache;
} CoglPangoRendererCaches;

struct _CoglPangoRenderer
{
  PangoRenderer parent_instance;

  CoglContext *ctx;

  /* Two caches of glyphs as textures and their corresponding pipeline
     caches, one with mipmapped textures and one without */
  CoglPangoRendererCaches no_mipmap_caches;
  CoglPangoRendererCaches mipmap_caches;

  CoglBool use_mipmapping;

  /* The current display list that is being built */
  CoglPangoDisplayList *display_list;
};

struct _CoglPangoRendererClass
{
  PangoRendererClass class_instance;
};

typedef struct _CoglPangoLayoutQdata CoglPangoLayoutQdata;

/* An instance of this struct gets attached to each PangoLayout to
   cache the VBO and to detect changes to the layout */
struct _CoglPangoLayoutQdata
{
  CoglPangoRenderer *renderer;
  /* The cache of the geometry for the layout */
  CoglPangoDisplayList *display_list;
  /* A reference to the first line of the layout. This is just used to
     detect changes */
  PangoLayoutLine *first_line;
  /* Whether mipmapping was previously used to render this layout. We
     need to regenerate the display list if the mipmapping value is
     changed because it will be using a different set of textures */
  CoglBool mipmapping_used;
};

static void
_cogl_pango_ensure_glyph_cache_for_layout_line (PangoLayoutLine *line);

typedef struct
{
  CoglPangoDisplayList *display_list;
  float x1, y1, x2, y2;
} CoglPangoRendererSliceCbData;

PangoRenderer *
_cogl_pango_renderer_new (CoglContext *context)
{
  return PANGO_RENDERER (g_object_new (COGL_PANGO_TYPE_RENDERER,
                                       "context", context, NULL));
}

static void
cogl_pango_renderer_slice_cb (CoglTexture *texture,
                              const float *slice_coords,
                              const float *virtual_coords,
                              void *user_data)
{
  CoglPangoRendererSliceCbData *data = user_data;

  /* Note: this assumes that there is only one slice containing the
     whole texture and it doesn't attempt to split up the vertex
     coordinates based on the virtual_coords */

  _cogl_pango_display_list_add_texture (data->display_list,
                                        texture,
                                        data->x1,
                                        data->y1,
                                        data->x2,
                                        data->y2,
                                        slice_coords[0],
                                        slice_coords[1],
                                        slice_coords[2],
                                        slice_coords[3]);
}

static void
cogl_pango_renderer_draw_glyph (CoglPangoRenderer        *priv,
                                CoglPangoGlyphCacheValue *cache_value,
                                float                     x1,
                                float                     y1)
{
  CoglPangoRendererSliceCbData data;

  _COGL_RETURN_IF_FAIL (priv->display_list != NULL);

  data.display_list = priv->display_list;
  data.x1 = x1;
  data.y1 = y1;
  data.x2 = x1 + (float) cache_value->draw_width;
  data.y2 = y1 + (float) cache_value->draw_height;

  /* We iterate the internal sub textures of the texture so that we
     can get a pointer to the base texture even if the texture is in
     the global atlas. That way the display list can recognise that
     the neighbouring glyphs are coming from the same atlas and bundle
     them together into a single VBO */

  cogl_meta_texture_foreach_in_region (COGL_META_TEXTURE (cache_value->texture),
                                       cache_value->tx1,
                                       cache_value->ty1,
                                       cache_value->tx2,
                                       cache_value->ty2,
                                       COGL_PIPELINE_WRAP_MODE_REPEAT,
                                       COGL_PIPELINE_WRAP_MODE_REPEAT,
                                       cogl_pango_renderer_slice_cb,
                                       &data);
}

static void cogl_pango_renderer_dispose (GObject *object);
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
}

static void
_cogl_pango_renderer_constructed (GObject *gobject)
{
  CoglPangoRenderer *renderer = COGL_PANGO_RENDERER (gobject);
  CoglContext *ctx = renderer->ctx;

  renderer->no_mipmap_caches.pipeline_cache =
    _cogl_pango_pipeline_cache_new (ctx, FALSE);
  renderer->mipmap_caches.pipeline_cache =
    _cogl_pango_pipeline_cache_new (ctx, TRUE);

  renderer->no_mipmap_caches.glyph_cache =
    cogl_pango_glyph_cache_new (ctx, FALSE);
  renderer->mipmap_caches.glyph_cache =
    cogl_pango_glyph_cache_new (ctx, TRUE);

  _cogl_pango_renderer_set_use_mipmapping (renderer, FALSE);

  if (G_OBJECT_CLASS (cogl_pango_renderer_parent_class)->constructed)
    G_OBJECT_CLASS (cogl_pango_renderer_parent_class)->constructed (gobject);
}

static void
cogl_pango_renderer_set_property (GObject *object,
                                  unsigned int prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  CoglPangoRenderer *renderer = COGL_PANGO_RENDERER (object);

  switch (prop_id)
    {
    case PROP_COGL_CONTEXT:
      renderer->ctx = g_value_get_pointer (value);
      cogl_object_ref (renderer->ctx);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cogl_pango_renderer_class_init (CoglPangoRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);
  GParamSpec *pspec;

  object_class->set_property = cogl_pango_renderer_set_property;
  object_class->constructed = _cogl_pango_renderer_constructed;
  object_class->dispose = cogl_pango_renderer_dispose;
  object_class->finalize = cogl_pango_renderer_finalize;

  pspec = g_param_spec_pointer ("context",
                                "Context",
                                "The Cogl Context",
                                G_PARAM_WRITABLE |
                                G_PARAM_STATIC_STRINGS |
                                G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_property (object_class, PROP_COGL_CONTEXT, pspec);

  renderer_class->draw_glyphs = cogl_pango_renderer_draw_glyphs;
  renderer_class->draw_rectangle = cogl_pango_renderer_draw_rectangle;
  renderer_class->draw_trapezoid = cogl_pango_renderer_draw_trapezoid;
}

static void
cogl_pango_renderer_dispose (GObject *object)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (object);

  if (priv->ctx)
    {
      cogl_object_unref (priv->ctx);
      priv->ctx = NULL;
    }
}

static void
cogl_pango_renderer_finalize (GObject *object)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (object);

  cogl_pango_glyph_cache_free (priv->no_mipmap_caches.glyph_cache);
  cogl_pango_glyph_cache_free (priv->mipmap_caches.glyph_cache);

  _cogl_pango_pipeline_cache_free (priv->no_mipmap_caches.pipeline_cache);
  _cogl_pango_pipeline_cache_free (priv->mipmap_caches.pipeline_cache);

  G_OBJECT_CLASS (cogl_pango_renderer_parent_class)->finalize (object);
}

static CoglPangoRenderer *
cogl_pango_get_renderer_from_context (PangoContext *context)
{
  PangoFontMap *font_map;
  CoglPangoFontMap *cogl_font_map;
  PangoRenderer *renderer;

  font_map = pango_context_get_font_map (context);
  g_return_val_if_fail (COGL_PANGO_IS_FONT_MAP (font_map), NULL);

  cogl_font_map = COGL_PANGO_FONT_MAP (font_map);

  renderer = _cogl_pango_font_map_get_renderer (cogl_font_map);

  g_return_val_if_fail (COGL_PANGO_IS_RENDERER (renderer), NULL);

  return COGL_PANGO_RENDERER (renderer);
}

static GQuark
cogl_pango_layout_get_qdata_key (void)
{
  static GQuark key = 0;

  if (G_UNLIKELY (key == 0))
    key = g_quark_from_static_string ("CoglPangoDisplayList");

  return key;
}

static void
cogl_pango_layout_qdata_forget_display_list (CoglPangoLayoutQdata *qdata)
{
  if (qdata->display_list)
    {
      CoglPangoRendererCaches *caches = qdata->mipmapping_used ?
        &qdata->renderer->mipmap_caches :
        &qdata->renderer->no_mipmap_caches;

      _cogl_pango_glyph_cache_remove_reorganize_callback
        (caches->glyph_cache,
         (GHookFunc) cogl_pango_layout_qdata_forget_display_list,
         qdata);

      _cogl_pango_display_list_free (qdata->display_list);

      qdata->display_list = NULL;
    }
}

static void
cogl_pango_render_qdata_destroy (CoglPangoLayoutQdata *qdata)
{
  cogl_pango_layout_qdata_forget_display_list (qdata);
  if (qdata->first_line)
    pango_layout_line_unref (qdata->first_line);
  g_slice_free (CoglPangoLayoutQdata, qdata);
}

void
cogl_pango_show_layout (CoglFramebuffer *fb,
                        PangoLayout *layout,
                        float x,
                        float y,
                        const CoglColor *color)
{
  PangoContext *context;
  CoglPangoRenderer *priv;
  CoglPangoLayoutQdata *qdata;

  context = pango_layout_get_context (layout);
  priv = cogl_pango_get_renderer_from_context (context);
  if (G_UNLIKELY (!priv))
    return;

  qdata = g_object_get_qdata (G_OBJECT (layout),
                              cogl_pango_layout_get_qdata_key ());

  if (qdata == NULL)
    {
      qdata = g_slice_new0 (CoglPangoLayoutQdata);
      qdata->renderer = priv;
      g_object_set_qdata_full (G_OBJECT (layout),
                               cogl_pango_layout_get_qdata_key (),
                               qdata,
                               (GDestroyNotify)
                               cogl_pango_render_qdata_destroy);
    }

  /* Check if the layout has changed since the last build of the
     display list. This trick was suggested by Behdad Esfahbod here:
     http://mail.gnome.org/archives/gtk-i18n-list/2009-May/msg00019.html */
  if (qdata->display_list &&
      ((qdata->first_line &&
        qdata->first_line->layout != layout) ||
       qdata->mipmapping_used != priv->use_mipmapping))
    cogl_pango_layout_qdata_forget_display_list (qdata);

  if (qdata->display_list == NULL)
    {
      CoglPangoRendererCaches *caches = priv->use_mipmapping ?
        &priv->mipmap_caches :
        &priv->no_mipmap_caches;

      cogl_pango_ensure_glyph_cache_for_layout (layout);

      qdata->display_list =
        _cogl_pango_display_list_new (caches->pipeline_cache);

      /* Register for notification of when the glyph cache changes so
         we can rebuild the display list */
      _cogl_pango_glyph_cache_add_reorganize_callback
        (caches->glyph_cache,
         (GHookFunc) cogl_pango_layout_qdata_forget_display_list,
         qdata);

      priv->display_list = qdata->display_list;
      pango_renderer_draw_layout (PANGO_RENDERER (priv), layout, 0, 0);
      priv->display_list = NULL;

      qdata->mipmapping_used = priv->use_mipmapping;
    }

  cogl_framebuffer_push_matrix (fb);
  cogl_framebuffer_translate (fb, x, y, 0);

  _cogl_pango_display_list_render (fb,
                                   qdata->display_list,
                                   color);

  cogl_framebuffer_pop_matrix (fb);

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

void
cogl_pango_render_layout_subpixel (PangoLayout *layout,
                                   int x,
                                   int y,
                                   const CoglColor *color,
                                   int flags)
{
  cogl_pango_show_layout (cogl_get_draw_framebuffer (),
                          layout,
                          x / (float) PANGO_SCALE,
                          y / (float) PANGO_SCALE,
                          color);
}

void
cogl_pango_render_layout (PangoLayout *layout,
                          int x,
                          int y,
                          const CoglColor *color,
                          int flags)
{
  cogl_pango_render_layout_subpixel (layout,
                                     x * PANGO_SCALE,
                                     y * PANGO_SCALE,
                                     color,
                                     flags);
}

void
cogl_pango_show_layout_line (CoglFramebuffer *fb,
                             PangoLayoutLine *line,
                             float x,
                             float y,
                             const CoglColor *color)
{
  PangoContext *context;
  CoglPangoRenderer *priv;
  CoglPangoRendererCaches *caches;
  int pango_x = x * PANGO_SCALE;
  int pango_y = y * PANGO_SCALE;

  context = pango_layout_get_context (line->layout);
  priv = cogl_pango_get_renderer_from_context (context);
  if (G_UNLIKELY (!priv))
    return;

  caches = (priv->use_mipmapping ?
            &priv->mipmap_caches :
            &priv->no_mipmap_caches);

  priv->display_list = _cogl_pango_display_list_new (caches->pipeline_cache);

  _cogl_pango_ensure_glyph_cache_for_layout_line (line);

  pango_renderer_draw_layout_line (PANGO_RENDERER (priv), line,
                                   pango_x, pango_y);

  _cogl_pango_display_list_render (fb,
                                   priv->display_list,
                                   color);

  _cogl_pango_display_list_free (priv->display_list);
  priv->display_list = NULL;
}

void
cogl_pango_render_layout_line (PangoLayoutLine *line,
                               int x,
                               int y,
                               const CoglColor *color)
{
  cogl_pango_show_layout_line (cogl_get_draw_framebuffer (),
                               line,
                               x / (float) PANGO_SCALE,
                               y / (float) PANGO_SCALE,
                               color);
}

void
_cogl_pango_renderer_clear_glyph_cache (CoglPangoRenderer *renderer)
{
  cogl_pango_glyph_cache_clear (renderer->mipmap_caches.glyph_cache);
  cogl_pango_glyph_cache_clear (renderer->no_mipmap_caches.glyph_cache);
}

void
_cogl_pango_renderer_set_use_mipmapping (CoglPangoRenderer *renderer,
                                         CoglBool value)
{
  renderer->use_mipmapping = value;
}

CoglBool
_cogl_pango_renderer_get_use_mipmapping (CoglPangoRenderer *renderer)
{
  return renderer->use_mipmapping;
}

static CoglPangoGlyphCacheValue *
cogl_pango_renderer_get_cached_glyph (PangoRenderer *renderer,
                                      CoglBool       create,
                                      PangoFont     *font,
                                      PangoGlyph     glyph)
{
  CoglPangoRenderer *priv = COGL_PANGO_RENDERER (renderer);
  CoglPangoRendererCaches *caches = (priv->use_mipmapping ?
                                     &priv->mipmap_caches :
                                     &priv->no_mipmap_caches);

  return cogl_pango_glyph_cache_lookup (caches->glyph_cache,
                                        create, font, glyph);
}

static void
cogl_pango_renderer_set_dirty_glyph (PangoFont *font,
                                     PangoGlyph glyph,
                                     CoglPangoGlyphCacheValue *value)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  cairo_glyph_t cairo_glyph;
  cairo_format_t format_cairo;
  CoglPixelFormat format_cogl;

  COGL_NOTE (PANGO, "redrawing glyph %i", glyph);

  /* Glyphs that don't take up any space will end up without a
     texture. These should never become dirty so they shouldn't end up
     here */
  _COGL_RETURN_IF_FAIL (value->texture != NULL);

  if (_cogl_texture_get_format (value->texture) == COGL_PIXEL_FORMAT_A_8)
    {
      format_cairo = CAIRO_FORMAT_A8;
      format_cogl = COGL_PIXEL_FORMAT_A_8;
    }
  else
    {
      format_cairo = CAIRO_FORMAT_ARGB32;

      /* Cairo stores the data in native byte order as ARGB but Cogl's
         pixel formats specify the actual byte order. Therefore we
         need to use a different format depending on the
         architecture */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      format_cogl = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
#else
      format_cogl = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
#endif
    }

  surface = cairo_image_surface_create (format_cairo,
                                        value->draw_width,
                                        value->draw_height);
  cr = cairo_create (surface);

  scaled_font = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
  cairo_set_scaled_font (cr, scaled_font);

  cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);

  cairo_glyph.x = -value->draw_x;
  cairo_glyph.y = -value->draw_y;
  /* The PangoCairo glyph numbers directly map to Cairo glyph
     numbers */
  cairo_glyph.index = glyph;
  cairo_show_glyphs (cr, &cairo_glyph, 1);

  cairo_destroy (cr);
  cairo_surface_flush (surface);

  /* Copy the glyph to the texture */
  cogl_texture_set_region (value->texture,
                           0, /* src_x */
                           0, /* src_y */
                           value->tx_pixel, /* dst_x */
                           value->ty_pixel, /* dst_y */
                           value->draw_width, /* dst_width */
                           value->draw_height, /* dst_height */
                           value->draw_width, /* width */
                           value->draw_height, /* height */
                           format_cogl,
                           cairo_image_surface_get_stride (surface),
                           cairo_image_surface_get_data (surface));

  cairo_surface_destroy (surface);
}

static void
_cogl_pango_ensure_glyph_cache_for_layout_line_internal (PangoLayoutLine *line)
{
  PangoContext *context;
  PangoRenderer *renderer;
  GSList *l;

  context = pango_layout_get_context (line->layout);
  renderer =
    PANGO_RENDERER (cogl_pango_get_renderer_from_context (context));

  for (l = line->runs; l; l = l->next)
    {
      PangoLayoutRun *run = l->data;
      PangoGlyphString *glyphs = run->glyphs;
      int i;

      for (i = 0; i < glyphs->num_glyphs; i++)
        {
          PangoGlyphInfo *gi = &glyphs->glyphs[i];

          /* If the glyph isn't cached then this will reserve
             space for it now. We won't actually draw the glyph
             yet because reserving space could cause all of the
             other glyphs to be moved so we might as well redraw
             them all later once we know that the position is
             settled */
          cogl_pango_renderer_get_cached_glyph (renderer, TRUE,
                                                run->item->analysis.font,
                                                gi->glyph);
        }
    }
}

static void
_cogl_pango_set_dirty_glyphs (CoglPangoRenderer *priv)
{
  _cogl_pango_glyph_cache_set_dirty_glyphs
    (priv->mipmap_caches.glyph_cache, cogl_pango_renderer_set_dirty_glyph);
  _cogl_pango_glyph_cache_set_dirty_glyphs
    (priv->no_mipmap_caches.glyph_cache, cogl_pango_renderer_set_dirty_glyph);
}

static void
_cogl_pango_ensure_glyph_cache_for_layout_line (PangoLayoutLine *line)
{
  PangoContext *context;
  CoglPangoRenderer *priv;

  context = pango_layout_get_context (line->layout);
  priv = cogl_pango_get_renderer_from_context (context);

  _cogl_pango_ensure_glyph_cache_for_layout_line_internal (line);

  /* Now that we know all of the positions are settled we'll fill in
     any dirty glyphs */
  _cogl_pango_set_dirty_glyphs (priv);
}

void
cogl_pango_ensure_glyph_cache_for_layout (PangoLayout *layout)
{
  PangoContext *context;
  CoglPangoRenderer *priv;
  PangoLayoutIter *iter;

  context = pango_layout_get_context (layout);
  priv = cogl_pango_get_renderer_from_context (context);

  _COGL_RETURN_IF_FAIL (PANGO_IS_LAYOUT (layout));

  if ((iter = pango_layout_get_iter (layout)) == NULL)
    return;

  do
    {
      PangoLayoutLine *line;

      line = pango_layout_iter_get_line_readonly (iter);

      _cogl_pango_ensure_glyph_cache_for_layout_line_internal (line);
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);

  /* Now that we know all of the positions are settled we'll fill in
     any dirty glyphs */
  _cogl_pango_set_dirty_glyphs (priv);
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

      cogl_color_init_from_4ub (&color,
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

  _COGL_RETURN_IF_FAIL (priv->display_list != NULL);

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

  _COGL_RETURN_IF_FAIL (priv->display_list != NULL);

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

  _COGL_RETURN_IF_FAIL (priv->display_list != NULL);

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
	  if (font == NULL)
            {
	      cogl_pango_renderer_draw_box (renderer,
                                            x,
                                            y,
                                            PANGO_UNKNOWN_GLYPH_WIDTH,
                                            PANGO_UNKNOWN_GLYPH_HEIGHT);
            }
	  else
	    {
              PangoRectangle ink_rect;

              pango_font_get_glyph_extents (font, gi->glyph, &ink_rect, NULL);
              pango_extents_to_pixels (&ink_rect, NULL);

              cogl_pango_renderer_draw_box (renderer,
                                            x + ink_rect.x,
                                            y + ink_rect.y + ink_rect.height,
                                            ink_rect.width,
                                            ink_rect.height);
	    }
	}
      else
	{
	  /* Get the texture containing the glyph */
	  cache_value =
            cogl_pango_renderer_get_cached_glyph (renderer,
                                                  FALSE,
                                                  font,
                                                  gi->glyph);

          /* cogl_pango_ensure_glyph_cache_for_layout should always be
             called before rendering a layout so we should never have
             a dirty glyph here */
          g_assert (cache_value == NULL || !cache_value->dirty);

	  if (cache_value == NULL)
            {
              cogl_pango_renderer_draw_box (renderer,
                                            x,
                                            y,
                                            PANGO_UNKNOWN_GLYPH_WIDTH,
                                            PANGO_UNKNOWN_GLYPH_HEIGHT);
            }
	  else if (cache_value->texture)
	    {
	      x += (float)(cache_value->draw_x);
	      y += (float)(cache_value->draw_y);

              cogl_pango_renderer_draw_glyph (priv, cache_value, x, y);
	    }
	}

      xi += gi->geometry.width;
    }
}
