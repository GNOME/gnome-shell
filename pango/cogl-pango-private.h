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

#ifndef __COGL_PANGO_PRIVATE_H__
#define __COGL_PANGO_PRIVATE_H__

#include "cogl-pango.h"

G_BEGIN_DECLS

void           _cogl_pango_renderer_clear_glyph_cache  (CoglPangoRenderer *renderer);
void           _cogl_pango_renderer_set_use_mipmapping (CoglPangoRenderer *renderer,
                                                        gboolean           value);
gboolean       _cogl_pango_renderer_get_use_mipmapping (CoglPangoRenderer *renderer);

G_END_DECLS

#endif /* __COGL_PANGO_PRIVATE_H__ */
