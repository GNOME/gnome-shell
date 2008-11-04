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
#include "compositor-xrender.h"
#include "prefs.h"

#ifdef WITH_CLUTTER
#include "compositor-mutter.h"
int meta_compositor_can_use_clutter__ = 0;
#endif

MetaCompositor *
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
#ifdef WITH_CLUTTER
  /* At some point we would have a way to select between backends */
  /* return meta_compositor_xrender_new (display); */
  if (meta_compositor_can_use_clutter__ && !meta_prefs_get_clutter_disabled ())
    return mutter_new (display);
  else
#endif
  return meta_compositor_xrender_new (display);
#else
  return NULL;
#endif
}

void
meta_compositor_destroy (MetaCompositor *compositor)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->destroy)
    compositor->destroy (compositor);
#endif
}

void
meta_compositor_add_window (MetaCompositor    *compositor,
                            MetaWindow        *window,
                            Window             xwindow,
                            XWindowAttributes *attrs)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->add_window)
    compositor->add_window (compositor, window, xwindow, attrs);
#endif
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               Window          xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->remove_window)
    compositor->remove_window (compositor, xwindow);
#endif
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->manage_screen)
    compositor->manage_screen (compositor, screen);
#endif
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->unmanage_screen)
    compositor->unmanage_screen (compositor, screen);
#endif
}

void
meta_compositor_set_updates (MetaCompositor *compositor,
                             MetaWindow     *window,
                             gboolean        updates)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->set_updates)
    compositor->set_updates (compositor, window, updates);
#endif
}

void
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->process_event)
    compositor->process_event (compositor, event, window);
#endif
}

Pixmap
meta_compositor_get_window_pixmap (MetaCompositor *compositor,
                                   MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->get_window_pixmap)
    return compositor->get_window_pixmap (compositor, window);
  else
    return None;
#else
  return None;
#endif
}

void
meta_compositor_set_active_window (MetaCompositor *compositor,
                                   MetaScreen     *screen,
                                   MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->set_active_window)
    compositor->set_active_window (compositor, screen, window);
#endif
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

void meta_compositor_free_window (MetaCompositor *compositor,
                                  MetaWindow     *window)
{
}

void
meta_compositor_destroy_window (MetaCompositor *compositor,
                                MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->destroy_window)
    compositor->destroy_window (compositor, window);
#endif
}

void
meta_compositor_minimize_window (MetaCompositor *compositor,
                                 MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->minimize_window)
    compositor->minimize_window (compositor, window);
#endif
}

void
meta_compositor_maximize_window (MetaCompositor    *compositor,
                                 MetaWindow        *window,
                                 int                x,
                                 int                y,
                                 int                width,
                                 int                height)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->maximize_window)
    compositor->maximize_window (compositor, window, x, y, width, height);
#endif
}

void
meta_compositor_unmaximize_window (MetaCompositor    *compositor,
                                   MetaWindow        *window,
                                   int                x,
                                   int                y,
                                   int                width,
                                   int                height)

{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->unmaximize_window)
    compositor->unmaximize_window (compositor, window, x, y, width, height);
#endif
}

void
meta_compositor_update_workspace_geometry (MetaCompositor *compositor,
                                           MetaWorkspace  *workspace)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->update_workspace_geometry)
    compositor->update_workspace_geometry (compositor, workspace);
#endif
}

void
meta_compositor_switch_workspace (MetaCompositor     *compositor,
                                  MetaScreen         *screen,
                                  MetaWorkspace      *from,
                                  MetaWorkspace      *to,
                                  MetaMotionDirection direction)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->switch_workspace)
    compositor->switch_workspace (compositor, screen, from, to, direction);
#endif
}

void
meta_compositor_sync_stack (MetaCompositor  *compositor,
			    MetaScreen	    *screen,
			    GList	    *stack)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->sync_stack)
    compositor->sync_stack (compositor, screen, stack);
#endif
}

void
meta_compositor_set_window_hidden (MetaCompositor *compositor,
				   MetaScreen	  *screen,
				   MetaWindow	  *window,
				   gboolean	   hidden)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (compositor && compositor->set_window_hidden)
    compositor->set_window_hidden (compositor, screen, window, hidden);
#endif
}

