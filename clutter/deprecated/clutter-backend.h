/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd
 * Copyright (C) 2009, 2010 Intel Corp
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

#ifndef __CLUTTER_BACKEND_DEPRECATED_H__
#define __CLUTTER_BACKEND_DEPRECATED_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_IN_1_4_FOR(ClutterSettings:font_dpi)
void            clutter_backend_set_resolution                  (ClutterBackend *backend,
                                                                 gdouble         dpi);

CLUTTER_DEPRECATED_IN_1_4_FOR(ClutterSettings:double_click_time)
void            clutter_backend_set_double_click_time           (ClutterBackend *backend,
                                                                 guint           msec);

CLUTTER_DEPRECATED_IN_1_4_FOR(ClutterSettings:double_click_time)
guint           clutter_backend_get_double_click_time           (ClutterBackend *backend);

CLUTTER_DEPRECATED_IN_1_4_FOR(ClutterSettings:double_click_distance)
void            clutter_backend_set_double_click_distance       (ClutterBackend *backend,
                                                                 guint           distance);

CLUTTER_DEPRECATED_IN_1_4_FOR(ClutterSettings:double_click_distance)
guint           clutter_backend_get_double_click_distance       (ClutterBackend *backend);

CLUTTER_DEPRECATED_IN_1_4_FOR(ClutterSettings:font_name)
void            clutter_backend_set_font_name                   (ClutterBackend *backend,
                                                                 const gchar    *font_name);

CLUTTER_DEPRECATED_IN_1_4_FOR(ClutterSettings:font_name)
const gchar *   clutter_backend_get_font_name                   (ClutterBackend *backend);


G_END_DECLS

#endif /* __CLUTTER_BACKEND_DEPRECATED_H__ */
