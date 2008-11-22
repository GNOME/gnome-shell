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

#ifndef META_COMPOSITOR_PRIVATE_H
#define META_COMPOSITOR_PRIVATE_H

#include "compositor.h"

struct _MetaCompositor
{
  void (* destroy) (MetaCompositor *compositor);

  void (*manage_screen) (MetaCompositor *compositor,
                         MetaScreen     *screen);
  void (*unmanage_screen) (MetaCompositor *compositor,
                           MetaScreen     *screen);
  void (*add_window) (MetaCompositor    *compositor,
                      MetaWindow        *window);
  void (*remove_window) (MetaCompositor *compositor,
                         MetaWindow     *window);
  void (*set_updates) (MetaCompositor *compositor,
                       MetaWindow     *window,
                       gboolean        update);
  gboolean (*process_event) (MetaCompositor *compositor,
                             XEvent         *event,
                             MetaWindow     *window);
  Pixmap (*get_window_pixmap) (MetaCompositor *compositor,
                               MetaWindow     *window);
  void (*set_active_window) (MetaCompositor *compositor,
                             MetaScreen     *screen,
                             MetaWindow     *window);
  void (*map_window) (MetaCompositor *compositor,
                      MetaWindow     *window);
  void (*unmap_window) (MetaCompositor *compositor,
			MetaWindow     *window);
  void (*minimize_window) (MetaCompositor *compositor,
                           MetaWindow     *window,
			   MetaRectangle  *window_rect,
			   MetaRectangle  *icon_rect);
  void (*unminimize_window) (MetaCompositor *compositor,
			     MetaWindow     *window,
			     MetaRectangle  *window_rect,
			     MetaRectangle  *icon_rect);
  void (*maximize_window) (MetaCompositor    *compositor,
                           MetaWindow        *window,
			   MetaRectangle     *window_rect);
  void (*unmaximize_window) (MetaCompositor    *compositor,
                             MetaWindow        *window,
			     MetaRectangle     *window_rect);
  void (*update_workspace_geometry) (MetaCompositor *compositor,
                                     MetaWorkspace   *workspace);
  void (*switch_workspace) (MetaCompositor     *compositor,
                            MetaScreen         *screen,
                            MetaWorkspace      *from,
                            MetaWorkspace      *to,
                            MetaMotionDirection direction);
  void (*sync_stack) (MetaCompositor *compositor,
		      MetaScreen     *screen,
		      GList	     *stack);
  void (*set_window_hidden) (MetaCompositor *compositor,
			     MetaScreen	    *screen,
			     MetaWindow	    *window,
			     gboolean	     hidden);
  void (*sync_window_geometry) (MetaCompositor	*compositor,
				MetaWindow	*window);
  void (*sync_screen_size) (MetaCompositor *compositor,
			    MetaScreen	   *screen,
			    guint	    width,
			    guint	    height);
};

#endif
