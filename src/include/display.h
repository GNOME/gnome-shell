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

#ifndef META_DISPLAY_H
#define META_DISPLAY_H

#include <glib-object.h>
#include <X11/Xlib.h>

#include "types.h"
#include "common.h"

typedef enum
{
  META_TAB_LIST_NORMAL,
  META_TAB_LIST_DOCKS,
  META_TAB_LIST_GROUP
} MetaTabList;

typedef enum
{
  META_TAB_SHOW_ICON,      /* Alt-Tab mode */
  META_TAB_SHOW_INSTANTLY  /* Alt-Esc mode */
} MetaTabShowType;

typedef struct _MetaDisplayClass MetaDisplayClass;

#define META_TYPE_DISPLAY              (meta_display_get_type ())
#define META_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), META_TYPE_DISPLAY, MetaDisplay))
#define META_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_DISPLAY, MetaDisplayClass))
#define META_IS_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), META_TYPE_DISPLAY))
#define META_IS_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_DISPLAY))
#define META_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_DISPLAY, MetaDisplayClass))

GType meta_display_get_type (void) G_GNUC_CONST;

#define meta_XFree(p) do { if ((p)) XFree ((p)); } while (0)

typedef enum
{
  META_ATOM_FIRST = 0,
#define item(x) META_ATOM_##x,
#include "atomnames.h"
#undef item
}MetaAtom;

void meta_display_get_compositor_version (MetaDisplay *display,
                                          int         *major,
                                          int         *minor);
Display *meta_display_get_xdisplay (MetaDisplay *display);
MetaCompositor *meta_display_get_compositor (MetaDisplay *display);
GSList *meta_display_get_screens (MetaDisplay *display);

gboolean meta_display_has_shape (MetaDisplay *display);

MetaScreen *meta_display_screen_for_root (MetaDisplay *display,
                                          Window       xroot);
MetaWindow *meta_display_get_focus_window (MetaDisplay *display);

int meta_display_get_damage_event_base (MetaDisplay *display);
int meta_display_get_shape_event_base (MetaDisplay *display);
Atom meta_display_get_atom (MetaDisplay *display, MetaAtom meta_atom);
guint32 meta_display_get_current_time (MetaDisplay *display);
guint32 meta_display_get_current_time_roundtrip (MetaDisplay *display);

GList* meta_display_get_tab_list (MetaDisplay   *display,
                                  MetaTabList    type,
                                  MetaScreen    *screen,
                                  MetaWorkspace *workspace);

MetaWindow* meta_display_get_tab_next (MetaDisplay   *display,
                                       MetaTabList    type,
				       MetaScreen    *screen,
                                       MetaWorkspace *workspace,
                                       MetaWindow    *window,
                                       gboolean       backward);

MetaWindow* meta_display_get_tab_current (MetaDisplay   *display,
                                          MetaTabList    type,
                                          MetaScreen    *screen,
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
/* meta_display_set_input_focus_window is like XSetInputFocus, except
 * that (a) it can't detect timestamps later than the current time,
 * since Mutter isn't part of the XServer, and thus gives erroneous
 * behavior in this circumstance (so don't do it), (b) it uses
 * display->last_focus_time since we don't have access to the true
 * Xserver one, (c) it makes use of display->user_time since checking
 * whether a window should be allowed to be focused should depend
 * on user_time events (see bug 167358, comment 15 in particular)
 */
void meta_display_set_input_focus_window   (MetaDisplay *display,
                                            MetaWindow  *window,
                                            gboolean     focus_frame,
                                            guint32      timestamp);

/* meta_display_focus_the_no_focus_window is called when the
 * designated no_focus_window should be focused, but is otherwise the
 * same as meta_display_set_input_focus_window
 */
void meta_display_focus_the_no_focus_window (MetaDisplay *display,
                                             MetaScreen  *screen,
                                             guint32      timestamp);

#endif
