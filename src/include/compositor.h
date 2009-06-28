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

MetaCompositor *meta_compositor_new     (MetaDisplay    *display);
void            meta_compositor_destroy (MetaCompositor *compositor);

void meta_compositor_manage_screen   (MetaCompositor *compositor,
                                      MetaScreen     *screen);
void meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                      MetaScreen     *screen);

gboolean meta_compositor_process_event (MetaCompositor *compositor,
                                        XEvent         *event,
                                        MetaWindow     *window);

void meta_compositor_add_window    (MetaCompositor *compositor,
                                    MetaWindow     *window);
void meta_compositor_remove_window (MetaCompositor *compositor,
                                    MetaWindow     *window);

void meta_compositor_map_window        (MetaCompositor      *compositor,
                                        MetaWindow          *window);
void meta_compositor_unmap_window      (MetaCompositor      *compositor,
                                        MetaWindow          *window);
void meta_compositor_minimize_window   (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaRectangle       *window_rect,
                                        MetaRectangle       *icon_rect);
void meta_compositor_unminimize_window (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaRectangle       *window_rect,
                                        MetaRectangle       *icon_rect);
void meta_compositor_maximize_window   (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaRectangle       *old_rect,
                                        MetaRectangle       *new_rect);
void meta_compositor_unmaximize_window (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaRectangle       *old_rect,
                                        MetaRectangle       *new_rect);
void meta_compositor_switch_workspace  (MetaCompositor      *compositor,
                                        MetaScreen          *screen,
                                        MetaWorkspace       *from,
                                        MetaWorkspace       *to,
                                        MetaMotionDirection  direction);

void meta_compositor_set_window_hidden    (MetaCompositor *compositor,
                                           MetaScreen	  *screen,
                                           MetaWindow     *window,
                                           gboolean        hidden);
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
