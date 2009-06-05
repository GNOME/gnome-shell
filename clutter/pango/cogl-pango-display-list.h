/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2009  Intel Corporation.
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

#ifndef __COGL_PANGO_DISPLAY_LIST_H__
#define __COGL_PANGO_DISPLAY_LIST_H__

#include <glib.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

typedef struct _CoglPangoDisplayList CoglPangoDisplayList;

CoglPangoDisplayList *_cogl_pango_display_list_new (void);

void _cogl_pango_display_list_set_color (CoglPangoDisplayList *dl,
                                         const CoglColor *color);

void _cogl_pango_display_list_add_texture (CoglPangoDisplayList *dl,
                                           CoglHandle texture,
                                           float x_1, float y_1,
                                           float x_2, float y_2,
                                           float tx_1, float ty_1,
                                           float tx_2, float ty_2);

void _cogl_pango_display_list_add_rectangle (CoglPangoDisplayList *dl,
                                             float x_1, float y_1,
                                             float x_2, float y_2);

void _cogl_pango_display_list_add_trapezoid (CoglPangoDisplayList *dl,
                                             float y_1,
                                             float x_11,
                                             float x_21,
                                             float y_2,
                                             float x_12,
                                             float x_22);

void _cogl_pango_display_list_render (CoglPangoDisplayList *dl,
                                      CoglHandle glyph_material,
                                      CoglHandle solid_material);

void _cogl_pango_display_list_clear (CoglPangoDisplayList *dl);

void _cogl_pango_display_list_free (CoglPangoDisplayList *dl);

G_END_DECLS

#endif /* __COGL_PANGO_DISPLAY_LIST_H__ */
