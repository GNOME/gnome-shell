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
#include <X11/Xatom.h>

static void     constrain_size            (MetaWindow             *window,
                                           int                     width,
                                           int                     height,
                                           int                    *new_width,
                                           int                    *new_height);
static int      update_size_hints         (MetaWindow             *window);
static int      update_title              (MetaWindow             *window);
static int      update_protocols          (MetaWindow             *window);
static gboolean process_configure_request (MetaWindow             *window,
                                           int x, int y, int width, int height,
                                           int border_width);
static gboolean process_property_notify   (MetaWindow             *window,
                                           XPropertyEvent         *event);

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
      meta_verbose ("Deciding not to manage override_redirect window 0x%lx\n", xwindow);
      meta_error_trap_pop (display);
      return NULL;
    }
  
  XAddToSaveSet (display->xdisplay, xwindow);

  XSelectInput (display->xdisplay, xwindow,
                PropertyChangeMask);
  
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

  window->size_hints.x = attrs.x;
  window->size_hints.y = attrs.y;
  window->size_hints.width = attrs.width;
  window->size_hints.height = attrs.height;
  
  window->depth = attrs.depth;
  window->xvisual = attrs.visual;

  window->title = NULL;
  window->iconic = FALSE;

  window->desc = g_strdup_printf ("0x%lx", window->xwindow);

  window->frame = NULL;
  
  meta_display_register_x_window (display, &window->xwindow, window);

  update_size_hints (window);
  update_title (window);
  update_protocols (window);  

  meta_window_resize (window, window->size_hints.width, window->size_hints.height);
  
  meta_window_ensure_frame (window);
  
  return window;
}

void
meta_window_free (MetaWindow  *window)
{
  meta_verbose ("Unmanaging 0x%lx\n", window->xwindow);
  
  meta_display_unregister_x_window (window->display, window->xwindow);

  meta_window_destroy_frame (window);
  
  g_free (window->title);
  g_free (window->desc);
  g_free (window);
}

void
meta_window_show (MetaWindow *window)
{
  if (window->frame)
    XMapWindow (window->display->xdisplay, window->frame->xwindow);
  XMapWindow (window->display->xdisplay, window->xwindow);

  window->iconic = FALSE;
}

void
meta_window_hide (MetaWindow *window)
{
  if (window->frame)
    XUnmapWindow (window->display->xdisplay, window->frame->xwindow);
  XUnmapWindow (window->display->xdisplay, window->xwindow);

  window->iconic = TRUE;
}

void
meta_window_resize (MetaWindow  *window,
                    int          w,
                    int          h)
{
  meta_verbose ("Resizing %s to %d x %d\n", window->desc, w, h);
  constrain_size (window, w, h, &w, &h);
  meta_verbose ("Constrained resize of %s to %d x %d\n", window->desc, w, h);

  if (w != window->rect.width ||
      h != window->rect.height)
    {
      meta_error_trap_push (window->display);
      XResizeWindow (window->display->xdisplay,
                     window->xwindow,
                     w, h);
      meta_error_trap_pop (window->display);
      window->rect.width = w;
      window->rect.height = h;
      
      if (window->frame)
        meta_frame_queue_recalc (window->frame);
    }
}

gboolean
meta_window_configure_request (MetaWindow *window,
                               XEvent     *event)
{
  return process_configure_request (window,
                                    event->xconfigurerequest.x,
                                    event->xconfigurerequest.y,
                                    event->xconfigurerequest.width,
                                    event->xconfigurerequest.height,
                                    event->xconfigurerequest.border_width);
}

gboolean
meta_window_property_notify (MetaWindow *window,
                             XEvent     *event)
{
  return process_property_notify (window, &event->xproperty);  
}



gboolean
meta_window_event (MetaWindow  *window,
                   XEvent      *event)
{

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

      return TRUE;
      break;
    case UnmapNotify:
      /* Window withdrawn */
      meta_window_free (window);
      return TRUE;
      break;
    case MapNotify:
      break;
    case MapRequest:
      break;
    case ReparentNotify:
      break;
    case ConfigureNotify:

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

static gboolean
process_property_notify (MetaWindow     *window,
                         XPropertyEvent *event)
{
  if (event->atom == XA_WM_NAME ||
      event->atom == window->display->atom_net_wm_name)
    {
      update_title (window);
      
      if (window->frame)
        meta_frame_queue_recalc (window->frame);
    }
  else if (event->atom == XA_WM_NORMAL_HINTS)
    {
      update_size_hints (window);

      /* See if we need to constrain current size */
      meta_window_resize (window, window->rect.width, window->rect.height);
    }
  else if (event->atom == window->display->atom_wm_protocols)
    {
      update_protocols (window);

      if (window->frame)
        meta_frame_queue_recalc (window->frame);
    }
  
  return TRUE;
}

static void
send_configure_notify (MetaWindow *window)
{
  XEvent event;

  /* from twm */
  
  event.type = ConfigureNotify;
  event.xconfigure.display = window->display->xdisplay;
  event.xconfigure.event = window->xwindow;
  event.xconfigure.window = window->xwindow;
  event.xconfigure.x = window->rect.x - window->border_width;
  event.xconfigure.y = window->rect.y - window->border_width;
  if (window->frame)
    {
      /* Need to be in root window coordinates */
      event.xconfigure.x += window->frame->rect.x;
      event.xconfigure.y += window->frame->rect.y;
    }
  event.xconfigure.width = window->rect.width;
  event.xconfigure.height = window->rect.height;
  event.xconfigure.border_width = window->border_width; /* requested not actual */
  event.xconfigure.above = None; /* FIXME */
  event.xconfigure.override_redirect = False;

  meta_verbose ("Sending synthetic configure notify to %s with x: %d y: %d w: %d h: %d\n",
                window->desc,
                event.xconfigure.x, event.xconfigure.y,
                event.xconfigure.width, event.xconfigure.height);
  
  meta_error_trap_push (window->display);
  XSendEvent(window->display->xdisplay,
             window->xwindow,
             False, StructureNotifyMask, &event);
  meta_error_trap_pop (window->display);
}

static gboolean
process_configure_request (MetaWindow *window,
                           int x, int y,
                           int width, int height,
                           int border_width)
{
  /* ICCCM 4.1.5 */
  XWindowChanges values;
  unsigned int mask;
  int client_x, client_y;
  
  /* Note that x, y is the corner of the window border,
   * and width, height is the size of the window inside
   * its border, but that we always deny border requests
   * because we don't believe in clients who use lame-ass
   * X features like that.
   */
  window->border_width = border_width;

  /* We're ignoring the value_mask here, since sizes
   * not in the mask will be the current window geometry.
   */
  
  window->size_hints.x = x;
  window->size_hints.y = y;
  window->size_hints.width = width;
  window->size_hints.height = height;
  
  constrain_size (window,
                  window->size_hints.width,
                  window->size_hints.height,
                  &window->size_hints.width,
                  &window->size_hints.height);

  meta_verbose ("Constrained configure request size to %d x %d\n",
                window->size_hints.width, window->size_hints.height);
  
  if (window->frame)
    {
      meta_frame_child_configure_request (window->frame);
      client_x = window->frame->child_x;
      client_y = window->frame->child_y;
      meta_verbose ("Will place client window %s inside frame at %d,%d\n",
                    window->desc, client_x, client_y);
    }
  else
    {
      client_x = window->size_hints.x;
      client_y = window->size_hints.y;
      meta_verbose ("Will place client window %s at root coordinate %d,%d\n",
                    window->desc, client_x, client_y);
    }
  
  values.border_width = 0;
  values.x = client_x;
  values.y = client_y;
  values.width = window->size_hints.width;
  values.height = window->size_hints.height;
  
  mask = 0;
  if (window->border_width != 0)
    mask |= CWBorderWidth;
  if (values.x != window->rect.x)
    mask |= CWX;
  if (values.y != window->rect.y)
    mask |= CWY;
  if (values.width != window->rect.width)
    mask |= CWWidth;
  if (values.height != window->rect.height)
    mask |= CWHeight;
  
  window->rect.x = values.x;
  window->rect.y = values.y;
  window->rect.width = values.width;
  window->rect.height = values.height;
  
  meta_error_trap_push (window->display);
  XConfigureWindow (window->display->xdisplay,
                    window->xwindow,
                    mask,
                    &values);
  meta_error_trap_pop (window->display);
  
  if (mask & (CWBorderWidth | CWWidth | CWHeight))
    {
      /* Resizing, no synthetic ConfigureNotify, third case in 4.1.5 */      
    }
  else
    {
      /* Moving but not resizing, second case in 4.1.5, or
       * have to send the ConfigureNotify, first case in 4.1.5
       */
      send_configure_notify (window);
    }
    
  return TRUE;
}

static int
update_size_hints (MetaWindow *window)
{
  int x, y, w, h;

  /* Save the last ConfigureRequest, which we put here.
   * Values here set in the hints are supposed to
   * be ignored.
   */
  x = window->size_hints.x;
  y = window->size_hints.y;
  w = window->size_hints.width;
  h = window->size_hints.height;
  
  window->size_hints.flags = 0;
  
  meta_error_trap_push (window->display);
  XGetNormalHints (window->display->xdisplay,
                   window->xwindow,
                   &window->size_hints);

  /* Put it back. */
  window->size_hints.x = x;
  window->size_hints.y = y;
  window->size_hints.width = w;
  window->size_hints.height = h;
  
  if (window->size_hints.flags & PBaseSize)
    {
      meta_verbose ("Window %s sets base size %d x %d\n",
                    window->desc,
                    window->size_hints.base_width,
                    window->size_hints.base_height);
    }
  else if (window->size_hints.flags & PMinSize)
    {
      window->size_hints.base_width = window->size_hints.min_width;
      window->size_hints.base_height = window->size_hints.min_height;
    }
  else
    {
      window->size_hints.base_width = 0;
      window->size_hints.base_height = 0;
    }
  window->size_hints.flags |= PBaseSize;
  
  if (window->size_hints.flags & PMinSize)
    {
      meta_verbose ("Window %s sets min size %d x %d\n",
                    window->desc,
                    window->size_hints.min_width,
                    window->size_hints.min_height);
    }
  else if (window->size_hints.flags & PBaseSize)
    {
      window->size_hints.min_width = window->size_hints.base_width;
      window->size_hints.min_height = window->size_hints.base_height;
    }
  else
    {
      window->size_hints.min_width = 0;
      window->size_hints.min_height = 0;
    }
  window->size_hints.flags |= PMinSize;
  
  if (window->size_hints.flags & PMaxSize)
    {
      meta_verbose ("Window %s sets max size %d x %d\n",
                    window->desc,
                    window->size_hints.max_width,
                    window->size_hints.max_height);
    }
  else
    {
      window->size_hints.max_width = G_MAXINT;
      window->size_hints.max_height = G_MAXINT;
      window->size_hints.flags |= PMaxSize;
    }
  
  if (window->size_hints.flags & PResizeInc)
    {
      meta_verbose ("Window %s sets resize width inc: %d height inc: %d\n",
                    window->desc,
                    window->size_hints.width_inc,
                    window->size_hints.height_inc);
      if (window->size_hints.width_inc == 0)
        {
          window->size_hints.width_inc = 1;
          meta_verbose ("Corrected 0 width_inc to 1\n");
        }
      if (window->size_hints.height_inc == 0)
        {
          window->size_hints.height_inc = 1;
          meta_verbose ("Corrected 0 height_inc to 1\n");
        }
    }
  else
    {
      window->size_hints.width_inc = 1;
      window->size_hints.height_inc = 1;
      window->size_hints.flags |= PResizeInc;
    }
  
  if (window->size_hints.flags & PAspect)
    {
      meta_verbose ("Window %s sets min_aspect: %d/%d max_aspect: %d/%d\n",
                    window->desc,
                    window->size_hints.min_aspect.x,
                    window->size_hints.min_aspect.y,
                    window->size_hints.max_aspect.x,
                    window->size_hints.max_aspect.y);

      /* don't divide by 0 */
      if (window->size_hints.min_aspect.y < 1)
        window->size_hints.min_aspect.y = 1;
      if (window->size_hints.max_aspect.y < 1)
        window->size_hints.max_aspect.y = 1;
    }
  else
    {
      window->size_hints.min_aspect.x = 1;
      window->size_hints.min_aspect.y = G_MAXINT;
      window->size_hints.max_aspect.x = G_MAXINT;
      window->size_hints.max_aspect.y = 1;
      window->size_hints.flags |= PAspect;
    }

  if (window->size_hints.flags & PWinGravity)
    {
      meta_verbose ("Window %s sets gravity %d\n",
                    window->desc,
                    window->size_hints.win_gravity);
    }
  else
    {
      window->size_hints.win_gravity = NorthWestGravity;
      window->size_hints.flags |= PWinGravity;
    }
  
  return meta_error_trap_pop (window->display);
}

static int
update_title (MetaWindow *window)
{
  XTextProperty text;

  meta_error_trap_push (window->display);
  
  if (window->title)
    {
      g_free (window->title);
      window->title = NULL;
    }
  
  /* FIXME How does memory management for text.value work? */
  XGetTextProperty (window->display->xdisplay,
                    window->xwindow,
                    &text,
                    window->display->atom_net_wm_name);

  if (text.nitems > 0 &&
      text.format == 8 && 
      g_utf8_validate (text.value, text.nitems, NULL))
    {
      meta_verbose ("Using _NET_WM_NAME for new title of %s: '%s'\n",
                    window->desc, text.value);

      window->title = g_strdup (text.value);
    }

  if (window->title == NULL &&
      text.nitems > 0)
    meta_warning ("_NET_WM_NAME property for %s contained invalid UTF-8\n",
                  window->desc);

  if (window->title == NULL)
    {
      XGetTextProperty (window->display->xdisplay,
                        window->xwindow,
                        &text,
                        XA_WM_NAME);

      if (text.nitems > 0)
        {
          /* FIXME This isn't particularly correct. Need to copy the
           * GDK code...
           */
          char *str;
          GError *err;

          err = NULL;
          str = g_locale_to_utf8 (text.value,
                                  (text.format / 8) * text.nitems,
                                  NULL, NULL,
                                  &err);
          if (err != NULL)
            {
              meta_warning ("WM_NAME property for %s contained stuff we are too dumb to figure out: %s\n", window->desc, err->message);
              g_error_free (err);
            }

          if (window->title)
            meta_verbose ("Using WM_NAME for new title of %s: '%s'\n",
                          window->desc, text.value);

          window->title = str;
        }
    }
  
  if (window->title == NULL)
    window->title = g_strdup ("");
  
  window->desc = g_strdup_printf ("0x%lx (%.10s)", window->xwindow, window->title);
  
  return meta_error_trap_pop (window->display);
}

static int
update_protocols (MetaWindow *window)
{
  Atom *protocols = NULL;
  int n_protocols = 0;
  int i;

  window->take_focus = FALSE;
  window->delete_window = FALSE;
  
  meta_error_trap_push (window->display);  
  
  if (XGetWMProtocols (window->display->xdisplay,
                       window->xwindow,
                       &protocols,
                       &n_protocols) == Success)
    {
      i = 0;
      while (i < n_protocols)
        {
          if (protocols[i] == window->display->atom_wm_take_focus)
            window->take_focus = TRUE;
          else if (protocols[i] == window->display->atom_wm_delete_window)
            window->delete_window = TRUE;
          ++i;
        }

      if (protocols)
        XFree (protocols);
    }

  meta_verbose ("Window %s has take_focus = %d delete_window = %d\n",
                window->desc, window->take_focus, window->delete_window);
  
  return meta_error_trap_pop (window->display);
}

static void
constrain_size (MetaWindow *window,
                int width, int height,
                int *new_width, int *new_height)
{
  /* This is partially borrowed from GTK (LGPL), which in turn 
   * partially borrowed from fvwm,
   *
   * Copyright 1993, Robert Nation
   *     You may use this code for any purpose, as long as the original
   *     copyright remains in the source code and all documentation
   *
   * which in turn borrows parts of the algorithm from uwm
   */
  int delta;
  double min_aspect, max_aspect;
  
#define FLOOR(value, base)	( ((gint) ((value) / (base))) * (base) )
  
  /* clamp width and height to min and max values
   */
  width = CLAMP (width,
                 window->size_hints.min_width,
                 window->size_hints.max_width);
  height = CLAMP (height,
                  window->size_hints.min_height,
                  window->size_hints.max_height);
  
  /* shrink to base + N * inc
   */
  width = window->size_hints.base_width +
    FLOOR (width - window->size_hints.base_width, window->size_hints.width_inc);
  height = window->size_hints.base_height +
    FLOOR (height - window->size_hints.base_height, window->size_hints.height_inc);

  /* constrain aspect ratio, according to:
   *
   *                width     
   * min_aspect <= -------- <= max_aspect
   *                height    
   */  

  min_aspect = window->size_hints.min_aspect.x / (double) window->size_hints.min_aspect.y;
  max_aspect = window->size_hints.max_aspect.x / (double) window->size_hints.max_aspect.y;

  if (min_aspect * height > width)
    {
      delta = FLOOR (height - width * min_aspect, window->size_hints.height_inc);
      if (height - delta >= window->size_hints.min_height)
        height -= delta;
      else
        { 
          delta = FLOOR (height * min_aspect - width, window->size_hints.width_inc);
          if (width + delta <= window->size_hints.max_width) 
            width += delta;
        }
    }
      
  if (max_aspect * height < width)
    {
      delta = FLOOR (width - height * max_aspect, window->size_hints.width_inc);
      if (width - delta >= window->size_hints.min_width) 
        width -= delta;
      else
        {
          delta = FLOOR (width / max_aspect - height, window->size_hints.height_inc);
          if (height + delta <= window->size_hints.max_height)
            height += delta;
        }
    }

#undef FLOOR
  
  *new_width = width;
  *new_height = height;
}

