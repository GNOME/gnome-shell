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

#ifndef META_COMPOSITOR_CLUTTER_H_
#define META_COMPOSITOR_CLUTTER_H_

#include <clutter/clutter.h>
#include <xlib/xlib.h>

#include "types.h"

/*
 * MetaCompWindow object (ClutterGroup sub-class)
 */
#define META_TYPE_COMP_WINDOW            (meta_comp_window_get_type ())
#define META_COMP_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_COMP_WINDOW, MetaCompWindow))
#define META_COMP_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_COMP_WINDOW, MetaCompWindowClass))
#define IS_META_COMP_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_COMP_WINDOW_TYPE))
#define META_IS_COMP_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_COMP_WINDOW))
#define META_COMP_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_COMP_WINDOW, MetaCompWindowClass))

typedef struct _MetaCompWindow        MetaCompWindow;
typedef struct _MetaCompWindowClass   MetaCompWindowClass;
typedef struct _MetaCompWindowPrivate MetaCompWindowPrivate;

struct _MetaCompWindowClass
{
  ClutterGroupClass parent_class;
};

struct _MetaCompWindow
{
  ClutterGroup           parent;

  MetaCompWindowPrivate *priv;
};

GType meta_comp_window_get_type (void);

Window             meta_comp_window_get_x_window    (MetaCompWindow *mcw);
MetaCompWindowType meta_comp_window_get_window_type (MetaCompWindow *mcw);
gint               meta_comp_window_get_workspace   (MetaCompWindow *mcw);


/* Compositor API */
MetaCompositor *meta_compositor_clutter_new (MetaDisplay *display);

void meta_compositor_clutter_window_effect_completed (MetaCompWindow *actor, gulong event);

ClutterActor * meta_compositor_clutter_get_stage_for_screen (MetaScreen *screen);
ClutterActor * meta_compositor_clutter_get_overlay_group_for_screen (MetaScreen *screen);

Window meta_compositor_clutter_get_overlay_window (MetaScreen *screen);


#endif
