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

#include <config.h>
#include "compositor-private.h"
#include "compositor-mutter.h"
#include "prefs.h"

MetaCompositor *
meta_compositor_new (MetaDisplay *display)
{
  return mutter_new (display);
}

void
meta_compositor_destroy (MetaCompositor *compositor)
{
  if (compositor && compositor->destroy)
    compositor->destroy (compositor);
}

void
meta_compositor_add_window (MetaCompositor    *compositor,
                            MetaWindow        *window)
{
  if (compositor && compositor->add_window)
    compositor->add_window (compositor, window);
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
  if (compositor && compositor->remove_window)
    compositor->remove_window (compositor, window);
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
  if (compositor && compositor->manage_screen)
    compositor->manage_screen (compositor, screen);
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
  if (compositor && compositor->unmanage_screen)
    compositor->unmanage_screen (compositor, screen);
}

void
meta_compositor_set_updates (MetaCompositor *compositor,
                             MetaWindow     *window,
                             gboolean        updates)
{
  if (compositor && compositor->set_updates)
    compositor->set_updates (compositor, window, updates);
}

gboolean
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
  if (compositor && compositor->process_event)
    return compositor->process_event (compositor, event, window);
  else
    return FALSE;
}

Pixmap
meta_compositor_get_window_pixmap (MetaCompositor *compositor,
                                   MetaWindow     *window)
{
  if (compositor && compositor->get_window_pixmap)
    return compositor->get_window_pixmap (compositor, window);
  else
    return None;
}

void
meta_compositor_set_active_window (MetaCompositor *compositor,
                                   MetaScreen     *screen,
                                   MetaWindow     *window)
{
  if (compositor && compositor->set_active_window)
    compositor->set_active_window (compositor, screen, window);
}

/* These functions are unused at the moment */
void meta_compositor_begin_move (MetaCompositor *compositor,
                                 MetaWindow     *window,
                                 MetaRectangle  *initial,
                                 int             grab_x,
                                 int             grab_y)
{
}

void meta_compositor_update_move (MetaCompositor *compositor,
                                  MetaWindow     *window,
                                  int             x,
                                  int             y)
{
}

void meta_compositor_end_move (MetaCompositor *compositor,
                               MetaWindow     *window)
{
}

void
meta_compositor_map_window (MetaCompositor *compositor,
			    MetaWindow	   *window)
{
  if (compositor && compositor->map_window)
    compositor->map_window (compositor, window);
}

void
meta_compositor_unmap_window (MetaCompositor *compositor,
			      MetaWindow     *window)
{
  if (compositor && compositor->unmap_window)
    compositor->unmap_window (compositor, window);
}

void
meta_compositor_minimize_window (MetaCompositor *compositor,
                                 MetaWindow     *window,
				 MetaRectangle	*window_rect,
				 MetaRectangle	*icon_rect)
{
  if (compositor && compositor->minimize_window)
    compositor->minimize_window (compositor, window, window_rect, icon_rect);
}

void
meta_compositor_unminimize_window (MetaCompositor    *compositor,
                                   MetaWindow        *window,
				   MetaRectangle     *window_rect,
				   MetaRectangle     *icon_rect)
{
  if (compositor && compositor->unminimize_window)
    compositor->unminimize_window (compositor, window, window_rect, icon_rect);
}

void
meta_compositor_maximize_window (MetaCompositor    *compositor,
                                 MetaWindow        *window,
				 MetaRectangle	   *window_rect)
{
  if (compositor && compositor->maximize_window)
    compositor->maximize_window (compositor, window, window_rect);
}

void
meta_compositor_unmaximize_window (MetaCompositor    *compositor,
                                   MetaWindow        *window,
				   MetaRectangle     *window_rect)
{
  if (compositor && compositor->unmaximize_window)
    compositor->unmaximize_window (compositor, window, window_rect);
}

void
meta_compositor_update_workspace_geometry (MetaCompositor *compositor,
                                           MetaWorkspace  *workspace)
{
  if (compositor && compositor->update_workspace_geometry)
    compositor->update_workspace_geometry (compositor, workspace);
}

void
meta_compositor_switch_workspace (MetaCompositor     *compositor,
                                  MetaScreen         *screen,
                                  MetaWorkspace      *from,
                                  MetaWorkspace      *to,
                                  MetaMotionDirection direction)
{
  if (compositor && compositor->switch_workspace)
    compositor->switch_workspace (compositor, screen, from, to, direction);
}

void
meta_compositor_sync_stack (MetaCompositor  *compositor,
			    MetaScreen	    *screen,
			    GList	    *stack)
{
  if (compositor && compositor->sync_stack)
    compositor->sync_stack (compositor, screen, stack);
}

void
meta_compositor_set_window_hidden (MetaCompositor *compositor,
				   MetaScreen	  *screen,
				   MetaWindow	  *window,
				   gboolean	   hidden)
{
  if (compositor && compositor->set_window_hidden)
    compositor->set_window_hidden (compositor, screen, window, hidden);
}

void
meta_compositor_sync_window_geometry (MetaCompositor *compositor,
				      MetaWindow *window)
{
  if (compositor && compositor->sync_window_geometry)
    compositor->sync_window_geometry (compositor, window);
}

void
meta_compositor_sync_screen_size (MetaCompositor  *compositor,
				  MetaScreen	  *screen,
				  guint		   width,
				  guint		   height)
{
  if (compositor && compositor->sync_screen_size)
    compositor->sync_screen_size (compositor, screen, width, height);
}
