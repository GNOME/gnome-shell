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

#ifndef META_WINDOW_H
#define META_WINDOW_H

#include <glib-object.h>
#include <X11/Xlib.h>

#include "boxes.h"
#include "types.h"

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

typedef enum
{
  META_MAXIMIZE_HORIZONTAL = 1 << 0,
  META_MAXIMIZE_VERTICAL   = 1 << 1
} MetaMaximizeFlags;

#define META_TYPE_WINDOW            (meta_window_get_type ())
#define META_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_WINDOW, MetaWindow))
#define META_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_WINDOW, MetaWindowClass))
#define META_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_WINDOW_TYPE))
#define META_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_WINDOW))
#define META_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_WINDOW, MetaWindowClass))

typedef struct _MetaWindowClass   MetaWindowClass;

GType meta_window_get_type (void);

MetaFrame *meta_window_get_frame (MetaWindow *window);
gboolean meta_window_has_focus (MetaWindow *window);
gboolean meta_window_is_shaded (MetaWindow *window);
MetaRectangle *meta_window_get_rect (MetaWindow *window);
void meta_window_get_outer_rect (const MetaWindow *window, MetaRectangle *rect);
MetaScreen *meta_window_get_screen (MetaWindow *window);
MetaDisplay *meta_window_get_display (MetaWindow *window);
Window meta_window_get_xwindow (MetaWindow *window);
MetaWindowType meta_window_get_window_type (MetaWindow *window);
Atom meta_window_get_window_type_atom (MetaWindow *window);
MetaWorkspace *meta_window_get_workspace (MetaWindow *window);
gboolean meta_window_is_on_all_workspaces (MetaWindow *window);
gboolean meta_window_is_hidden (MetaWindow *window);
void     meta_window_activate  (MetaWindow *window,guint32 current_time);
void     meta_window_activate_with_workspace (MetaWindow    *window,
                                              guint32        current_time,
                                              MetaWorkspace *workspace);
const char * meta_window_get_description (MetaWindow *window);
/* Return whether the window would be showing if we were on its workspace */
gboolean    meta_window_showing_on_its_workspace (MetaWindow *window);

const char* meta_window_get_startup_id (MetaWindow *window);
void meta_window_change_workspace_by_index (MetaWindow *window,
                                            gint        space_index,
                                            gboolean    append,
                                            guint32     timestamp);
GObject *meta_window_get_compositor_private (MetaWindow *window);
void meta_window_set_compositor_private (MetaWindow *window, GObject *priv);
void meta_window_configure_notify (MetaWindow *window, XConfigureEvent *event);
const char *meta_window_get_role (MetaWindow *window);
MetaStackLayer meta_window_get_layer (MetaWindow *window);
MetaWindow* meta_window_find_root_ancestor    (MetaWindow *window);
gboolean meta_window_get_icon_geometry (MetaWindow    *window,
                                        MetaRectangle *rect);
void meta_window_maximize   (MetaWindow        *window,
                             MetaMaximizeFlags  directions);
void meta_window_unmaximize (MetaWindow        *window,
                             MetaMaximizeFlags  directions);

#endif
