/* Metacity X display handler */

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

#include "display.h"
#include "util.h"
#include "main.h"
#include "screen.h"
#include "window.h"
#include "frame.h"

static GSList *all_displays = NULL;
static void   meta_spew_event           (MetaDisplay    *display,
                                         XEvent         *event);
static void   event_queue_callback      (MetaEventQueue *queue,
                                         XEvent         *event,
                                         gpointer        data);
static Window event_get_modified_window (MetaDisplay    *display,
                                         XEvent         *event);




static gint
unsigned_long_equal (gconstpointer v1,
                     gconstpointer v2)
{
  return *((const gulong*) v1) == *((const gulong*) v2);
}

static guint
unsigned_long_hash (gconstpointer v)
{
  gulong val = * (const gulong *) v;

  /* I'm not sure this works so well. */
#if G_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}


gboolean
meta_display_open (const char *name)
{
  MetaDisplay *display;
  Display *xdisplay;
  GSList *screens;
  GSList *tmp;
  int i;
  char *atom_names[] = { "_NET_WM_NAME", "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW" };
  Atom atoms[G_N_ELEMENTS(atom_names)];
  
  meta_verbose ("Opening display '%s'\n", XDisplayName (name));
  
  xdisplay = XOpenDisplay (name);

  if (xdisplay == NULL)
    {
      meta_warning (_("Failed to open X Window System display '%s'\n"),
                    XDisplayName (name));
      return FALSE;
    }

  if (meta_is_syncing ())
    XSynchronize (xdisplay, True);
  
  display = g_new (MetaDisplay, 1);

  /* here we use XDisplayName which is what the user
   * probably put in, vs. DisplayString(display) which is
   * canonicalized by XOpenDisplay()
   */
  display->name = g_strdup (XDisplayName (name));
  display->xdisplay = xdisplay;
  display->error_traps = NULL;

  /* we have to go ahead and do this so error handlers work */
  all_displays = g_slist_prepend (all_displays, display);
  
  screens = NULL;
  i = 0;
  while (i < ScreenCount (xdisplay))
    {
      MetaScreen *screen;

      screen = meta_screen_new (display, i);

      if (screen)
        screens = g_slist_prepend (screens, screen);
      ++i;
    }

  if (screens == NULL)
    {
      /* This would typically happen because all the screens already
       * have window managers
       */
      XCloseDisplay (xdisplay);
      all_displays = g_slist_remove (all_displays, display);
      g_free (display->name);
      g_free (display);
      return FALSE;
    }
  
  display->screens = screens;
  
  display->events = meta_event_queue_new (display->xdisplay,
                                          event_queue_callback,
                                          display);

  display->window_ids = g_hash_table_new (unsigned_long_hash, unsigned_long_equal);

  display->server_grab_count = 0;

  XInternAtoms (display->xdisplay, atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);
  display->atom_net_wm_name = atoms[0];
  
  /* Now manage all existing windows */
  tmp = display->screens;
  while (tmp != NULL)
    {
      meta_screen_manage_all_windows (tmp->data);
      tmp = tmp->next;
    }
  
  return TRUE;
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;

  listp = data;
  *listp = g_slist_prepend (*listp, value);
}

static gint
ptrcmp (gconstpointer a, gconstpointer b)
{
  if (a < b)
    return -1;
  else if (a > b)
    return 1;
  else
    return 0;
}

void
meta_display_close (MetaDisplay *display)
{
  GSList *winlist;
  GSList *tmp;
  
  if (display->error_traps)
    meta_bug ("Display closed with error traps pending\n");

  winlist = NULL;
  g_hash_table_foreach (display->window_ids,
                        listify_func,
                        &winlist);
  
  g_hash_table_destroy (display->window_ids);

  winlist = g_slist_sort (winlist, ptrcmp);

  tmp = winlist;
  while (tmp != NULL)
    {
      /* If the next node doesn't contain this window
       * a second time, delete the window.
       */
      g_assert (tmp->data != NULL);
      
      if (tmp->next == NULL ||
          (tmp->next && tmp->next->data != tmp->data))
        meta_window_free (tmp->data);
      
      tmp = tmp->data;
    }
  g_slist_free (winlist);
  
  meta_event_queue_free (display->events);
  XCloseDisplay (display->xdisplay);
  g_free (display->name);

  all_displays = g_slist_remove (all_displays, display);
  
  g_free (display);

  if (all_displays == NULL)
    {
      meta_verbose ("Last display closed, exiting\n");
      meta_quit (META_EXIT_SUCCESS);
    }
}

MetaScreen*
meta_display_screen_for_root (MetaDisplay *display,
                              Window       xroot)
{
  GSList *tmp;

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      if (xroot == screen->xroot)
        return screen;

      tmp = tmp->next;
    }

  return NULL;
}

MetaScreen*
meta_display_screen_for_x_screen (MetaDisplay *display,
                                  Screen      *xscreen)
{
  GSList *tmp;

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      if (xscreen == screen->xscreen)
        return screen;

      tmp = tmp->next;
    }

  return NULL;
}

/* Grab/ungrab routines taken from fvwm */
void
meta_display_grab (MetaDisplay *display)
{
  if (display->server_grab_count == 0)
    {
      XSync (display->xdisplay, False);
      XGrabServer (display->xdisplay);
    }
  XSync (display->xdisplay, False);
  display->server_grab_count += 1;
}

void
meta_display_ungrab (MetaDisplay *display)
{
  if (display->server_grab_count == 0)
    meta_bug ("Ungrabbed non-grabbed server\n");

  display->server_grab_count -= 1;
  if (display->server_grab_count == 0)
    {
      XUngrabServer (display->xdisplay);
    }
  XSync (display->xdisplay, False);
}

MetaDisplay*
meta_display_for_x_display (Display *xdisplay)
{
  GSList *tmp;

  tmp = all_displays;
  while (tmp != NULL)
    {
      MetaDisplay *display = tmp->data;

      if (display->xdisplay == xdisplay)
        return display;

      tmp = tmp->next;
    }

  return NULL;
}

GSList*
meta_displays_list (void)
{
  return all_displays;
}

static gboolean dump_events = TRUE;

static void
event_queue_callback (MetaEventQueue *queue,
                      XEvent         *event,
                      gpointer        data)
{
  MetaWindow *window;
  MetaDisplay *display;
  Window modified;
  
  display = data;
  
  if (dump_events)
    meta_spew_event (display, event);

  modified = event_get_modified_window (display, event);

  if (modified != None)
    window = meta_display_lookup_x_window (display, modified);
  else
    window = NULL;
  
  if (window &&
      window->frame &&
      modified == window->frame->xwindow)
    {
      meta_frame_event (window->frame, event);
      return;
    }
  
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
      if (window)
        meta_window_free (window); /* Unmanage destroyed window */
      break;
    case UnmapNotify:
      if (window)
        meta_window_free (window); /* Unmanage withdrawn window */
      break;
    case MapNotify:
      break;
    case MapRequest:
      if (window == NULL)
        window = meta_window_new (display, event->xmaprequest.window);
      break;
    case ReparentNotify:
      break;
    case ConfigureNotify:
      if (window && event->xconfigure.override_redirect)
        {
          /* Unmanage it, override_redirect was toggled on?
           * Can this happen?
           */
          meta_window_free (window);
        }
      break;
    case ConfigureRequest:
      /* This comment and code is found in both twm and fvwm */
      /*
       * According to the July 27, 1988 ICCCM draft, we should ignore size and
       * position fields in the WM_NORMAL_HINTS property when we map a window.
       * Instead, we'll read the current geometry.  Therefore, we should respond
       * to configuration requests for windows which have never been mapped.
       */
      if (window == NULL)
        {
          unsigned int xwcm;
          XWindowChanges xwc;
          
          xwcm = event->xconfigurerequest.value_mask &
            (CWX | CWY | CWWidth | CWHeight | CWBorderWidth);

          xwc.x = event->xconfigurerequest.x;
          xwc.y = event->xconfigurerequest.y;
          xwc.width = event->xconfigurerequest.width;
          xwc.height = event->xconfigurerequest.height;
          xwc.border_width = event->xconfigurerequest.border_width;

          XConfigureWindow (display->xdisplay, event->xconfigurerequest.window,
                            xwcm, &xwc);
        }
      else
        {
          meta_window_configure_request (window, event);
        }
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
      if (window)
        meta_window_property_notify (window, event);
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
}

/* Return the window this has to do with, if any, rather
 * than the frame or root window that was selecting
 * for substructure
 */
static Window
event_get_modified_window (MetaDisplay *display,
                           XEvent *event)
{
  switch (event->type)
    {
    case KeyPress:
    case KeyRelease:
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
    case EnterNotify:
    case LeaveNotify:
    case FocusIn:
    case FocusOut:
    case KeymapNotify:
    case Expose:
    case GraphicsExpose:
    case NoExpose:
    case VisibilityNotify:
    case ResizeRequest:
    case PropertyNotify:
    case SelectionClear:
    case SelectionRequest:
    case SelectionNotify:
    case ColormapNotify:
    case ClientMessage:
      return event->xany.window;

    case CreateNotify:
      return event->xcreatewindow.window;
      
    case DestroyNotify:
      return event->xdestroywindow.window;

    case UnmapNotify:
      return event->xunmap.window;

    case MapNotify:
      return event->xmap.window;

    case MapRequest:
      return event->xmaprequest.window;

    case ReparentNotify:
     return event->xreparent.window;
      
    case ConfigureNotify:
      return event->xconfigure.window;
      
    case ConfigureRequest:
      return event->xconfigurerequest.window;

    case GravityNotify:
      return event->xgravity.window;

    case CirculateNotify:
      return event->xcirculate.window;

    case CirculateRequest:
      return event->xcirculaterequest.window;

    case MappingNotify:
      return None;

    default:
      return None;
    }
}
  
static void
meta_spew_event (MetaDisplay *display,
                 XEvent      *event)
{

      const char *name = NULL;
      char *extra = NULL;
      char *winname;
      MetaScreen *screen;
      
      switch (event->type)
        {
        case KeyPress:
          name = "KeyPress";      
          break;
        case KeyRelease:
          name = "KeyRelease";
          break;
        case ButtonPress:
          name = "ButtonPress";
          break;
        case ButtonRelease:
          name = "ButtonRelease";
          break;
        case MotionNotify:
          name = "MotionNotify";
          break;
        case EnterNotify:
          name = "EnterNotify";
          break;
        case LeaveNotify:
          name = "LeaveNotify";
          break;
        case FocusIn:
          name = "FocusIn";
          break;
        case FocusOut:
          name = "FocusOut";
          break;
        case KeymapNotify:
          name = "KeymapNotify";
          break;
        case Expose:
          name = "Expose";
          break;
        case GraphicsExpose:
          name = "GraphicsExpose";
          break;
        case NoExpose:
          name = "NoExpose";
          break;
        case VisibilityNotify:
          name = "VisibilityNotify";
          break;
        case CreateNotify:
          name = "CreateNotify";
          break;
        case DestroyNotify:
          name = "DestroyNotify";
          break;
        case UnmapNotify:
          name = "UnmapNotify";
          break;
        case MapNotify:
          name = "MapNotify";
          break;
        case MapRequest:
          name = "MapRequest";
          break;
        case ReparentNotify:
          name = "ReparentNotify";
          break;
        case ConfigureNotify:
          name = "ConfigureNotify";
          extra = g_strdup_printf ("x: %d y: %d w: %d h: %d above: 0x%lx",
                                   event->xconfigure.x,
                                   event->xconfigure.y,
                                   event->xconfigure.width,
                                   event->xconfigure.height,
                                   event->xconfigure.above);
          break;
        case ConfigureRequest:
          name = "ConfigureRequest";
          extra = g_strdup_printf ("parent: 0x%lx window: 0x%lx x: %d y: %d w: %d h: %d border: %d",
                                   event->xconfigurerequest.parent,
                                   event->xconfigurerequest.window,
                                   event->xconfigurerequest.x,
                                   event->xconfigurerequest.y,
                                   event->xconfigurerequest.width,
                                   event->xconfigurerequest.height,
                                   event->xconfigurerequest.border_width);
          break;
        case GravityNotify:
          name = "GravityNotify";
          break;
        case ResizeRequest:
          name = "ResizeRequest";
          break;
        case CirculateNotify:
          name = "CirculateNotify";
          break;
        case CirculateRequest:
          name = "CirculateRequest";
          break;
        case PropertyNotify:
          name = "PropertyNotify";
          break;
        case SelectionClear:
          name = "SelectionClear";
          break;
        case SelectionRequest:
          name = "SelectionRequest";
          break;
        case SelectionNotify:
          name = "SelectionNotify";
          break;
        case ColormapNotify:
          name = "ColormapNotify";
          break;
        case ClientMessage:
          name = "ClientMessage";
          break;
        case MappingNotify:
          name = "MappingNotify";
          break;
        default:
          name = "Unknown";
          break;
        }

      screen = meta_display_screen_for_root (display, event->xany.window);
      
      if (screen)
        winname = g_strdup_printf ("root %d", screen->number);
      else
        winname = g_strdup_printf ("0x%lx", event->xany.window);
      
      meta_verbose ("%s on %s%s %s\n", name, winname,
                    extra ? ":" : "", extra ? extra : "");

      g_free (winname);

      if (extra)
        g_free (extra);
}

MetaWindow*
meta_display_lookup_x_window (MetaDisplay *display,
                              Window       xwindow)
{
  return g_hash_table_lookup (display->window_ids, &xwindow);
}

void
meta_display_register_x_window (MetaDisplay *display,
                                Window      *xwindowp,
                                MetaWindow  *window)
{
  g_return_if_fail (g_hash_table_lookup (display->window_ids, xwindowp) == NULL);
  
  g_hash_table_insert (display->window_ids, xwindowp, window);
}

void
meta_display_unregister_x_window (MetaDisplay *display,
                                  Window       xwindow)
{
  g_return_if_fail (g_hash_table_lookup (display->window_ids, &xwindow) != NULL);

  g_hash_table_remove (display->window_ids, &xwindow);
}
