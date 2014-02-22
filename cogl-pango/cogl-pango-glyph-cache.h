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

#ifndef __COGL_PANGO_GLYPH_CACHE_H__
#define __COGL_PANGO_GLYPH_CACHE_H__

#include <glib.h>
#include <pango/pango-font.h>

#include "cogl/cogl-texture.h"

COGL_BEGIN_DECLS

typedef struct _CoglPangoGlyphCache      CoglPangoGlyphCache;
typedef struct _CoglPangoGlyphCacheValue CoglPangoGlyphCacheValue;

struct _CoglPangoGlyphCacheValue
{
  CoglTexture *texture;

  float tx1;
  float ty1;
  float tx2;
  float ty2;

  int tx_pixel;
  int ty_pixel;

  int draw_x;
  int draw_y;
  int draw_width;
  int draw_height;

  /* This will be set to TRUE when the glyph atlas is reorganized
     which means the glyph will need to be redrawn */
  CoglBool   dirty;
};

typedef void (* CoglPangoGlyphCacheDirtyFunc) (PangoFont *font,
                                               PangoGlyph glyph,
                                               CoglPangoGlyphCacheValue *value);

CoglPangoGlyphCache *
cogl_pango_glyph_cache_new (CoglContext *ctx,
                            CoglBool use_mipmapping);

void
cogl_pango_glyph_cache_free (CoglPangoGlyphCache *cache);

CoglPangoGlyphCacheValue *
cogl_pango_glyph_cache_lookup (CoglPangoGlyphCache *cache,
                               CoglBool             create,
                               PangoFont           *font,
                               PangoGlyph           glyph);

void
cogl_pango_glyph_cache_clear (CoglPangoGlyphCache *cache);

void
_cogl_pango_glyph_cache_add_reorganize_callback (CoglPangoGlyphCache *cache,
                                                 GHookFunc func,
                                                 void *user_data);

void
_cogl_pango_glyph_cache_remove_reorganize_callback (CoglPangoGlyphCache *cache,
                                                    GHookFunc func,
                                                    void *user_data);

void
_cogl_pango_glyph_cache_set_dirty_glyphs (CoglPangoGlyphCache *cache,
                                          CoglPangoGlyphCacheDirtyFunc func);

COGL_END_DECLS

#endif /* __COGL_PANGO_GLYPH_CACHE_H__ */
