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

static GSList *all_displays = NULL;

static void meta_spew_event      (MetaDisplay    *display,
                                  XEvent         *event);
static void event_queue_callback (MetaEventQueue *queue,
                                  XEvent         *event,
                                  gpointer        data);


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
free_window (gpointer key, gpointer value, gpointer data)
{
  MetaWindow *window;

  window = value;

  meta_window_free (window);
}

void
meta_display_close (MetaDisplay *display)
{
  if (display->error_traps)
    meta_bug ("Display closed with error traps pending\n");
  
  g_hash_table_foreach (display->window_ids,
                        free_window,
                        NULL);
  
  g_hash_table_destroy (display->window_ids);
  
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
  gboolean is_root;
  
  display = data;
  
  if (dump_events)
    meta_spew_event (display, event);

  is_root = meta_display_screen_for_root (display, event->xany.window) != NULL;  
  window = NULL;
  
  if (!is_root)
    {
      if (window == NULL)
        window = meta_display_lookup_x_window (display, event->xany.window);
  
      if (window != NULL)
        {
          if (meta_window_event (window, event))
            return;
        }
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
      break;
    case UnmapNotify:
      break;
    case MapNotify:
      break;
    case MapRequest:
      if (is_root && !event->xmap.override_redirect)
        {
          /* Window requested mapping. Manage it if we haven't. Note that
           * meta_window_new() can return NULL
           */
          window = meta_display_lookup_x_window (display,
                                                 event->xmaprequest.window);
          if (window == NULL)
            window = meta_window_new (display, event->xmaprequest.window);
        }
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
