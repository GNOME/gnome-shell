/* Metacity X window decorations */

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

#include "frame.h"
#include "errors.h"

void
meta_window_ensure_frame (MetaWindow *window)
{
  MetaFrame *frame;
  int child_x, child_y;
  unsigned long background_pixel;
  XSetWindowAttributes attrs;
  
  if (window->frame)
    return;

  frame = g_new (MetaFrame, 1);

  frame->window = window;

  /* FIXME de-hardcode */
  child_x = 5;
  child_y = 5;

  frame->rect.width = window->rect.width + 10;
  frame->rect.height = window->rect.height + 10;

  background_pixel = BlackPixel (window->display, window->screen->number);
  
  switch (window->win_gravity)
    {
    case NorthWestGravity:
      frame->rect.x = window->rect.x;
      frame->rect.y = window->rect.y;
      break;
    case NorthGravity:
      frame->rect.x = window->rect.x - frame->rect.width / 2;
      frame->rect.y = window->rect.y;
      break;
    case NorthEastGravity:
      frame->rect.x = window->rect.x - frame->rect.width;
      frame->rect.y = window->rect.y;
      break;
    case WestGravity:
      frame->rect.x = window->rect.x;
      frame->rect.y = window->rect.y - frame->rect.height / 2;
      break;
    case CenterGravity:
      frame->rect.x = window->rect.x - frame->rect.width / 2;
      frame->rect.y = window->rect.y - frame->rect.height / 2;
      break;
    case EastGravity:
      frame->rect.x = window->rect.x - frame->rect.width;
      frame->rect.y = window->rect.y - frame->rect.height / 2;
      break;
    case SouthWestGravity:
      frame->rect.x = window->rect.x;
      frame->rect.y = window->rect.y - frame->rect.height;
      break;
    case SouthGravity:
      frame->rect.x = window->rect.x - frame->rect.width / 2;
      frame->rect.y = window->rect.y - frame->rect.height;
      break;
    case SouthEastGravity:
      frame->rect.x = window->rect.x - frame->rect.width;
      frame->rect.y = window->rect.y - frame->rect.height;
      break;
    case StaticGravity:
    default:
      frame->rect.x = window->rect.x - child_x;
      frame->rect.y = window->rect.y - child_y;
      break;
    }

  meta_verbose ("Creating frame %d,%d %dx%d around window 0x%lx %d,%d %dx%d with child position inside frame %d,%d\n",
                frame->rect.x, frame->rect.y,
                frame->rect.width, frame->rect.height,
                window->xwindow,
                window->rect.x, window->rect.y,
                window->rect.width, window->rect.height,
                child_x, child_y);

  attrs.background_pixel = background_pixel;
  attrs.event_mask =
    StructureNotifyMask | ExposureMask |
    ButtonPressMask | ButtonReleaseMask;
  
  frame->xwindow = XCreateWindow (window->display->xdisplay,
                                  window->screen->xroot,
                                  frame->rect.x,
                                  frame->rect.y,
                                  frame->rect.width,
                                  frame->rect.height,
                                  0,
                                  window->depth,
                                  InputOutput,
                                  window->xvisual,
                                  CWBackPixel | CWEventMask,
                                  &attrs);
  
  meta_display_register_x_window (window->display, &frame->xwindow, window);

  /* Reparent the client window; it may be destroyed,
   * thus the error trap. We'll get a destroy notify later
   * and free everything. Comment in FVWM source code says
   * we need the server grab or the child can get its MapNotify
   * before we've finished reparenting and getting the decoration
   * window onscreen.
   */
  meta_display_grab (window->display);

  meta_error_trap_push (window->display);
  XReparentWindow (window->display->xdisplay,
                   window->xwindow,
                   frame->xwindow,
                   child_x,
                   child_y);
  meta_error_trap_pop (window->display);

  /* Show windows */
  XMapWindow (window->display->xdisplay, frame->xwindow);
  XMapWindow (window->display->xdisplay, window->xwindow);
  
  /* stick frame to the window */
  window->frame = frame;
  
  /* Ungrab server */
  meta_display_ungrab (window->display);
}

void
meta_window_destroy_frame (MetaWindow *window)
{
  MetaFrame *frame;
  
  if (window->frame == NULL)
    return;

  frame = window->frame;
  
  /* Unparent the client window; it may be destroyed,
   * thus the error trap.
   */
  meta_error_trap_push (window->display);
  XReparentWindow (window->display->xdisplay,
                   window->xwindow,
                   window->screen->xroot,
                   /* FIXME where to put it back depends on the gravity */
                   window->frame->rect.x,
                   window->frame->rect.y);
  meta_error_trap_pop (window->display);

  meta_display_unregister_x_window (window->display,
                                    frame->xwindow);
  
  window->frame = NULL;

  /* should we push an error trap? */
  XDestroyWindow (window->display->xdisplay, frame->xwindow);
  
  g_free (frame);
}


void
meta_frame_show (MetaFrame *frame)
{
  XMapWindow (frame->window->display->xdisplay, frame->xwindow);
}

void
meta_frame_hide (MetaFrame *frame)
{
  XUnmapWindow (frame->window->display->xdisplay, frame->xwindow);
}

gboolean
meta_frame_event (MetaFrame *frame,
                  XEvent    *event)
{   
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
      meta_warning ("Unexpected destruction of frame 0x%lx, not sure if this should silently fail or be considered a bug\n", frame->xwindow);
      meta_error_trap_push (frame->window->display);
      meta_window_destroy_frame (frame->window);
      meta_error_trap_pop (frame->window->display);
      return TRUE;
      break;
    case UnmapNotify:
      break;
    case MapNotify:
      break;
    case MapRequest:
      break;
    case ReparentNotify:
      break;
    case ConfigureNotify:
      frame->rect.x = event->xconfigure.x;
      frame->rect.y = event->xconfigure.y;
      frame->rect.width = event->xconfigure.width;
      frame->rect.height = event->xconfigure.height;
      return TRUE;
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

  return FALSE;
}
