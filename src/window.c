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
#include "frame.h"
#include "errors.h"

MetaWindow*
meta_window_new (MetaDisplay *display, Window xwindow)
{
  MetaWindow *window;
  XWindowAttributes attrs;
  GSList *tmp;

  meta_verbose ("Attempting to manage 0x%lx\n", xwindow);
  
  /* round trip */
  meta_error_trap_push (display);

  if (XGetWindowAttributes (display->xdisplay,
                            xwindow, &attrs) == Success &&
      attrs.override_redirect)
    {
      /* Oops. Probably attempted to manage override redirect window
       * in initial screen_manage_all_windows() call.
       */
      meta_error_trap_pop (display);
      return NULL;
    }
  
  XAddToSaveSet (display->xdisplay, xwindow);

  XSelectInput (display->xdisplay, xwindow,
                StructureNotifyMask);
  
  if (meta_error_trap_pop (display) != Success)
    {
      meta_verbose ("Window 0x%lx disappeared just as we tried to manage it\n",
                    xwindow);
      
      return NULL;
    }
  
  window = g_new (MetaWindow, 1);

  window->xwindow = xwindow;

  /* this is in window->screen->display, but that's too annoying to
   * type
   */
  window->display = display;
  
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
  
  window->rect.x = attrs.x;
  window->rect.y = attrs.y;
  window->rect.width = attrs.width;
  window->rect.height = attrs.height;
  window->border_width = attrs.border_width;
  window->win_gravity = attrs.win_gravity;
  window->depth = attrs.depth;
  window->xvisual = attrs.visual;
  
  meta_display_register_x_window (display, &window->xwindow, window);

  window->frame = NULL;
  meta_window_ensure_frame (window);
  
  return window;
}

void
meta_window_free (MetaWindow  *window)
{
  meta_verbose ("Unmanaging 0x%lx\n", window->xwindow);
  
  meta_display_unregister_x_window (window->display, window->xwindow);
  
  meta_window_destroy_frame (window);
  
  g_free (window);
}

gboolean
meta_window_event (MetaWindow  *window,
                   XEvent      *event)
{
  if (window->frame &&
      event->xany.window == window->frame->xwindow)
    return meta_frame_event (window->frame, event);

  if (event->xany.window != window->xwindow)
    return FALSE;
  
  switch (event->type)
    {
    case KeyPress:
      break;
    case KeyRelease:
      break;
    case ButtonPress:
      break;
    case ButtonRelease:
      break;
    case MotionNotify:
      break;
    case EnterNotify:
      break;
    case LeaveNotify:
      break;
    case FocusIn:
      break;
    case FocusOut:
      break;
    case KeymapNotify:
      break;
    case Expose:
      break;
    case GraphicsExpose:
      break;
    case NoExpose:
      break;
    case VisibilityNotify:
      break;
    case CreateNotify:
      break;
    case DestroyNotify:
      meta_window_free (window);
      return TRUE;
      break;
    case UnmapNotify:
      if (window->frame)
        meta_frame_hide (window->frame);
      break;
    case MapNotify:
      if (window->frame)
        meta_frame_show (window->frame);
      break;
    case MapRequest:
      break;
    case ReparentNotify:
      break;
    case ConfigureNotify:
      if (event->xconfigure.override_redirect)
        {
          /* Unmanage it */
          meta_window_free (window);
          return TRUE;
        }
      else
        {
          window->rect.x = event->xconfigure.x;
          window->rect.y = event->xconfigure.y;
          window->rect.width = event->xconfigure.width;
          window->rect.height = event->xconfigure.height;
          window->border_width = event->xconfigure.border_width;
          return TRUE;
        }
      break;
    case ConfigureRequest:
      break;
    case GravityNotify:
      break;
    case ResizeRequest:
      break;
    case CirculateNotify:
      break;
    case CirculateRequest:
      break;
    case PropertyNotify:
      break;
    case SelectionClear:
      break;
    case SelectionRequest:
      break;
    case SelectionNotify:
      break;
    case ColormapNotify:
      break;
    case ClientMessage:
      break;
    case MappingNotify:
      break;
    default:
      break;
    }  
  
  /* Didn't use this event */
  return FALSE;
}
