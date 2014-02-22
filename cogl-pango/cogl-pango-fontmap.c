/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008 OpenedHand
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
#include "cogl-util.h"
#include "cogl/cogl-context-private.h"

static GQuark cogl_pango_font_map_get_priv_key (void) G_GNUC_CONST;

typedef struct _CoglPangoFontMapPriv
{
  CoglContext *ctx;
  PangoRenderer *renderer;
} CoglPangoFontMapPriv;

static void
free_priv (gpointer data)
{
  CoglPangoFontMapPriv *priv = data;

  cogl_object_unref (priv->ctx);
  cogl_object_unref (priv->renderer);

  g_free (priv);
}

PangoFontMap *
cogl_pango_font_map_new (void)
{
  PangoFontMap *fm = pango_cairo_font_map_new ();
  CoglPangoFontMapPriv *priv = g_new0 (CoglPangoFontMapPriv, 1);

  _COGL_GET_CONTEXT (context, NULL);

  priv->ctx = cogl_object_ref (context);

  /* XXX: The public pango api doesn't let us sub-class
   * PangoCairoFontMap so we attach our own private data using qdata
   * for now. */
  g_object_set_qdata_full (G_OBJECT (fm),
                           cogl_pango_font_map_get_priv_key (),
                           priv,
                           free_priv);

  return fm;
}

PangoContext *
cogl_pango_font_map_create_context (CoglPangoFontMap *fm)
{
  _COGL_RETURN_VAL_IF_FAIL (COGL_PANGO_IS_FONT_MAP (fm), NULL);

  /* We can just directly use the pango context from the Cairo font
     map */
  return pango_cairo_font_map_create_context (PANGO_CAIRO_FONT_MAP (fm));
}

static CoglPangoFontMapPriv *
_cogl_pango_font_map_get_priv (CoglPangoFontMap *fm)
{
  return g_object_get_qdata (G_OBJECT (fm),
			     cogl_pango_font_map_get_priv_key ());
}

PangoRenderer *
_cogl_pango_font_map_get_renderer (CoglPangoFontMap *fm)
{
  CoglPangoFontMapPriv *priv = _cogl_pango_font_map_get_priv (fm);
  if (G_UNLIKELY (!priv->renderer))
    priv->renderer = _cogl_pango_renderer_new (priv->ctx);
  return priv->renderer;
}

PangoRenderer *
cogl_pango_font_map_get_renderer (CoglPangoFontMap *fm)
{
  return _cogl_pango_font_map_get_renderer (fm);
}

CoglContext *
_cogl_pango_font_map_get_cogl_context (CoglPangoFontMap *fm)
{
  CoglPangoFontMapPriv *priv = _cogl_pango_font_map_get_priv (fm);
  return priv->ctx;
}

void
cogl_pango_font_map_set_resolution (CoglPangoFontMap *font_map,
				    double            dpi)
{
  _COGL_RETURN_IF_FAIL (COGL_PANGO_IS_FONT_MAP (font_map));

  pango_cairo_font_map_set_resolution (PANGO_CAIRO_FONT_MAP (font_map), dpi);
}

void
cogl_pango_font_map_clear_glyph_cache (CoglPangoFontMap *fm)
{
  PangoRenderer *renderer = _cogl_pango_font_map_get_renderer (fm);

  _cogl_pango_renderer_clear_glyph_cache (COGL_PANGO_RENDERER (renderer));
}

void
cogl_pango_font_map_set_use_mipmapping (CoglPangoFontMap *fm,
                                        CoglBool          value)
{
  PangoRenderer *renderer = _cogl_pango_font_map_get_renderer (fm);

  _cogl_pango_renderer_set_use_mipmapping (COGL_PANGO_RENDERER (renderer),
                                           value);
}

CoglBool
cogl_pango_font_map_get_use_mipmapping (CoglPangoFontMap *fm)
{
  PangoRenderer *renderer = _cogl_pango_font_map_get_renderer (fm);

  return
    _cogl_pango_renderer_get_use_mipmapping (COGL_PANGO_RENDERER (renderer));
}

static GQuark
cogl_pango_font_map_get_priv_key (void)
{
  static GQuark priv_key = 0;

  if (G_UNLIKELY (priv_key == 0))
      priv_key = g_quark_from_static_string ("CoglPangoFontMap");

  return priv_key;
}
