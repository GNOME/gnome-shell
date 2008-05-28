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

#ifndef _HAVE_PANGO_CLUTTER_PRIVATE_H
#define _HAVE_PANGO_CLUTTER_PRIVATE_H

#include "pangoclutter.h"

G_BEGIN_DECLS

PangoRenderer *_pango_clutter_font_map_get_renderer (PangoClutterFontMap *fm);

void _pango_clutter_renderer_clear_glyph_cache (PangoClutterRenderer *renderer);

void _pango_clutter_renderer_set_use_mipmapping (PangoClutterRenderer *renderer,
						 gboolean              value);

G_END_DECLS

#endif /* _HAVE_PANGO_CLUTTER_H */
