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
  /*
   * Types shared with MetaWindow
   */
  META_COMP_WINDOW_NORMAL  = META_WINDOW_NORMAL,
  META_COMP_WINDOW_DESKTOP = META_WINDOW_DESKTOP,
  META_COMP_WINDOW_DOCK    = META_WINDOW_DOCK,
  META_COMP_WINDOW_MENU    = META_WINDOW_MENU,

  /*
   * Extended types that WM does not care about, but we do.
   */
  META_COMP_WINDOW_TOOLTIP = 0xf000,
  META_COMP_WINDOW_DROP_DOWN_MENU,
  META_COMP_WINDOW_DND,
  META_COMP_WINDOW_OVERRIDE,
} MetaCompWindowType;


#ifdef WITH_CLUTTER
extern int meta_compositor_can_use_clutter__;
#endif
MetaCompositor *meta_compositor_new (MetaDisplay *display);
void meta_compositor_destroy (MetaCompositor *compositor);

void meta_compositor_manage_screen (MetaCompositor *compositor,
                                    MetaScreen     *screen);
void meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                      MetaScreen     *screen);

void meta_compositor_add_window (MetaCompositor    *compositor,
                                 MetaWindow        *window,
                                 Window             xwindow,
                                 XWindowAttributes *attrs);
void meta_compositor_remove_window (MetaCompositor *compositor,
                                    Window          xwindow);

void meta_compositor_set_updates (MetaCompositor *compositor,
                                  MetaWindow     *window,
                                  gboolean        updates);

void meta_compositor_process_event (MetaCompositor *compositor,
                                    XEvent         *event,
                                    MetaWindow     *window);
Pixmap meta_compositor_get_window_pixmap (MetaCompositor *compositor,
                                          MetaWindow     *window);
void meta_compositor_set_active_window (MetaCompositor *compositor,
                                        MetaScreen     *screen,
                                        MetaWindow     *window);

void meta_compositor_begin_move (MetaCompositor *compositor,
                                 MetaWindow *window,
                                 MetaRectangle *initial,
                                 int grab_x, int grab_y);
void meta_compositor_update_move (MetaCompositor *compositor,
                                  MetaWindow *window,
                                  int x, int y);
void meta_compositor_end_move (MetaCompositor *compositor,
                               MetaWindow *window);
void meta_compositor_free_window (MetaCompositor *compositor,
                                  MetaWindow *window);

void
meta_compositor_destroy_window (MetaCompositor *compositor,
                                MetaWindow     *window);

void
meta_compositor_minimize_window (MetaCompositor *compositor,
                                 MetaWindow     *window);

void
meta_compositor_maximize_window (MetaCompositor    *compositor,
                                 MetaWindow        *window,
                                 int                x,
                                 int                y,
                                 int                width,
                                 int                height);

void
meta_compositor_unmaximize_window (MetaCompositor    *compositor,
                                   MetaWindow        *window,
                                   int                x,
                                   int                y,
                                   int                width,
                                   int                height);

void
meta_compositor_update_workspace_geometry (MetaCompositor *compositor,
                                           MetaWorkspace  *workspace);

void
meta_compositor_switch_workspace (MetaCompositor     *compositor,
                                  MetaScreen         *screen,
                                  MetaWorkspace      *from,
                                  MetaWorkspace      *to,
                                  MetaMotionDirection direction);

void
meta_compositor_sync_stack (MetaCompositor  *compositor,
			    MetaScreen      *screen,
			    GList	    *stack);

void
meta_compositor_set_window_hidden (MetaCompositor *compositor,
				   MetaScreen	  *screen,
				   MetaWindow	  *window,
				   gboolean	   hidden);

#endif





