/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Iain Holmes
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_WINDOW_H
#define META_WINDOW_H

#include <glib-object.h>
#include <cairo.h>
#include <X11/Xlib.h>

#include <meta/boxes.h>
#include <meta/types.h>

/**
 * MetaWindowType:
 * @META_WINDOW_NORMAL: Normal
 * @META_WINDOW_DESKTOP: Desktop
 * @META_WINDOW_DOCK: Dock
 * @META_WINDOW_DIALOG: Dialog
 * @META_WINDOW_MODAL_DIALOG: Modal dialog
 * @META_WINDOW_TOOLBAR: Toolbar
 * @META_WINDOW_MENU: Menu
 * @META_WINDOW_UTILITY: Utility
 * @META_WINDOW_SPLASHSCREEN: Splashcreen
 * @META_WINDOW_DROPDOWN_MENU: Dropdown menu
 * @META_WINDOW_POPUP_MENU: Popup menu
 * @META_WINDOW_TOOLTIP: Tooltip
 * @META_WINDOW_NOTIFICATION: Notification
 * @META_WINDOW_COMBO: Combobox
 * @META_WINDOW_DND: Drag and drop
 * @META_WINDOW_OVERRIDE_OTHER: Other override-redirect window type
 */
typedef enum
{
  META_WINDOW_NORMAL,
  META_WINDOW_DESKTOP,
  META_WINDOW_DOCK,
  META_WINDOW_DIALOG,
  META_WINDOW_MODAL_DIALOG,
  META_WINDOW_TOOLBAR,
  META_WINDOW_MENU,
  META_WINDOW_UTILITY,
  META_WINDOW_SPLASHSCREEN,

  /* override redirect window types: */
  META_WINDOW_DROPDOWN_MENU,
  META_WINDOW_POPUP_MENU,
  META_WINDOW_TOOLTIP,
  META_WINDOW_NOTIFICATION,
  META_WINDOW_COMBO,
  META_WINDOW_DND,
  META_WINDOW_OVERRIDE_OTHER
} MetaWindowType;

/**
 * MetaMaximizeFlags:
 * @META_MAXIMIZE_HORIZONTAL: Horizontal
 * @META_MAXIMIZE_VERTICAL: Vertical
 * @META_MAXIMIZE_BOTH: Both
 */
typedef enum
{
  META_MAXIMIZE_HORIZONTAL = 1 << 0,
  META_MAXIMIZE_VERTICAL   = 1 << 1,
  META_MAXIMIZE_BOTH       = (1 << 0 | 1 << 1),
} MetaMaximizeFlags;

/**
 * MetaWindowClientType:
 * @META_WINDOW_CLIENT_TYPE_WAYLAND: A Wayland based window
 * @META_WINDOW_CLIENT_TYPE_X11: An X11 based window
 */
typedef enum {
  META_WINDOW_CLIENT_TYPE_WAYLAND,
  META_WINDOW_CLIENT_TYPE_X11
} MetaWindowClientType;

#define META_TYPE_WINDOW            (meta_window_get_type ())
#define META_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_WINDOW, MetaWindow))
#define META_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_WINDOW, MetaWindowClass))
#define META_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_WINDOW))
#define META_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_WINDOW))
#define META_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_WINDOW, MetaWindowClass))

typedef struct _MetaWindowClass   MetaWindowClass;

GType meta_window_get_type (void);

MetaFrame *meta_window_get_frame (MetaWindow *window);
gboolean meta_window_has_focus (MetaWindow *window);
gboolean meta_window_appears_focused (MetaWindow *window);
gboolean meta_window_is_shaded (MetaWindow *window);
gboolean meta_window_is_override_redirect (MetaWindow *window);
gboolean meta_window_is_skip_taskbar (MetaWindow *window);
void meta_window_get_buffer_rect (const MetaWindow *window, MetaRectangle *rect);

void meta_window_get_frame_rect (const MetaWindow *window, MetaRectangle *rect);
void meta_window_get_outer_rect (const MetaWindow *window, MetaRectangle *rect) G_GNUC_DEPRECATED;

void meta_window_client_rect_to_frame_rect (MetaWindow    *window,
                                            MetaRectangle *client_rect,
                                            MetaRectangle *frame_rect);
void meta_window_frame_rect_to_client_rect (MetaWindow    *window,
                                            MetaRectangle *frame_rect,
                                            MetaRectangle *client_rect);

MetaScreen *meta_window_get_screen (MetaWindow *window);
MetaDisplay *meta_window_get_display (MetaWindow *window);
Window meta_window_get_xwindow (MetaWindow *window);
MetaWindowType meta_window_get_window_type (MetaWindow *window);
MetaWorkspace *meta_window_get_workspace (MetaWindow *window);
int      meta_window_get_monitor (MetaWindow *window);
gboolean meta_window_is_on_all_workspaces (MetaWindow *window);
gboolean meta_window_located_on_workspace (MetaWindow    *window,
                                           MetaWorkspace *workspace);
gboolean meta_window_is_hidden (MetaWindow *window);
void     meta_window_activate  (MetaWindow *window,guint32 current_time);
void     meta_window_activate_with_workspace (MetaWindow    *window,
                                              guint32        current_time,
                                              MetaWorkspace *workspace);
const char * meta_window_get_description (MetaWindow *window);
const char * meta_window_get_wm_class (MetaWindow *window);
const char * meta_window_get_wm_class_instance (MetaWindow *window);
gboolean    meta_window_showing_on_its_workspace (MetaWindow *window);

const char * meta_window_get_gtk_theme_variant (MetaWindow *window);
const char * meta_window_get_gtk_application_id (MetaWindow *window);
const char * meta_window_get_gtk_unique_bus_name (MetaWindow *window);
const char * meta_window_get_gtk_application_object_path (MetaWindow *window);
const char * meta_window_get_gtk_window_object_path (MetaWindow *window);
const char * meta_window_get_gtk_app_menu_object_path (MetaWindow *window);
const char * meta_window_get_gtk_menubar_object_path (MetaWindow *window);

void meta_window_move_frame(MetaWindow *window, gboolean user_op, int root_x_nw, int root_y_nw);
void meta_window_move_resize_frame (MetaWindow *window, gboolean user_op, int root_x_nw, int root_y_nw, int w, int h);
void meta_window_move_to_monitor (MetaWindow *window, int monitor);

void meta_window_set_demands_attention (MetaWindow *window);
void meta_window_unset_demands_attention (MetaWindow *window);

const char* meta_window_get_startup_id (MetaWindow *window);
void meta_window_change_workspace_by_index (MetaWindow *window,
                                            gint        space_index,
                                            gboolean    append);
void meta_window_change_workspace          (MetaWindow  *window,
                                            MetaWorkspace *workspace);
GObject *meta_window_get_compositor_private (MetaWindow *window);
void meta_window_set_compositor_private (MetaWindow *window, GObject *priv);
const char *meta_window_get_role (MetaWindow *window);
MetaStackLayer meta_window_get_layer (MetaWindow *window);
MetaWindow* meta_window_find_root_ancestor    (MetaWindow *window);
gboolean meta_window_is_ancestor_of_transient (MetaWindow            *window,
                                               MetaWindow            *transient);

typedef gboolean (*MetaWindowForeachFunc) (MetaWindow *window,
                                           void       *user_data);

void     meta_window_foreach_transient        (MetaWindow            *window,
                                               MetaWindowForeachFunc  func,
                                               void                  *user_data);
void     meta_window_foreach_ancestor         (MetaWindow            *window,
                                               MetaWindowForeachFunc  func,
                                               void                  *user_data);

MetaMaximizeFlags meta_window_get_maximized (MetaWindow *window);
gboolean          meta_window_is_fullscreen (MetaWindow *window);
gboolean          meta_window_is_screen_sized (MetaWindow *window);
gboolean          meta_window_is_monitor_sized (MetaWindow *window);
gboolean          meta_window_is_on_primary_monitor (MetaWindow *window);
gboolean          meta_window_requested_bypass_compositor (MetaWindow *window);
gboolean          meta_window_requested_dont_bypass_compositor (MetaWindow *window);
gint             *meta_window_get_all_monitors (MetaWindow *window, gsize *length);

gboolean meta_window_get_icon_geometry (MetaWindow    *window,
                                        MetaRectangle *rect);
void meta_window_set_icon_geometry (MetaWindow    *window,
                                    MetaRectangle *rect);
void meta_window_maximize   (MetaWindow        *window,
                             MetaMaximizeFlags  directions);
void meta_window_unmaximize (MetaWindow        *window,
                             MetaMaximizeFlags  directions);
void        meta_window_minimize           (MetaWindow  *window);
void        meta_window_unminimize         (MetaWindow  *window);
void        meta_window_raise              (MetaWindow  *window);
void        meta_window_lower              (MetaWindow  *window);
const char *meta_window_get_title (MetaWindow *window);
MetaWindow *meta_window_get_transient_for (MetaWindow *window);
void        meta_window_delete             (MetaWindow  *window,
                                            guint32      timestamp);
guint       meta_window_get_stable_sequence (MetaWindow *window);
guint32     meta_window_get_user_time (MetaWindow *window);
int         meta_window_get_pid (MetaWindow *window);
const char *meta_window_get_client_machine (MetaWindow *window);
gboolean    meta_window_is_remote (MetaWindow *window);
gboolean    meta_window_is_attached_dialog (MetaWindow *window);
const char *meta_window_get_mutter_hints (MetaWindow *window);

MetaFrameType meta_window_get_frame_type (MetaWindow *window);

cairo_region_t *meta_window_get_frame_bounds (MetaWindow *window);

MetaWindow *meta_window_get_tile_match (MetaWindow *window);

void        meta_window_make_fullscreen    (MetaWindow  *window);
void        meta_window_unmake_fullscreen  (MetaWindow  *window);
void        meta_window_make_above         (MetaWindow  *window);
void        meta_window_unmake_above       (MetaWindow  *window);
void        meta_window_shade              (MetaWindow  *window,
                                            guint32      timestamp);
void        meta_window_unshade            (MetaWindow  *window,
                                            guint32      timestamp);
void        meta_window_stick              (MetaWindow  *window);
void        meta_window_unstick            (MetaWindow  *window);

void        meta_window_kill               (MetaWindow  *window);
void        meta_window_focus              (MetaWindow  *window,
                                            guint32      timestamp);

void        meta_window_check_alive        (MetaWindow  *window,
                                            guint32      timestamp);

void meta_window_get_work_area_current_monitor (MetaWindow    *window,
                                                MetaRectangle *area);
void meta_window_get_work_area_for_monitor     (MetaWindow    *window,
                                                int            which_monitor,
                                                MetaRectangle *area);
void meta_window_get_work_area_all_monitors    (MetaWindow    *window,
                                                MetaRectangle *area);

void meta_window_begin_grab_op (MetaWindow *window,
                                MetaGrabOp  op,
                                gboolean    frame_action,
                                guint32     timestamp);

gboolean meta_window_can_maximize (MetaWindow *window);
gboolean meta_window_can_minimize (MetaWindow *window);
gboolean meta_window_can_shade (MetaWindow *window);
gboolean meta_window_can_close (MetaWindow *window);
gboolean meta_window_is_always_on_all_workspaces (MetaWindow *window);
gboolean meta_window_is_above (MetaWindow *window);
gboolean meta_window_allows_move (MetaWindow *window);
gboolean meta_window_allows_resize (MetaWindow *window);

gboolean meta_window_titlebar_is_onscreen    (MetaWindow *window);
void     meta_window_shove_titlebar_onscreen (MetaWindow *window);

#endif
