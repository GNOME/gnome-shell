/*
 * Clutter
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Emmanuele Bassi <ebassi@linux.intel.com>
 *              Matthew Allum <mallum@o-hand.com>
 *              Chris Lord <chris@o-hand.com>
 *              Iain Holmes <iain@o-hand.com>
 *              Neil Roberts <neil@linux.intel.com>
 *
 * Copyright (C) 2008, 2009, 2010  Intel Corporation.
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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CAIRO_TEXTURE_DEPRECATED_H__
#define __CLUTTER_CAIRO_TEXTURE_DEPRECATED_H__

#include <clutter/clutter-cairo-texture.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_cairo_texture_invalidate_rectangle)
cairo_t *       clutter_cairo_texture_create_region             (ClutterCairoTexture   *self,
                                                                 gint                   x_offset,
                                                                 gint                   y_offset,
                                                                 gint                   width,
                                                                 gint                   height);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_cairo_texture_invalidate)
cairo_t *       clutter_cairo_texture_create                    (ClutterCairoTexture   *self);

G_END_DECLS

#endif /* __CLUTTER_CAIRO_TEXTURE_DEPRECATED_H__ */
