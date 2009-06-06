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

#ifndef __CLUTTER_MAIN_H__
#define __CLUTTER_MAIN_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-stage.h>
#include <pango/pango.h>

G_BEGIN_DECLS

/**
 * CLUTTER_INIT_ERROR:
 *
 * #GError domain for #ClutterInitError
 */
#define CLUTTER_INIT_ERROR      (clutter_init_error_quark ())

/**
 * ClutterInitError:
 * @CLUTTER_INIT_SUCCESS: Initialisation successful
 * @CLUTTER_INIT_ERROR_UNKNOWN: Unknown error
 * @CLUTTER_INIT_ERROR_THREADS: Thread initialisation failed
 * @CLUTTER_INIT_ERROR_BACKEND: Backend initialisation failed
 * @CLUTTER_INIT_ERROR_INTERNAL: Internal error
 *
 * Error conditions returned by clutter_init() and clutter_init_with_args().
 *
 * Since: 0.2
 */
typedef enum {
  CLUTTER_INIT_SUCCESS        =  1,
  CLUTTER_INIT_ERROR_UNKNOWN  =  0,
  CLUTTER_INIT_ERROR_THREADS  = -1,
  CLUTTER_INIT_ERROR_BACKEND  = -2,
  CLUTTER_INIT_ERROR_INTERNAL = -3
} ClutterInitError;

GQuark clutter_init_error_quark (void);

/**
 * CLUTTER_PRIORITY_REDRAW:
 *
 * Priority of the redraws.
 *
 * Since: 0.8
 */
#define CLUTTER_PRIORITY_REDRAW         (G_PRIORITY_HIGH_IDLE + 20)

/**
 * CLUTTER_PRIORITY_TIMELINE:
 *
 * Priority of the timelines.
 *
 * Since: 0.8
 */
#define CLUTTER_PRIORITY_TIMELINE       (G_PRIORITY_DEFAULT + 30)

/* Initialisation */
void             clutter_base_init        (void);
ClutterInitError clutter_init             (int          *argc,
                                           char       ***argv);
ClutterInitError clutter_init_with_args   (int          *argc,
                                           char       ***argv,
                                           const char   *parameter_string,
                                           GOptionEntry *entries,
                                           const char   *translation_domain,
                                           GError      **error);
GOptionGroup *   clutter_get_option_group (void);
GOptionGroup *   clutter_get_option_group_without_init (void);

/* Mainloop */
void             clutter_main                       (void);
void             clutter_main_quit                  (void);
gint             clutter_main_level                 (void);

void             clutter_redraw                     (ClutterStage *stage);

void             clutter_do_event                   (ClutterEvent *event);

/* Debug utility functions */
gboolean         clutter_get_debug_enabled          (void);
gboolean         clutter_get_show_fps               (void);
gulong           clutter_get_timestamp              (void);

/* Threading functions */
void             clutter_threads_init                  (void);
void             clutter_threads_enter                 (void);
void             clutter_threads_leave                 (void);
void             clutter_threads_set_lock_functions    (GCallback enter_fn,
                                                        GCallback leave_fn);
guint            clutter_threads_add_idle              (GSourceFunc    func,
                                                        gpointer       data);
guint            clutter_threads_add_idle_full         (gint           priority,
                                                        GSourceFunc    func,
                                                        gpointer       data,
                                                        GDestroyNotify notify);
guint            clutter_threads_add_timeout           (guint          interval,
                                                        GSourceFunc    func,
                                                        gpointer       data);
guint            clutter_threads_add_timeout_full      (gint           priority,
                                                        guint          interval,
                                                        GSourceFunc    func,
                                                        gpointer       data,
                                                        GDestroyNotify notify);
guint            clutter_threads_add_frame_source      (guint          fps,
						        GSourceFunc    func,
						        gpointer       data);
guint            clutter_threads_add_frame_source_full (gint           priority,
						        guint          fps,
						        GSourceFunc    func,
						        gpointer       data,
						        GDestroyNotify notify);

guint            clutter_threads_add_repaint_func      (GSourceFunc    func,
                                                        gpointer       data,
                                                        GDestroyNotify notify);
void             clutter_threads_remove_repaint_func   (guint          handle_id);

void             clutter_set_motion_events_enabled   (gboolean enable);
gboolean         clutter_get_motion_events_enabled   (void);

void             clutter_set_default_frame_rate      (guint    frames_per_sec);
guint            clutter_get_default_frame_rate      (void);

void             clutter_grab_pointer                (ClutterActor *actor);
void             clutter_ungrab_pointer              (void);
ClutterActor *   clutter_get_pointer_grab            (void);

void             clutter_grab_keyboard               (ClutterActor *actor);
void             clutter_ungrab_keyboard             (void);
ClutterActor *   clutter_get_keyboard_grab           (void);

void             clutter_clear_glyph_cache           (void);
void             clutter_set_font_flags              (ClutterFontFlags flags);
ClutterFontFlags clutter_get_font_flags              (void);

ClutterInputDevice *clutter_get_input_device_for_id  (gint id);

void             clutter_grab_pointer_for_device     (ClutterActor  *actor,
                                                      gint           id);

void             clutter_ungrab_pointer_for_device   (gint id);

PangoFontMap *   clutter_get_font_map                (void);

G_END_DECLS

#endif /* _CLUTTER_MAIN_H__ */
