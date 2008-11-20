/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Matthew Allum
 * Copyright (C) 2007 Iain Holmes
 * Based on xcompmgr - (c) 2003 Keith Packard
 *          xfwm4    - (c) 2005-2007 Olivier Fourdan
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

#ifndef MUTTER_H_
#define MUTTER_H_

#include <clutter/clutter.h>
#include <X11/Xlib.h>

#include "types.h"
#include "compositor.h"

/*
 * MutterWindow object (ClutterGroup sub-class)
 */
#define MUTTER_TYPE_COMP_WINDOW       (mutter_window_get_type ())
#define MUTTER_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MUTTER_TYPE_COMP_WINDOW, MutterWindow))
#define MUTTER_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MUTTER_TYPE_COMP_WINDOW, MutterWindowClass))
#define IS_MUTTER_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MUTTER_WINDOW_TYPE))
#define MUTTER_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MUTTER_TYPE_COMP_WINDOW))
#define MUTTER_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MUTTER_TYPE_COMP_WINDOW, MutterWindowClass))

typedef struct _MutterWindow        MutterWindow;
typedef struct _MutterWindowClass   MutterWindowClass;
typedef struct _MutterWindowPrivate MutterWindowPrivate;

struct _MutterWindowClass
{
  ClutterGroupClass parent_class;
};

struct _MutterWindow
{
  ClutterGroup           parent;

  MutterWindowPrivate *priv;
};

GType mutter_window_get_type (void);

Window             mutter_window_get_x_window         (MutterWindow *mcw);
MetaCompWindowType mutter_window_get_window_type      (MutterWindow *mcw);
gint               mutter_window_get_workspace        (MutterWindow *mcw);
gboolean           mutter_window_is_hidden            (MutterWindow *mcw);
MetaWindow *       mutter_window_get_meta_window      (MutterWindow *mcw);
ClutterActor *     mutter_window_get_texture          (MutterWindow *mcw);
gboolean           mutter_window_is_override_redirect (MutterWindow *mcw);
const char *       mutter_window_get_description      (MutterWindow *mcw);
gboolean       mutter_window_showing_on_its_workspace (MutterWindow *mcw);

/* Compositor API */
MetaCompositor *mutter_new (MetaDisplay *display);

void mutter_window_effect_completed (MutterWindow *actor, gulong event);

ClutterActor * mutter_get_stage_for_screen (MetaScreen *screen);
ClutterActor * mutter_get_overlay_group_for_screen (MetaScreen *screen);
Window         mutter_get_overlay_window (MetaScreen *screen);
GList        * mutter_get_windows (MetaScreen *screen);
ClutterActor * mutter_get_window_group_for_screen (MetaScreen *screen);

#endif
