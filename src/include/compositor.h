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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_COMPOSITOR_H
#define META_COMPOSITOR_H

#include <glib.h>
#include <X11/Xlib.h>

#include "types.h"
#include "boxes.h"
#include "window.h"
#include "workspace.h"

typedef enum _MetaCompWindowType
{
  META_COMP_WINDOW_NORMAL = META_WINDOW_NORMAL,
  META_COMP_WINDOW_DESKTOP =  META_WINDOW_DESKTOP,
  META_COMP_WINDOW_DOCK = META_WINDOW_DOCK,
  META_COMP_WINDOW_DIALOG = META_WINDOW_DIALOG,
  META_COMP_WINDOW_MODAL_DIALOG = META_WINDOW_MODAL_DIALOG,
  META_COMP_WINDOW_TOOLBAR = META_WINDOW_TOOLBAR,
  META_COMP_WINDOW_MENU = META_WINDOW_MENU,
  META_COMP_WINDOW_UTILITY = META_WINDOW_UTILITY,
  META_COMP_WINDOW_SPLASHSCREEN = META_WINDOW_SPLASHSCREEN,

  /* override redirect window types, */
  META_COMP_WINDOW_DROPDOWN_MENU = META_WINDOW_DROPDOWN_MENU,
  META_COMP_WINDOW_POPUP_MENU = META_WINDOW_POPUP_MENU,
  META_COMP_WINDOW_TOOLTIP = META_WINDOW_TOOLTIP,
  META_COMP_WINDOW_NOTIFICATION = META_WINDOW_NOTIFICATION,
  META_COMP_WINDOW_COMBO = META_WINDOW_COMBO,
  META_COMP_WINDOW_DND = META_WINDOW_DND,
  META_COMP_WINDOW_OVERRIDE_OTHER = META_WINDOW_OVERRIDE_OTHER

} MetaCompWindowType;

/**
 * MetaCompEffect:
 * @META_COMP_EFFECT_CREATE: The window is newly created
 *   (also used for a window that was previously on a different
 *   workspace and is changed to become visible on the active
 *   workspace.)
 * @META_COMP_EFFECT_UNMINIMIZE: The window should be shown
 *   as unminimizing from its icon geometry.
 * @META_COMP_EFFECT_DESTROY: The window is being destroyed
 * @META_COMP_EFFECT_MINIMIZE: The window should be shown
 *   as minimizing to its icon geometry.
 * @META_COMP_EFFECT_NONE: No effect, the window should be
 *   shown or hidden immediately.
 *
 * Indicates the appropriate effect to show the user for
 * meta_compositor_show_window() and meta_compositor_hide_window()
 */
typedef enum
{
  META_COMP_EFFECT_CREATE,
  META_COMP_EFFECT_UNMINIMIZE,
  META_COMP_EFFECT_DESTROY,
  META_COMP_EFFECT_MINIMIZE,
  META_COMP_EFFECT_NONE
} MetaCompEffect;

MetaCompositor *meta_compositor_new     (MetaDisplay    *display);
void            meta_compositor_destroy (MetaCompositor *compositor);

void meta_compositor_manage_screen   (MetaCompositor *compositor,
                                      MetaScreen     *screen);
void meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                      MetaScreen     *screen);

gboolean meta_compositor_process_event (MetaCompositor *compositor,
                                        XEvent         *event,
                                        MetaWindow     *window);

/* At a high-level, a window is not-visible or visible. When a
 * window is added (with add_window()) it is not visible.
 * show_window() indicates a transition from not-visible to
 * visible. Some of the reasons for this:
 *
 *  - Window newly created
 *  - Window is unminimized
 *  - Window is moved to the current desktop
 *  - Window was made sticky
 *
 * hide_window() indicates that the window has transitioned from
 * visible to not-visible. Some reasons include:
 *
 *  - Window was destroyed
 *  - Window is minimized
 *  - Window is moved to a different desktop
 *  - Window no longer sticky.
 *
 * Note that combinations are possible - a window might have first
 * been minimized and then moved to a different desktop. The
 * 'effect' parameter to show_window() and hide_window() is a hint
 * as to the appropriate effect to show the user and should not
 * be considered to be indicative of a state change.
 *
 * When the active workspace is changed, switch_workspace() is called
 * first, then show_window() and hide_window() are called individually
 * for each window affected, with an effect of META_COMP_EFFECT_NONE.
 * If hiding windows will affect the switch workspace animation, the
 * compositor needs to delay hiding the windows until the switch
 * workspace animation completes.
 *
 * maximize_window() and unmaximize_window() are transitions within
 * the visible state. The window is resized *before* the call, so
 * it may be necessary to readjust the display based on the old_rect
 * to start the animation.
 *
 * window_mapped() and window_unmapped() are notifications when the
 * toplevel window (frame or client window) is mapped or unmapped.
 * That is, when the result of meta_window_toplevel_is_mapped()
 * changes. The main use of this is to drop resources when a window
 * is unmapped. A window will always be mapped before show_window()
 * is called and will not be unmapped until after hide_window() is
 * called. If the live_hidden_windows preference is set, windows will
 * never be unmapped.
 */

void meta_compositor_add_window    (MetaCompositor *compositor,
                                    MetaWindow     *window);
void meta_compositor_remove_window (MetaCompositor *compositor,
                                    MetaWindow     *window);

void meta_compositor_show_window       (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaCompEffect       effect);
void meta_compositor_hide_window       (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaCompEffect       effect);
void meta_compositor_switch_workspace  (MetaCompositor      *compositor,
                                        MetaScreen          *screen,
                                        MetaWorkspace       *from,
                                        MetaWorkspace       *to,
                                        MetaMotionDirection  direction);

void meta_compositor_maximize_window   (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaRectangle       *old_rect,
                                        MetaRectangle       *new_rect);
void meta_compositor_unmaximize_window (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaRectangle       *old_rect,
                                        MetaRectangle       *new_rect);

void meta_compositor_window_mapped        (MetaCompositor *compositor,
                                           MetaWindow     *window);
void meta_compositor_window_unmapped      (MetaCompositor *compositor,
                                           MetaWindow     *window);
void meta_compositor_sync_window_geometry (MetaCompositor *compositor,
                                           MetaWindow     *window);
void meta_compositor_set_updates          (MetaCompositor *compositor,
                                           MetaWindow     *window,
                                           gboolean        updates);

void meta_compositor_update_workspace_geometry (MetaCompositor *compositor,
                                                MetaWorkspace  *workspace);
void meta_compositor_sync_stack                (MetaCompositor *compositor,
                                                MetaScreen     *screen,
                                                GList          *stack);
void meta_compositor_sync_screen_size          (MetaCompositor *compositor,
                                                MetaScreen     *screen,
                                                guint           width,
                                                guint           height);

#endif /* META_COMPOSITOR_H */
