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
#include "errors.h"
#include "keybindings.h"
#include "workspace.h"
#include <X11/Xatom.h>
#include <string.h>

#define USE_GDK_DISPLAY

static GSList *all_displays = NULL;
static void   meta_spew_event           (MetaDisplay    *display,
                                         XEvent         *event);
static void   event_queue_callback      (XEvent         *event,
                                         gpointer        data);
static gboolean event_callback          (XEvent         *event,
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

static int
set_string_hint (MetaDisplay *display,
                 Window xwindow,
                 Atom atom,
                 const char *val)
{
  meta_error_trap_push (display);
  XChangeProperty (display->xdisplay, 
                   xwindow, atom,
                   XA_STRING,
                   8, PropModeReplace, (guchar*) val, strlen (val) + 1);
  return meta_error_trap_pop (display);
}

gboolean
meta_display_open (const char *name)
{
  MetaDisplay *display;
  Display *xdisplay;
  GSList *screens;
  GSList *tmp;
  int i;
  /* Remember to edit code that assigns each atom to display struct
   * when adding an atom name here.
   */
  char *atom_names[] = {
    "_NET_WM_NAME",
    "WM_PROTOCOLS",
    "WM_TAKE_FOCUS",
    "WM_DELETE_WINDOW",
    "WM_STATE",
    "_NET_CLOSE_WINDOW",
    "_NET_WM_STATE",
    "_MOTIF_WM_HINTS",
    "_NET_WM_STATE_SHADED",
    "_NET_WM_STATE_MAXIMIZED_HORZ",
    "_NET_WM_STATE_MAXIMIZED_VERT",
    "_NET_WM_DESKTOP",
    "_NET_NUMBER_OF_DESKTOPS",
    "WM_CHANGE_STATE",
    "SM_CLIENT_ID",
    "WM_CLIENT_LEADER",
    "WM_WINDOW_ROLE",
    "_NET_CURRENT_DESKTOP",
    "_NET_SUPPORTING_WM_CHECK",
    "_NET_WM_SUPPORTED",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DESKTOP",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_WINDOW_TYPE_TOOLBAR",
    "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_STATE_MODAL",
    "_NET_CLIENT_LIST",
    "_NET_CLIENT_LIST_STACKING",
    "_NET_WM_STATE_SKIP_TASKBAR",
    "_NET_WM_STATE_SKIP_PAGER",
    "_WIN_WORKSPACE",
    "_WIN_LAYER",
    "_WIN_PROTOCOLS",
    "_WIN_SUPPORTING_WM_CHECK"
  };
  Atom atoms[G_N_ELEMENTS(atom_names)];
  
  meta_verbose ("Opening display '%s'\n", XDisplayName (name));

#ifdef USE_GDK_DISPLAY
  xdisplay = meta_ui_get_display (name);
#else
  xdisplay = XOpenDisplay (name);
#endif
  
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
  display->server_grab_count = 0;
  display->workspaces = NULL;
  
  /* we have to go ahead and do this so error handlers work */
  all_displays = g_slist_prepend (all_displays, display);

  meta_display_init_keys (display);

  XInternAtoms (display->xdisplay, atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);
  display->atom_net_wm_name = atoms[0];
  display->atom_wm_protocols = atoms[1];
  display->atom_wm_take_focus = atoms[2];
  display->atom_wm_delete_window = atoms[3];
  display->atom_wm_state = atoms[4];
  display->atom_net_close_window = atoms[5];
  display->atom_net_wm_state = atoms[6];
  display->atom_motif_wm_hints = atoms[7];
  display->atom_net_wm_state_shaded = atoms[8];
  display->atom_net_wm_state_maximized_horz = atoms[9];
  display->atom_net_wm_state_maximized_vert = atoms[10];
  display->atom_net_wm_desktop = atoms[11];
  display->atom_net_number_of_desktops = atoms[12];
  display->atom_wm_change_state = atoms[13];
  display->atom_sm_client_id = atoms[14];
  display->atom_wm_client_leader = atoms[15];
  display->atom_wm_window_role = atoms[16];
  display->atom_net_current_desktop = atoms[17];
  display->atom_net_supporting_wm_check = atoms[18];
  display->atom_net_wm_supported = atoms[19];
  display->atom_net_wm_window_type = atoms[20];
  display->atom_net_wm_window_type_desktop = atoms[21];
  display->atom_net_wm_window_type_dock = atoms[22];
  display->atom_net_wm_window_type_toolbar = atoms[23];
  display->atom_net_wm_window_type_menu = atoms[24];
  display->atom_net_wm_window_type_dialog = atoms[25];
  display->atom_net_wm_window_type_normal = atoms[26];
  display->atom_net_wm_state_modal = atoms[27];
  display->atom_net_client_list = atoms[28];
  display->atom_net_client_list_stacking = atoms[29];
  display->atom_net_wm_state_skip_taskbar = atoms[30];
  display->atom_net_wm_state_skip_pager = atoms[31];
  display->atom_win_workspace = atoms[32];
  display->atom_win_layer = atoms[33];
  display->atom_win_protocols = atoms[34];
  display->atom_win_supporting_wm_check = atoms[35];
  
  /* Offscreen unmapped window used for _NET_SUPPORTING_WM_CHECK,
   * created in screen_new
   */
  display->leader_window = None;

  screens = NULL;
#if 0
  /* disable multihead pending GTK support */
  i = 0;
  while (i < ScreenCount (xdisplay))
    {
      MetaScreen *screen;

      screen = meta_screen_new (display, i);

      if (screen)
        screens = g_slist_prepend (screens, screen);
      ++i;
    }
#else
  {
    MetaScreen *screen;
    screen = meta_screen_new (display, DefaultScreen (xdisplay));
    if (screen)
      screens = g_slist_prepend (screens, screen);
  }
#endif

  if (screens == NULL)
    {
      /* This would typically happen because all the screens already
       * have window managers
       */
#ifndef USE_GDK_DISPLAY
      XCloseDisplay (xdisplay);
#endif
      all_displays = g_slist_remove (all_displays, display);
      g_free (display->name);
      g_free (display);
      return FALSE;
    }
  
  display->screens = screens;

#ifdef USE_GDK_DISPLAY
  display->events = NULL;

  /* Get events */
  meta_ui_add_event_func (display->xdisplay,
                          event_callback,
                          display);
#else
  display->events = meta_event_queue_new (display->xdisplay,
                                          event_queue_callback,
                                          display);
#endif
  
  display->window_ids = g_hash_table_new (unsigned_long_hash, unsigned_long_equal);
  
  display->double_click_time = 250;
  display->last_button_time = 0;
  display->last_button_xwindow = None;
  display->last_button_num = 0;
  display->is_double_click = FALSE;

  set_string_hint (display,
                   display->leader_window,
                   display->atom_net_wm_name,
                   "Metacity");

  {
    /* The legacy GNOME hint is to set a cardinal which is the window
     * id of the supporting_wm_check window on the supporting_wm_check
     * window itself
     */
    gulong data[1];

    data[0] = display->leader_window;
    XChangeProperty (display->xdisplay,
                     display->leader_window,
                     display->atom_win_supporting_wm_check,
                     XA_CARDINAL,
                     32, PropModeReplace, (guchar*) data, 1);
  }
  
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

  winlist = g_slist_sort (winlist, ptrcmp);

  /* Unmanage all windows */
  meta_display_grab (display);
  tmp = winlist;
  while (tmp != NULL)
    {      
      if (tmp->next == NULL ||
          (tmp->next && tmp->next->data != tmp->data))
        meta_window_free (tmp->data);
      
      tmp = tmp->next;
    }
  g_slist_free (winlist);
  meta_display_ungrab (display);

#ifdef USE_GDK_DISPLAY
  /* Stop caring about events */
  meta_ui_remove_event_func (display->xdisplay,
                             event_callback,
                             display);
#endif
  
  /* Must be after all calls to meta_window_free() since they
   * unregister windows
   */
  g_hash_table_destroy (display->window_ids);

  if (display->leader_window != None)
    XDestroyWindow (display->xdisplay, display->leader_window);

#ifndef USE_GDK_DISPLAY
  meta_event_queue_free (display->events);
  XCloseDisplay (display->xdisplay);
#endif 
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
  meta_verbose ("Grabbing display, grab count now %d\n",
                display->server_grab_count);
}

void
meta_display_ungrab (MetaDisplay *display)
{
  if (display->server_grab_count == 0)
    meta_bug ("Ungrabbed non-grabbed server\n");
  
  display->server_grab_count -= 1;
  if (display->server_grab_count == 0)
    {
      /* FIXME we want to purge all pending "queued" stuff
       * at this point, such as window hide/show
       */
      XSync (display->xdisplay, False);
      XUngrabServer (display->xdisplay);
    }
  XSync (display->xdisplay, False);

  meta_verbose ("Ungrabbing display, grab count now %d\n",
                display->server_grab_count);
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

gboolean
meta_display_is_double_click (MetaDisplay *display)
{
  return display->is_double_click;
}

static gboolean dump_events = TRUE;


static void
event_queue_callback (XEvent         *event,
                      gpointer        data)
{
  event_callback (event, data);
}

static gboolean
event_callback (XEvent   *event,
                gpointer  data)
{
  MetaWindow *window;
  MetaDisplay *display;
  Window modified;
  
  display = data;
  
  if (dump_events)
    meta_spew_event (display, event);
  
  /* mark double click events, kind of a hack, oh well. */
  if (event->type == ButtonPress)
    {
      if (event->xbutton.button == display->last_button_num &&
          event->xbutton.window == display->last_button_xwindow &&
          event->xbutton.time < (display->last_button_time + display->double_click_time))
        {
          display->is_double_click = TRUE;
          meta_verbose ("This was the second click of a double click\n");
        }
      else
        {
          display->is_double_click = FALSE;
        }

      display->last_button_num = event->xbutton.button;
      display->last_button_xwindow = event->xbutton.window;
      display->last_button_time = event->xbutton.time;
    }
  
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
      return FALSE;
    }
  
  switch (event->type)
    {
    case KeyPress:
      meta_display_process_key_press (display, window, event);
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
      /* We handle it here if an undecorated window
       * is involved, otherwise we handle it in frame.c
       */
      /* do this even if window->has_focus to avoid races */
      if (window)
        meta_window_focus (window, event->xcrossing.time);
      break;
    case LeaveNotify:
      break;
    case FocusIn:
    case FocusOut:
      if (window)
        meta_window_notify_focus (window, event);
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
        {
          if (window->unmaps_pending == 0)
            {
              meta_verbose ("Window %s withdrawn\n",
                            window->desc);
              meta_window_free (window); /* Unmanage withdrawn window */
            }
          else
            {
              window->unmaps_pending -= 1;
              meta_verbose ("Received pending unmap, %d now pending\n",
                            window->unmaps_pending);
            }
        }
      break;
    case MapNotify:
      break;
    case MapRequest:
      if (window == NULL)
        window = meta_window_new (display, event->xmaprequest.window, FALSE);
      else if (window)
        {
          if (window->minimized)
            meta_window_unminimize (window);
        }
      break;
    case ReparentNotify:
      break;
    case ConfigureNotify:
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

          meta_verbose ("Configuring withdrawn window to %d,%d %dx%d border %d (some values may not be in mask)\n",
                        xwc.x, xwc.y, xwc.width, xwc.height, xwc.border_width);
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
      if (window)
        {
          meta_window_client_message (window, event);
        }
      else
        {
          MetaScreen *screen;

          screen = meta_display_screen_for_root (display,
                                                 event->xclient.window);
          
          if (screen &&
              event->xclient.message_type ==
              display->atom_net_current_desktop)
            {
              int space;
              MetaWorkspace *workspace;
              
              space = event->xclient.data.l[0];
              
              meta_verbose ("Request to change current workspace to %d\n",
                            space);
              
              workspace =
                meta_display_get_workspace_by_screen_index (display,
                                                            screen,
                                                            space);

              if (workspace)
                meta_workspace_activate (workspace);
              else
                meta_verbose ("Don't know about workspace %d\n", space);
            }
        }
      break;
    case MappingNotify:
      break;
    default:
      break;
    }

  return FALSE;
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
    case EnterNotify:
    case LeaveNotify:
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

static const char*
focus_detail (int d)
{
  const char *detail = "???";
  switch (d)
    {
    case NotifyAncestor:
      detail = "NotifyAncestor";
      break;
    case NotifyDetailNone:
      detail = "NotifyDetailNone";
      break;
    case NotifyInferior:
      detail = "NotifyInferior";
      break;                
    case NotifyNonlinear:
      detail = "NotifyNonlinear";
      break;
    case NotifyNonlinearVirtual:
      detail = "NotifyNonlinearVirtual";
      break;
    case NotifyPointer:
      detail = "NotifyPointer";
      break;
    case NotifyPointerRoot:
      detail = "NotifyPointerRoot";
      break;
    case NotifyVirtual:
      detail = "NotifyVirtual";
      break;
    }

  return detail;
}

static const char*
focus_mode (int m)
{
  const char *mode = "???";
  switch (m)
    {
    case NotifyNormal:
      mode = "NotifyNormal";
      break;
    case NotifyGrab:
      mode = "NotifyGrab";
      break;
    case NotifyUngrab:
      mode = "NotifyUngrab";
      break;
    }

  return mode;
}

static void
meta_spew_event (MetaDisplay *display,
                 XEvent      *event)
{

      const char *name = NULL;
      char *extra = NULL;
      char *winname;
      MetaScreen *screen;

      /* filter overnumerous events */
      if (event->type == Expose || event->type == MotionNotify ||
          event->type == NoExpose)
        return;
      
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
          extra = g_strdup_printf ("win: 0x%lx root: 0x%lx subwindow: 0x%lx mode: %d detail: %d",
                                   event->xcrossing.window,
                                   event->xcrossing.root,
                                   event->xcrossing.subwindow,
                                   event->xcrossing.mode,
                                   event->xcrossing.detail);
          break;
        case LeaveNotify:
          name = "LeaveNotify";
          extra = g_strdup_printf ("win: 0x%lx root: 0x%lx subwindow: 0x%lx mode: %d detail: %d",
                                   event->xcrossing.window,
                                   event->xcrossing.root,
                                   event->xcrossing.subwindow,
                                   event->xcrossing.mode,
                                   event->xcrossing.detail);
          break;
        case FocusIn:
          name = "FocusIn";
          extra = g_strdup_printf ("detail: %s mode: %s\n",
                                   focus_detail (event->xfocus.detail),
                                   focus_mode (event->xfocus.mode));
          break;
        case FocusOut:
          name = "FocusOut";
          extra = g_strdup_printf ("detail: %s mode: %s\n",
                                   focus_detail (event->xfocus.detail),
                                   focus_mode (event->xfocus.mode));
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
          extra = g_strdup_printf ("parent: 0x%lx window: 0x%lx x: %d %sy: %d %sw: %d %sh: %d %sborder: %d %s",
                                   event->xconfigurerequest.parent,
                                   event->xconfigurerequest.window,
                                   event->xconfigurerequest.x,
                                   event->xconfigurerequest.value_mask &
                                   CWX ? "" : "(unset) ",
                                   event->xconfigurerequest.y,
                                   event->xconfigurerequest.value_mask &
                                   CWY ? "" : "(unset) ",
                                   event->xconfigurerequest.width,
                                   event->xconfigurerequest.value_mask &
                                   CWWidth ? "" : "(unset) ",
                                   event->xconfigurerequest.height,
                                   event->xconfigurerequest.value_mask &
                                   CWHeight ? "" : "(unset) ",
                                   event->xconfigurerequest.border_width,
                                   event->xconfigurerequest.value_mask &
                                   CWBorderWidth ? "" : "(unset)");
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
          {
            char *str;
            const char *state;
            
            name = "PropertyNotify";
            
            meta_error_trap_push (display);
            str = XGetAtomName (display->xdisplay,
                                event->xproperty.atom);
            meta_error_trap_pop (display);

            if (event->xproperty.state == PropertyNewValue)
              state = "PropertyNewValue";
            else if (event->xproperty.state == PropertyDelete)
              state = "PropertyDelete";
            else
              state = "???";
            
            extra = g_strdup_printf ("atom: %s state: %s",
                                     str ? str : "(unknown atom)",
                                     state);
            XFree (str);
          }
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
          {
            char *str;
            name = "ClientMessage";
            meta_error_trap_push (display);
            str = XGetAtomName (display->xdisplay,
                                event->xclient.message_type);
            meta_error_trap_pop (display);
            extra = g_strdup_printf ("type: %s format: %d\n",
                                     str ? str : "(unknown atom)",
                                     event->xclient.format);
            XFree (str);
          }
          break;
        case MappingNotify:
          name = "MappingNotify";
          break;
        default:
          name = "Unknown event type";
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

MetaWorkspace*
meta_display_get_workspace_by_index (MetaDisplay *display,
                                     int          index)
{
  GList *tmp;
  
  tmp = g_list_nth (display->workspaces, index);

  if (tmp == NULL)
    return NULL;
  else
    return tmp->data;
}

MetaWorkspace*
meta_display_get_workspace_by_screen_index (MetaDisplay *display,
                                            MetaScreen  *screen,
                                            int          index)
{
  GList *tmp;
  int i;

  i = 0;
  tmp = display->workspaces;
  while (tmp != NULL)
    {
      MetaWorkspace *w = tmp->data;

      if (w->screen == screen)
        {
          if (i == index)
            return w;
          else
            ++i;
        }
      
      tmp = tmp->next;
    }

  return NULL;
}
