/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011 Intel Corp
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

#ifndef __CLUTTER_MAIN_DEPRECATED_H__
#define __CLUTTER_MAIN_DEPRECATED_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-input-device.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_IN_1_10
void                    clutter_threads_init                    (void);

CLUTTER_DEPRECATED_IN_1_12
void                    clutter_threads_enter                   (void);

CLUTTER_DEPRECATED_IN_1_12
void                    clutter_threads_leave                   (void);

CLUTTER_DEPRECATED_IN_1_6
guint                   clutter_threads_add_frame_source        (guint             fps,
                                                                 GSourceFunc       func,
                                                                 gpointer          data);
CLUTTER_DEPRECATED_IN_1_6
guint                   clutter_threads_add_frame_source_full   (gint              priority,
                                                                 guint             fps,
                                                                 GSourceFunc       func,
                                                                 gpointer          data,
                                                                 GDestroyNotify    notify);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_stage_set_motion_events_enabled)
void                    clutter_set_motion_events_enabled       (gboolean          enable);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_stage_get_motion_events_enabled)
gboolean                clutter_get_motion_events_enabled       (void);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_stage_ensure_redraw)
void                    clutter_redraw                          (ClutterStage     *stage);

CLUTTER_DEPRECATED_IN_1_10_FOR(cogl_pango_font_map_clear_glyph_cache)
void                    clutter_clear_glyph_cache               (void);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_backend_set_font_options)
void                    clutter_set_font_flags                  (ClutterFontFlags  flags);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_backend_get_font_options)
ClutterFontFlags        clutter_get_font_flags                  (void);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_device_manager_get_device)
ClutterInputDevice *    clutter_get_input_device_for_id         (gint id_);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_input_device_grab)
void                    clutter_grab_pointer_for_device         (ClutterActor     *actor,
                                                                 gint              id_);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_input_device_ungrab)
void                    clutter_ungrab_pointer_for_device       (gint              id_);

CLUTTER_DEPRECATED_IN_1_10
void                    clutter_set_default_frame_rate          (guint             frames_per_sec);

CLUTTER_DEPRECATED_IN_1_10
gulong                  clutter_get_timestamp                   (void);

CLUTTER_DEPRECATED_IN_1_10
gboolean                clutter_get_debug_enabled               (void);

CLUTTER_DEPRECATED_IN_1_10
gboolean                clutter_get_show_fps                    (void);

G_END_DECLS

#endif /* __CLUTTER_MAIN_DEPRECATED_H__ */
