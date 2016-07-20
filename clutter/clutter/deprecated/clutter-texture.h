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

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_image_new)
ClutterActor *          clutter_texture_new                    (void);

CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and platform-specific image loading)
ClutterActor *          clutter_texture_new_from_file           (const gchar            *filename,
                                                                 GError                **error);

CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and platform-specific image loading)
gboolean                clutter_texture_set_from_file           (ClutterTexture         *texture,
                                                                 const gchar            *filename,
                                                                 GError                **error);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_image_set_data)
gboolean                clutter_texture_set_from_rgb_data       (ClutterTexture         *texture,
                                                                 const guchar           *data,
                                                                 gboolean                has_alpha,
                                                                 gint                    width,
                                                                 gint                    height,
                                                                 gint                    rowstride,
                                                                 gint                    bpp,
                                                                 ClutterTextureFlags     flags,
                                                                 GError                **error);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_image_set_area)
gboolean                clutter_texture_set_area_from_rgb_data  (ClutterTexture         *texture,
                                                                 const guchar           *data,
                                                                 gboolean                has_alpha,
                                                                 gint                    x,
                                                                 gint                    y,
                                                                 gint                    width,
                                                                 gint                    height,
                                                                 gint                    rowstride,
                                                                 gint                    bpp,
                                                                 ClutterTextureFlags     flags,
                                                                 GError                **error);
CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and clutter_content_get_preferred_size)
void                    clutter_texture_get_base_size           (ClutterTexture         *texture,
                                                                 gint                   *width,
                                                                 gint                   *height);
CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and clutter_actor_set_content_scaling_filters)
void                    clutter_texture_set_filter_quality      (ClutterTexture         *texture,
                                                                 ClutterTextureQuality   filter_quality);
CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and clutter_actor_get_content_scaling_filters)
ClutterTextureQuality   clutter_texture_get_filter_quality      (ClutterTexture         *texture);
CLUTTER_DEPRECATED_IN_1_12
CoglHandle              clutter_texture_get_cogl_texture        (ClutterTexture         *texture);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_texture_set_cogl_texture        (ClutterTexture         *texture,
                                                                 CoglHandle              cogl_tex);
CLUTTER_DEPRECATED_IN_1_12
CoglHandle              clutter_texture_get_cogl_material       (ClutterTexture         *texture);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_texture_set_cogl_material       (ClutterTexture         *texture,
                                                                 CoglHandle              cogl_material);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_texture_set_sync_size           (ClutterTexture         *texture,
                                                                 gboolean                sync_size);
CLUTTER_DEPRECATED_IN_1_12
gboolean                clutter_texture_get_sync_size           (ClutterTexture         *texture);
CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and clutter_actor_set_content_repeat)
void                    clutter_texture_set_repeat              (ClutterTexture         *texture,
                                                                 gboolean                repeat_x,
                                                                 gboolean                repeat_y);
CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and clutter_actor_get_content_repeat)
void                    clutter_texture_get_repeat              (ClutterTexture         *texture,
                                                                 gboolean               *repeat_x,
                                                                 gboolean               *repeat_y);
CLUTTER_DEPRECATED_IN_1_12
gint                    clutter_texture_get_max_tile_waste      (ClutterTexture         *texture);
CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and clutter_actor_set_content_gravity)
void                    clutter_texture_set_keep_aspect_ratio   (ClutterTexture         *texture,
                                                                 gboolean                keep_aspect);
CLUTTER_DEPRECATED_IN_1_12_FOR(ClutterImage and clutter_actor_get_content_gravity)
gboolean                clutter_texture_get_keep_aspect_ratio   (ClutterTexture         *texture);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_texture_set_load_async          (ClutterTexture         *texture,
                                                                 gboolean                load_async);
CLUTTER_DEPRECATED_IN_1_12
gboolean                clutter_texture_get_load_async          (ClutterTexture         *texture);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_texture_set_load_data_async     (ClutterTexture         *texture,
                                                                 gboolean                load_async);
CLUTTER_DEPRECATED_IN_1_12
gboolean                clutter_texture_get_load_data_async     (ClutterTexture         *texture);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_texture_set_pick_with_alpha     (ClutterTexture         *texture,
                                                                 gboolean                pick_with_alpha);
CLUTTER_DEPRECATED_IN_1_12
gboolean                clutter_texture_get_pick_with_alpha     (ClutterTexture         *texture);

CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterOffscreenEffect)
ClutterActor *          clutter_texture_new_from_actor          (ClutterActor           *actor);

CLUTTER_DEPRECATED_IN_1_10
gboolean                clutter_texture_set_from_yuv_data       (ClutterTexture         *texture,
                                                                 const guchar           *data,
                                                                 gint                    width,
                                                                 gint                    height,
                                                                 ClutterTextureFlags     flags,
                                                                 GError                **error);

G_END_DECLS

#endif /* __CLUTTER_TEXTURE_DEPRECATED_H__ */
