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

#ifndef META_DISPLAY_H
#define META_DISPLAY_H

#include <glib-object.h>
#include <X11/Xlib.h>

#include <meta/types.h>
#include <meta/prefs.h>
#include <meta/common.h>
#include <meta/workspace.h>

/**
 * MetaTabList:
 * @META_TAB_LIST_NORMAL: Normal windows
 * @META_TAB_LIST_DOCKS: Dock windows
 * @META_TAB_LIST_GROUP: Groups
 * @META_TAB_LIST_NORMAL_ALL: All windows
 */
typedef enum
{
  META_TAB_LIST_NORMAL,
  META_TAB_LIST_DOCKS,
  META_TAB_LIST_GROUP,
  META_TAB_LIST_NORMAL_ALL
} MetaTabList;

/**
 * MetaTabShowType:
 * @META_TAB_SHOW_ICON: Show icon (Alt-Tab mode)
 * @META_TAB_SHOW_INSTANTLY: Show instantly (Alt-Esc mode)
 */
typedef enum
{
  META_TAB_SHOW_ICON,      /* Alt-Tab mode */
  META_TAB_SHOW_INSTANTLY  /* Alt-Esc mode */
} MetaTabShowType;

typedef enum
{
  META_PAD_ACTION_BUTTON, /* Action is a button */
  META_PAD_ACTION_RING,   /* Action is a ring */
  META_PAD_ACTION_STRIP,  /* Action is a strip */
} MetaPadActionType;

typedef struct _MetaDisplayClass MetaDisplayClass;

#define META_TYPE_DISPLAY              (meta_display_get_type ())
#define META_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), META_TYPE_DISPLAY, MetaDisplay))
#define META_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_DISPLAY, MetaDisplayClass))
#define META_IS_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), META_TYPE_DISPLAY))
#define META_IS_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_DISPLAY))
#define META_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_DISPLAY, MetaDisplayClass))

GType meta_display_get_type (void) G_GNUC_CONST;

#define meta_XFree(p) do { if ((p)) XFree ((p)); } while (0)

gboolean meta_display_supports_extended_barriers (MetaDisplay *display);

MetaCompositor *meta_display_get_compositor  (MetaDisplay *display);
MetaX11Display *meta_display_get_x11_display (MetaDisplay *display);

MetaWindow *meta_display_get_focus_window (MetaDisplay *display);

gboolean meta_display_xserver_time_is_before (MetaDisplay *display,
                                              guint32      time1,
                                              guint32      time2);

guint32 meta_display_get_last_user_time (MetaDisplay *display);
guint32 meta_display_get_current_time (MetaDisplay *display);
guint32 meta_display_get_current_time_roundtrip (MetaDisplay *display);

GList* meta_display_get_tab_list (MetaDisplay   *display,
                                  MetaTabList    type,
                                  MetaWorkspace *workspace);

MetaWindow* meta_display_get_tab_next (MetaDisplay   *display,
                                       MetaTabList    type,
                                       MetaWorkspace *workspace,
                                       MetaWindow    *window,
                                       gboolean       backward);

MetaWindow* meta_display_get_tab_current (MetaDisplay   *display,
                                          MetaTabList    type,
                                          MetaWorkspace *workspace);

gboolean meta_display_begin_grab_op (MetaDisplay *display,
                                     MetaScreen  *screen,
                                     MetaWindow  *window,
                                     MetaGrabOp   op,
                                     gboolean     pointer_already_grabbed,
                                     gboolean     frame_action,
                                     int          button,
                                     gulong       modmask,
                                     guint32      timestamp,
                                     int          root_x,
                                     int          root_y);
void     meta_display_end_grab_op   (MetaDisplay *display,
                                     guint32      timestamp);

MetaGrabOp meta_display_get_grab_op (MetaDisplay *display);

guint meta_display_add_keybinding    (MetaDisplay         *display,
                                      const char          *name,
                                      GSettings           *settings,
                                      MetaKeyBindingFlags  flags,
                                      MetaKeyHandlerFunc   handler,
                                      gpointer             user_data,
                                      GDestroyNotify       free_data);
gboolean meta_display_remove_keybinding (MetaDisplay         *display,
                                         const char          *name);

guint    meta_display_grab_accelerator   (MetaDisplay *display,
                                          const char  *accelerator);
gboolean meta_display_ungrab_accelerator (MetaDisplay *display,
                                          guint        action_id);

guint meta_display_get_keybinding_action (MetaDisplay  *display,
                                          unsigned int  keycode,
                                          unsigned long mask);

GSList *meta_display_sort_windows_by_stacking (MetaDisplay *display,
                                               GSList      *windows);

void meta_display_add_ignored_crossing_serial (MetaDisplay  *display,
                                               unsigned long serial);

void meta_display_unmanage_screen (MetaDisplay *display,
                                   MetaScreen  *screen,
                                   guint32      timestamp);

void meta_display_clear_mouse_mode (MetaDisplay *display);

void meta_display_freeze_keyboard (MetaDisplay *display,
                                   guint32      timestamp);
void meta_display_ungrab_keyboard (MetaDisplay *display,
                                   guint32      timestamp);
void meta_display_unfreeze_keyboard (MetaDisplay *display,
                                     guint32      timestamp);
gboolean meta_display_is_pointer_emulating_sequence (MetaDisplay          *display,
                                                     ClutterEventSequence *sequence);

void    meta_display_request_pad_osd      (MetaDisplay        *display,
                                           ClutterInputDevice *pad,
                                           gboolean            edition_mode);
gchar * meta_display_get_pad_action_label (MetaDisplay        *display,
                                           ClutterInputDevice *pad,
                                           MetaPadActionType   action_type,
                                           guint               action_number);

void meta_display_get_size (MetaDisplay *display,
                            int         *width,
                            int         *height);

void meta_display_set_cursor (MetaDisplay *display,
                              MetaCursor   cursor);

GSList *meta_display_get_startup_sequences (MetaDisplay *display);

/**
 * MetaDisplayDirection:
 * @META_DISPLAY_UP: up
 * @META_DISPLAY_DOWN: down
 * @META_DISPLAY_LEFT: left
 * @META_DISPLAY_RIGHT: right
 */
typedef enum
{
  META_DISPLAY_UP,
  META_DISPLAY_DOWN,
  META_DISPLAY_LEFT,
  META_DISPLAY_RIGHT
} MetaDisplayDirection;

int  meta_display_get_n_monitors       (MetaDisplay   *display);
int  meta_display_get_primary_monitor  (MetaDisplay   *display);
int  meta_display_get_current_monitor  (MetaDisplay   *display);
void meta_display_get_monitor_geometry (MetaDisplay   *display,
                                        int            monitor,
                                        MetaRectangle *geometry);

gboolean meta_display_get_monitor_in_fullscreen (MetaDisplay *display,
                                                 int          monitor);

int meta_display_get_monitor_index_for_rect (MetaDisplay   *display,
                                             MetaRectangle *rect);

int meta_display_get_monitor_neighbor_index (MetaDisplay         *display,
                                             int                  which_monitor,
                                             MetaDisplayDirection dir);

#endif
