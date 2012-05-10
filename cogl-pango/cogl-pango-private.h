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

#ifndef __COGL_PANGO_PRIVATE_H__
#define __COGL_PANGO_PRIVATE_H__

#include "cogl-pango.h"

COGL_BEGIN_DECLS

PangoRenderer *
_cogl_pango_renderer_new (CoglContext *context);

void
_cogl_pango_renderer_clear_glyph_cache  (CoglPangoRenderer *renderer);

void
_cogl_pango_renderer_set_use_mipmapping (CoglPangoRenderer *renderer,
                                         CoglBool value);
CoglBool
_cogl_pango_renderer_get_use_mipmapping (CoglPangoRenderer *renderer);



CoglContext *
_cogl_pango_font_map_get_cogl_context (CoglPangoFontMap *fm);

PangoRenderer *
_cogl_pango_font_map_get_renderer (CoglPangoFontMap *fm);

COGL_END_DECLS

#endif /* __COGL_PANGO_PRIVATE_H__ */
