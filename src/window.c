/* Metacity X managed windows */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include "window.h"
#include "util.h"

MetaWindow*
meta_window_new (MetaDisplay *display, Window xwindow)
{
  MetaWindow *window;
  XWindowAttributes attrs;
  GSList *tmp;

  /* round trip */
  if (XGetWindowAttributes (display->xdisplay,
                            xwindow, &attrs) != Success)
    {
      meta_verbose ("Window 0x%lx disappeared just as we tried to manage it\n",
                    xwindow);
      
      return NULL;
    }
  
  window = g_new (MetaWindow, 1);

  window->xwindow = xwindow;
  
  window->screen = NULL;
  tmp = display->screens;
  while (tmp != NULL)
    {
      if (((MetaScreen *)tmp->data)->xscreen == attrs.screen)
        {
          window->screen = tmp->data;
          break;
        }
      
      tmp = tmp->next;
    }
  
  g_assert (window->screen);

  return window;
}

void
meta_window_free (MetaWindow  *window)
{
  g_free (window);
}

gboolean
meta_window_event (MetaWindow  *window,
                   XEvent      *event)
{

  /* consumed this event */
  return TRUE;
}
