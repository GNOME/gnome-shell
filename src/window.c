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
#include "workspace.h"

#include <X11/Xatom.h>

static void     constrain_size            (MetaWindow     *window,
                                           int             width,
                                           int             height,
                                           int            *new_width,
                                           int            *new_height);
static int      update_size_hints         (MetaWindow     *window);
static int      update_title              (MetaWindow     *window);
static int      update_protocols          (MetaWindow     *window);
static int      update_wm_hints           (MetaWindow     *window);
static int      update_net_wm_state       (MetaWindow     *window);
static int      update_mwm_hints          (MetaWindow     *window);
static int      set_wm_state              (MetaWindow     *window,
                                           int             state);
static void     send_configure_notify     (MetaWindow     *window);
static gboolean process_configure_request (MetaWindow     *window,
                                           int             x,
                                           int             y,
                                           int             width,
                                           int             height,
                                           int             border_width);
static gboolean process_property_notify   (MetaWindow     *window,
                                           XPropertyEvent *event);
static void     meta_window_show          (MetaWindow     *window);
static void     meta_window_hide          (MetaWindow     *window);


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
                PropertyChangeMask |
                EnterWindowMask | LeaveWindowMask |
                FocusChangeMask);  

  /* Get rid of any borders */
  if (attrs.border_width != 0)
    XSetWindowBorderWidth (display->xdisplay, xwindow, 0);
  
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
  window->workspaces = NULL;
  
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

  /* Remember this rect is the actual window size */
  window->rect.x = attrs.x;
  window->rect.y = attrs.y;
  window->rect.width = attrs.width;
  window->rect.height = attrs.height;

  /* And border width, size_hints are the "request" */
  window->border_width = attrs.border_width;
  window->size_hints.x = attrs.x;
  window->size_hints.y = attrs.y;
  window->size_hints.width = attrs.width;
  window->size_hints.height = attrs.height;

  /* And this is our unmaximized size */
  window->saved_rect = window->rect;
  
  window->depth = attrs.depth;
  window->xvisual = attrs.visual;

  window->title = NULL;

  window->desc = g_strdup_printf ("0x%lx", window->xwindow);

  window->frame = NULL;
  window->has_focus = FALSE;

  window->maximized = FALSE;
  window->shaded = FALSE;
  window->initially_iconic = FALSE;
  window->minimized = FALSE;
  window->iconic = FALSE;
  window->mapped = FALSE;

  window->decorated = TRUE;
  window->has_close_func = TRUE;
  window->has_minimize_func = TRUE;
  window->has_maximize_func = TRUE;
  
  meta_display_register_x_window (display, &window->xwindow, window);

  update_size_hints (window);
  update_title (window);
  update_protocols (window);  
  update_wm_hints (window);
  update_net_wm_state (window);
  update_mwm_hints (window);
  
  if (window->initially_iconic)
    {
      /* WM_HINTS said minimized */
      window->minimized = TRUE;
      meta_verbose ("Window %s asked to start out minimized\n", window->desc);
    }
  
  meta_window_resize (window,
                      window->size_hints.width,
                      window->size_hints.height);

  /* FIXME we have a tendency to set this then immediately
   * change it again.
   */
  set_wm_state (window, window->iconic ? IconicState : NormalState);

  if (window->decorated)
    meta_window_ensure_frame (window);

  meta_workspace_add_window (window->screen->active_workspace, window);
  
  /* Put our state back where it should be */
  meta_window_queue_calc_showing (window);
  
  return window;
}

void
meta_window_free (MetaWindow  *window)
{
  GList *tmp;
  
  meta_verbose ("Unmanaging 0x%lx\n", window->xwindow);

  tmp = window->workspaces;
  while (tmp != NULL)
    {
      GList *next;

      next = tmp->next;

      /* pops front of list */
      meta_workspace_remove_window (tmp->data, window);

      tmp = next;
    }

  g_assert (window->workspaces == NULL);

  /* FIXME restore original size if window has maximized */
  
  set_wm_state (window, WithdrawnState);  
  
  meta_display_unregister_x_window (window->display, window->xwindow);

  meta_window_destroy_frame (window);

  /* Put back anything we messed up */
  meta_error_trap_push (window->display);
  if (window->border_width != 0)
    XSetWindowBorderWidth (window->display->xdisplay,
                           window->xwindow,
                           window->border_width);
  
  meta_error_trap_pop (window->display);
  
  g_free (window->title);
  g_free (window->desc);
  g_free (window);
}

static int
set_wm_state (MetaWindow *window,
              int         state)
{
  unsigned long data[1];

  /* twm sets the icon window as data[1], I couldn't find that in
   * ICCCM.
   */
  data[0] = state;

  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_wm_state,
                   window->display->atom_wm_state,
                   32, PropModeReplace, (guchar*) data, 1);
  return meta_error_trap_pop (window->display);
}

void
meta_window_calc_showing (MetaWindow  *window)
{
  gboolean on_workspace;
  
  on_workspace = g_list_find (window->workspaces,
                              window->screen->active_workspace) != NULL;

  if (!on_workspace)
    meta_verbose ("Window %s is not on workspace %d\n",
                  window->desc,
                  meta_workspace_index (window->screen->active_workspace));
  else
    meta_verbose ("Window %s is on the active workspace %d\n",
                  window->desc,
                  meta_workspace_index (window->screen->active_workspace));
  
  if (window->minimized || !on_workspace)
    {
      meta_window_hide (window);
    }
  else
    {
      meta_window_show (window);
    }
}
     
void
meta_window_queue_calc_showing (MetaWindow  *window)
{
  /* FIXME */
  meta_window_calc_showing (window);
}

void
meta_window_show (MetaWindow *window)
{
  meta_verbose ("Showing window %s, shaded: %d iconic: %d\n",
                window->desc, window->shaded, window->iconic);

  /* Shaded means the frame is mapped but the window is not */
  
  if (window->frame)
    XMapWindow (window->display->xdisplay, window->frame->xwindow);

  if (window->shaded)
    {
      window->mapped = FALSE;
      meta_error_trap_push (window->display);
      XUnmapWindow (window->display->xdisplay, window->xwindow);
      meta_error_trap_pop (window->display);
      
      if (!window->iconic)
        {
          window->iconic = TRUE;
          set_wm_state (window, IconicState);
        }
    }
  else
    {
      window->mapped = TRUE;
      meta_error_trap_push (window->display);
      XMapWindow (window->display->xdisplay, window->xwindow);
      meta_error_trap_pop (window->display);
      
      if (window->iconic)
        {
          window->iconic = FALSE;
          set_wm_state (window, NormalState);
        }
    }
}

void
meta_window_hide (MetaWindow *window)
{
  meta_verbose ("Hiding window %s\n", window->desc);
  
  if (window->frame)
    XUnmapWindow (window->display->xdisplay, window->frame->xwindow);
  XUnmapWindow (window->display->xdisplay, window->xwindow);

  window->mapped = FALSE;

  if (!window->iconic)
    {
      window->iconic = TRUE;
      set_wm_state (window, IconicState);
    }
}

void
meta_window_minimize (MetaWindow  *window)
{
  if (!window->minimized)
    {
      window->minimized = TRUE;
      meta_window_queue_calc_showing (window);
    }
}

void
meta_window_unminimize (MetaWindow  *window)
{
  if (window->minimized)
    {
      window->minimized = FALSE;
      meta_window_queue_calc_showing (window);
    }
}

void
meta_window_maximize (MetaWindow  *window)
{
  if (!window->maximized)
    {
      int x, y;
      
      window->maximized = TRUE;
      
      /* save size/pos */
      window->saved_rect = window->rect;
      if (window->frame)
        {
          window->saved_rect.x += window->frame->rect.x;
          window->saved_rect.y += window->frame->rect.y;
        }

      /* find top left corner */
      x = window->screen->active_workspace->workarea.x;
      y = window->screen->active_workspace->workarea.y;
      if (window->frame)
        {
          x += window->frame->child_x;
          y += window->frame->child_y;
        }

      
      /* resize to current size with new maximization constraint,
       * and move to top-left corner
       */
      
      meta_window_move_resize (window, x, y,
                               window->rect.width, window->rect.height);
    }
}

void
meta_window_unmaximize (MetaWindow  *window)
{
  if (window->maximized)
    {
      window->maximized = FALSE;

      meta_window_move_resize (window,
                               window->saved_rect.x,
                               window->saved_rect.y,
                               window->saved_rect.width,
                               window->saved_rect.height);
    }
}

void
meta_window_shade (MetaWindow  *window)
{
  if (!window->shaded)
    {
      window->shaded = TRUE;
      if (window->frame)
        meta_frame_queue_recalc (window->frame);
      meta_window_queue_calc_showing (window);
    }
}

void
meta_window_unshade (MetaWindow  *window)
{
  if (window->shaded)
    {
      window->shaded = FALSE;
      if (window->frame)
        meta_frame_queue_recalc (window->frame);
      meta_window_queue_calc_showing (window);
    }
}

static void
meta_window_move_resize_internal (MetaWindow  *window,
                                  gboolean     move,
                                  gboolean     resize,
                                  int          root_x_nw,
                                  int          root_y_nw,
                                  int          w,
                                  int          h)
{  
  if (resize)
    meta_verbose ("Resizing %s to %d x %d\n", window->desc, w, h);
  if (move)
    meta_verbose ("Moving %s to %d,%d\n", window->desc,
                  root_x_nw, root_y_nw);

  if (resize)
    {
      constrain_size (window, w, h, &w, &h);
      meta_verbose ("Constrained resize of %s to %d x %d\n", window->desc, w, h);
      if (w == window->rect.width &&
          h == window->rect.height)
        resize = FALSE;

      window->rect.width = w;
      window->rect.height = h;
    }

  if (move)
    {
      if (window->frame)
        {
          int new_x, new_y;
          
          new_x = root_x_nw - window->frame->child_x;
          new_y = root_y_nw - window->frame->child_y;
      
          if (new_x == window->frame->rect.x &&
              new_y == window->frame->rect.y)
            move = FALSE;
          
          window->frame->rect.x = new_x;
          window->frame->rect.y = new_y;
          /* window->rect.x, window->rect.y remain relative to frame,
           * remember they are the server coords
           */
        }
      else
        {
          if (root_x_nw == window->rect.x &&
              root_y_nw == window->rect.y)
            move = FALSE;

          window->rect.x = root_x_nw;
          window->rect.y = root_y_nw;
        }
    }

  /* Sync our new size/pos with X as efficiently as possible */

  if (move && window->frame)
    {
      XMoveWindow (window->display->xdisplay,
                   window->frame->xwindow,
                   window->frame->rect.x,
                   window->frame->rect.y);
    }

  meta_error_trap_push (window->display);
  if ((move && window->frame == NULL) && resize)
    {
      XMoveResizeWindow (window->display->xdisplay,
                         window->xwindow,
                         window->rect.x,
                         window->rect.y,
                         window->rect.width,
                         window->rect.height);
    }
  else if (move && window->frame == NULL)
    {
      XMoveWindow (window->display->xdisplay,
                   window->xwindow,
                   window->rect.x,
                   window->rect.y);
    }
  else if (resize)
    {
      XResizeWindow (window->display->xdisplay,
                     window->xwindow,
                     w, h);

    }
  meta_error_trap_pop (window->display);
  
  if (move)
    send_configure_notify (window);

  if (window->frame && resize)
    meta_frame_queue_recalc (window->frame);
}

void
meta_window_resize (MetaWindow  *window,
                    int          w,
                    int          h)
{
  meta_window_move_resize_internal (window, FALSE, TRUE, -1, -1, w, h);
}

void
meta_window_move (MetaWindow  *window,
                  int          root_x_nw,
                  int          root_y_nw)
{
  meta_window_move_resize_internal (window, TRUE, FALSE,
                                    root_x_nw, root_y_nw, -1, -1);
}

void
meta_window_move_resize (MetaWindow  *window,
                         int          root_x_nw,
                         int          root_y_nw,
                         int          w,
                         int          h)
{
  meta_window_move_resize_internal (window, TRUE, TRUE,
                                    root_x_nw, root_y_nw,
                                    w, h);
}

void
meta_window_delete (MetaWindow  *window,
                    Time         timestamp)
{
  meta_error_trap_push (window->display);
  if (window->delete_window)
    {
      meta_verbose ("Deleting %s with delete_window request\n",
                    window->desc);
      meta_window_send_icccm_message (window,
                                      window->display->atom_wm_delete_window,
                                      timestamp);
    }
  else
    {
      meta_verbose ("Deleting %s with explicit kill\n",
                    window->desc);
      XKillClient (window->display->xdisplay, window->xwindow);
    }
  meta_error_trap_pop (window->display);
}

void
meta_window_focus (MetaWindow  *window,
                   Time         timestamp)
{
  meta_verbose ("Setting input focus to window %s, input: %d take_focus: %d\n",
                window->desc, window->input, window->take_focus);
  
  if (window->input)
    {
      meta_error_trap_push (window->display);
      if (window->take_focus)
        {
          meta_window_send_icccm_message (window,
                                          window->display->atom_wm_take_focus,
                                          timestamp);
        }
      else
        {
          XSetInputFocus (window->display->xdisplay,
                          window->xwindow,
                          RevertToParent,
                          timestamp);
        }
      meta_error_trap_pop (window->display);
    }
}

void
meta_window_change_workspace (MetaWindow    *window,
                              MetaWorkspace *workspace)
{
  meta_verbose ("Changing window %s to workspace %d\n",
                window->desc, meta_workspace_index (workspace));
  
  /* See if we're already on this space */
  if (g_list_find (window->workspaces, workspace) != NULL)
    {
      meta_verbose ("Already on this workspace\n");
      return;
    }

  /* Add first, to maintain invariant that we're always
   * on some workspace.
   */
  meta_workspace_add_window (workspace, window);

  /* Lamely rely on prepend */
  g_assert (window->workspaces->data == workspace);  
  
  /* Remove from all other spaces */
  while (window->workspaces->next) /* while list size > 1 */
    meta_workspace_remove_window (window->workspaces->next->data, window);
}

void
meta_window_raise (MetaWindow  *window)
{
  meta_verbose ("Raising window %s\n", window->desc);

  if (window->frame == NULL)
    {
      meta_error_trap_push (window->display);
      
      XRaiseWindow (window->display->xdisplay, window->xwindow);
      
      meta_error_trap_pop (window->display);
    }
  else
    {
      XRaiseWindow (window->display->xdisplay,
                    window->frame->xwindow);
    }
}

void
meta_window_send_icccm_message (MetaWindow *window,
                                Atom        atom,
                                Time        timestamp)
{
  /* This comment and code are from twm, copyright
   * Open Group, Evans & Sutherland, etc.
   */
  
  /*
   * ICCCM Client Messages - Section 4.2.8 of the ICCCM dictates that all
   * client messages will have the following form:
   *
   *     event type	ClientMessage
   *     message type	_XA_WM_PROTOCOLS
   *     window		tmp->w
   *     format		32
   *     data[0]		message atom
   *     data[1]		time stamp
   */
  
    XClientMessageEvent ev;

    /* This should always be error trapped. */
    g_return_if_fail (window->display->error_traps != NULL);
    
    ev.type = ClientMessage;
    ev.window = window->xwindow;
    ev.message_type = window->display->atom_wm_protocols;
    ev.format = 32;
    ev.data.l[0] = atom;
    ev.data.l[1] = timestamp;
    
    XSendEvent (window->display->xdisplay,
                window->xwindow, False, 0, (XEvent*) &ev);
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
  else if (event->atom == XA_WM_HINTS)
    {
      update_wm_hints (window);

      if (window->frame)
        meta_frame_queue_recalc (window->frame);
    }
  else if (event->atom == window->display->atom_motif_wm_hints)
    {
      update_mwm_hints (window);
      
      if (window->decorated)
        meta_window_ensure_frame (window);
      else
        meta_window_destroy_frame (window);

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
   * and give windows a border of 0
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

  if (text.nitems > 0)
    XFree (text.value);
  
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

          XFree (text.value);
        }
    }
  
  if (window->title == NULL)
    window->title = g_strdup ("");

  g_free (window->desc);
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
                       &n_protocols))
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

static int
update_wm_hints (MetaWindow *window)
{
  XWMHints *hints;

  /* Fill in defaults */
  window->input = FALSE;
  window->initially_iconic = FALSE;
  
  meta_error_trap_push (window->display);
  
  hints = XGetWMHints (window->display->xdisplay,
                       window->xwindow);
  if (hints)
    {
      window->input = (hints->flags & InputHint) != 0;

      window->initially_iconic = (hints->initial_state == IconicState);
      
      /* FIXME there are a few others there. */

      meta_verbose ("Read WM_HINTS input: %d iconic: %d\n",
                    window->input, window->initially_iconic);
      
      XFree (hints);
    }
  
  return meta_error_trap_pop (window->display);
}

static int
update_net_wm_state (MetaWindow *window)
{
  /* We know this is only on initial window creation,
   * clients don't change the property.
   */
  Atom type;
  gint format;
  gulong n_atoms;
  gulong bytes_after;
  Atom *atoms;
  int result;
  int i;

  window->shaded = FALSE;
  window->maximized = FALSE;
  
  meta_error_trap_push (window->display);
  XGetWindowProperty (window->display->xdisplay, window->xwindow,
		      window->display->atom_net_wm_state,
                      0, G_MAXLONG,
		      False, XA_ATOM, &type, &format, &n_atoms,
		      &bytes_after, (guchar **)&atoms);  

  result = meta_error_trap_pop (window->display);
  if (result != Success)
    return result;
  
  if (type != XA_ATOM)
    return -1; /* whatever */

  i = 0;
  while (i < n_atoms)
    {
      if (atoms[i] == window->display->atom_net_wm_state_shaded)
        window->shaded = TRUE;
      else if (atoms[i] == window->display->atom_net_wm_state_maximized_horz)
        window->maximized = TRUE;
      else if (atoms[i] == window->display->atom_net_wm_state_maximized_vert)
        window->maximized = TRUE;
      
      ++i;
    }
  
  XFree (atoms);

  return Success;
}


/* I don't know of any docs anywhere on what the
 * hell most of this means. Copied from Lesstif by
 * way of GTK
 */
typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} MotifWmHints, MwmHints;

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

#define MWM_INPUT_MODELESS 0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL 2
#define MWM_INPUT_FULL_APPLICATION_MODAL 3
#define MWM_INPUT_APPLICATION_MODAL MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW	(1L<<0)

static int
update_mwm_hints (MetaWindow *window)
{
  MotifWmHints *hints;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  int result;

  window->decorated = TRUE;
  window->has_close_func = TRUE;
  window->has_minimize_func = TRUE;
  window->has_maximize_func = TRUE;
  
  meta_error_trap_push (window->display);
  XGetWindowProperty (window->display->xdisplay, window->xwindow,
		      window->display->atom_motif_wm_hints,
                      0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, (guchar **)&hints);

  result = meta_error_trap_pop (window->display);

  if (result != Success)
    return result;
  
  if (type == None)
    return -1; /* whatever */

  /* We support MWM hints deemed non-stupid */
  
  if (hints->flags & MWM_HINTS_DECORATIONS)
    {
      if (hints->decorations == 0)
        window->decorated = FALSE;
    }

  if (hints->flags & MWM_HINTS_FUNCTIONS)
    {
      if ((hints->functions & MWM_FUNC_CLOSE) == 0)
        window->has_close_func = FALSE;
      if ((hints->functions & MWM_FUNC_MINIMIZE) == 0)
        window->has_minimize_func = FALSE;
      if ((hints->functions & MWM_FUNC_MAXIMIZE) == 0)
        window->has_maximize_func = FALSE;
    }

  XFree (hints);
  
  return Success;
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
  int minw, minh, maxw, maxh, fullw, fullh;
  
#define FLOOR(value, base)	( ((gint) ((value) / (base))) * (base) )

  /* Get the allowed size ranges, considering maximized, etc. */
  fullw = window->screen->active_workspace->workarea.width;
  fullh = window->screen->active_workspace->workarea.height;
  if (window->frame)
    {
      fullw -= window->frame->child_x + window->frame->right_width;
      fullh -= window->frame->child_y + window->frame->bottom_height;
    }
  
  maxw = window->size_hints.max_width;
  maxh = window->size_hints.max_height;
  if (window->maximized)
    {
      maxw = MIN (maxw, fullw);
      maxh = MIN (maxh, fullh);
    }

  minw = window->size_hints.min_width;
  minh = window->size_hints.min_height;
  if (window->maximized)
    {
      minw = MAX (minw, fullw);
      minh = MAX (minh, fullh);
    }

  /* Check that fullscreen doesn't exceed max width hint,
   * if so then snap back to max width hint
   */
  if (minw > maxw)
    minw = maxw;
  if (minh > maxh)
    minh = maxh;
  
  /* clamp width and height to min and max values
   */
  width = CLAMP (width, minw, maxw);

  height = CLAMP (height, minh, maxh);
  
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

