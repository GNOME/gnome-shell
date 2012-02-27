/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#ifndef __CLUTTER_TEXTURE_DEPRECATED_H__
#define __CLUTTER_TEXTURE_DEPRECATED_H__

#include <clutter/clutter-texture.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterOffscreenEffect)
ClutterActor *  clutter_texture_new_from_actor          (ClutterActor           *actor);

CLUTTER_DEPRECATED_IN_1_10
gboolean        clutter_texture_set_from_yuv_data       (ClutterTexture         *texture,
                                                         const guchar           *data,
                                                         gint                    width,
                                                         gint                    height,
                                                         ClutterTextureFlags     flags,
                                                         GError                **error);

G_END_DECLS

#endif /* __CLUTTER_TEXTURE_DEPRECATED_H__ */
