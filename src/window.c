/* Metacity X managed windows */

/* 
 * Copyright (C) 2001 Havoc Pennington, Anders Carlsson
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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
#include "window.h"
#include "edge-resistance.h"
#include "util.h"
#include "frame.h"
#include "errors.h"
#include "workspace.h"
#include "stack.h"
#include "keybindings.h"
#include "ui.h"
#include "place.h"
#include "session.h"
#include "effects.h"
#include "prefs.h"
#include "resizepopup.h"
#include "xprops.h"
#include "group.h"
#include "window-props.h"
#include "constraints.h"
#include "compositor.h"
#include "effects.h"

#include <X11/Xatom.h>
#include <string.h>

#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif

static int destroying_windows_disallowed = 0;


static void     update_sm_hints           (MetaWindow     *window);
static void     update_role               (MetaWindow     *window);
static void     update_net_wm_type        (MetaWindow     *window);
static void     update_net_frame_extents  (MetaWindow     *window);
static void     recalc_window_type        (MetaWindow     *window);
static void     recalc_window_features    (MetaWindow     *window);
static void     invalidate_work_areas     (MetaWindow     *window);
static void     recalc_window_type        (MetaWindow     *window);
static void     set_wm_state              (MetaWindow     *window,
                                           int             state);
static void     set_net_wm_state          (MetaWindow     *window);

static void     send_configure_notify     (MetaWindow     *window);
static gboolean process_property_notify   (MetaWindow     *window,
                                           XPropertyEvent *event);
static void     meta_window_show          (MetaWindow     *window);
static void     meta_window_hide          (MetaWindow     *window);

static void     meta_window_save_rect         (MetaWindow    *window);

static void meta_window_move_resize_internal (MetaWindow         *window,
                                              MetaMoveResizeFlags flags,
                                              int                 resize_gravity,
                                              int                 root_x_nw,
                                              int                 root_y_nw,
                                              int                 w,
                                              int                 h);

static void     ensure_mru_position_after (MetaWindow *window,
                                           MetaWindow *after_this_one);


static void meta_window_move_resize_now (MetaWindow  *window);

static void     update_move           (MetaWindow   *window,
                                       gboolean      snap,
                                       int           x,
                                       int           y);
static gboolean update_move_timeout   (gpointer data);
static void     update_resize         (MetaWindow   *window,
                                       gboolean      snap,
                                       int           x,
                                       int           y,
                                       gboolean      force);
static gboolean update_resize_timeout (gpointer data);


/* FIXME we need an abstraction that covers all these queues. */   

static void meta_window_unqueue_calc_showing (MetaWindow *window);
static void meta_window_flush_calc_showing   (MetaWindow *window);

static void meta_window_unqueue_move_resize  (MetaWindow *window);

static void meta_window_update_icon_now (MetaWindow *window);
static void meta_window_unqueue_update_icon    (MetaWindow *window);

static gboolean queue_calc_showing_func (MetaWindow *window,
                                         void       *data);

static void meta_window_apply_session_info (MetaWindow                  *window,
                                            const MetaWindowSessionInfo *info);

static void unmaximize_window_before_freeing (MetaWindow        *window);
static void unminimize_window_and_all_transient_parents (MetaWindow *window);


#ifdef WITH_VERBOSE_MODE
static const char*
wm_state_to_string (int state)
{
  switch (state)
    {
    case NormalState:
      return "NormalState";      
    case IconicState:
      return "IconicState";
    case WithdrawnState:
      return "WithdrawnState";
    }

  return "Unknown";
}
#endif

static gboolean
is_desktop_or_dock_foreach (MetaWindow *window,
                            void       *data)
{
  gboolean *result = data;

  *result =
    window->type == META_WINDOW_DESKTOP ||
    window->type == META_WINDOW_DOCK;
  if (*result)
    return FALSE; /* stop as soon as we find one */
  else
    return TRUE;
}

/* window is the window that's newly mapped provoking
 * the possible change
 */
static void
maybe_leave_show_desktop_mode (MetaWindow *window)
{
  gboolean is_desktop_or_dock;

  if (!window->screen->active_workspace->showing_desktop)
    return;

  /* If the window is a transient for the dock or desktop, don't
   * leave show desktop mode when the window opens. That's
   * so you can e.g. hide all windows, manipulate a file on
   * the desktop via a dialog, then unshow windows again.
   */
  is_desktop_or_dock = FALSE;
  is_desktop_or_dock_foreach (window,
                              &is_desktop_or_dock);

  meta_window_foreach_ancestor (window, is_desktop_or_dock_foreach,
                                &is_desktop_or_dock);

  if (!is_desktop_or_dock)
    {
      meta_screen_minimize_all_on_active_workspace_except (window->screen,
                                                           window);
      meta_screen_unshow_desktop (window->screen);      
    }
}

MetaWindow*
meta_window_new (MetaDisplay *display,
                 Window       xwindow,
                 gboolean     must_be_viewable)
{
  XWindowAttributes attrs;
  MetaWindow *window;
  
  meta_display_grab (display);
  meta_error_trap_push (display); /* Push a trap over all of window
                                   * creation, to reduce XSync() calls
                                   */
  
  meta_error_trap_push_with_return (display);
  
  XGetWindowAttributes (display->xdisplay,
                        xwindow, &attrs);
  
  if (meta_error_trap_pop_with_return (display, TRUE) != Success)
    {
      meta_verbose ("Failed to get attributes for window 0x%lx\n",
                    xwindow);
      meta_error_trap_pop (display, TRUE);
      meta_display_ungrab (display);
      return NULL;
    }
  window = meta_window_new_with_attrs (display, xwindow,
                                       must_be_viewable, &attrs);


  meta_error_trap_pop (display, FALSE);
  meta_display_ungrab (display);

  return window;
}

MetaWindow*
meta_window_new_with_attrs (MetaDisplay       *display,
                            Window             xwindow,
                            gboolean           must_be_viewable,
                            XWindowAttributes *attrs)
{
  MetaWindow *window;
  GSList *tmp;
  MetaWorkspace *space;
  gulong existing_wm_state;
  gulong event_mask;
  MetaMoveResizeFlags flags;
#define N_INITIAL_PROPS 17
  Atom initial_props[N_INITIAL_PROPS];
  int i;
  gboolean has_shape;

  g_assert (attrs != NULL);
  g_assert (N_INITIAL_PROPS == (int) G_N_ELEMENTS (initial_props));
  
  meta_verbose ("Attempting to manage 0x%lx\n", xwindow);

  if (meta_display_xwindow_is_a_no_focus_window (display, xwindow))
    {
      meta_verbose ("Not managing no_focus_window 0x%lx\n",
                    xwindow);
      return NULL;
    }

  if (attrs->override_redirect)
    {
      meta_verbose ("Deciding not to manage override_redirect window 0x%lx\n", xwindow);
      return NULL;
    }
  
  /* Grab server */
  meta_display_grab (display);
  meta_error_trap_push (display); /* Push a trap over all of window
                                   * creation, to reduce XSync() calls
                                   */

  meta_verbose ("must_be_viewable = %d attrs->map_state = %d (%s)\n",
                must_be_viewable,
                attrs->map_state,
                (attrs->map_state == IsUnmapped) ?
                "IsUnmapped" :
                (attrs->map_state == IsViewable) ?
                "IsViewable" :
                (attrs->map_state == IsUnviewable) ?
                "IsUnviewable" :
                "(unknown)");
  
  existing_wm_state = WithdrawnState;
  if (must_be_viewable && attrs->map_state != IsViewable)
    {
      /* Only manage if WM_STATE is IconicState or NormalState */
      gulong state;

      /* WM_STATE isn't a cardinal, it's type WM_STATE, but is an int */
      if (!(meta_prop_get_cardinal_with_atom_type (display, xwindow,
                                                   display->atom_wm_state,
                                                   display->atom_wm_state,
                                                   &state) &&
            (state == IconicState || state == NormalState)))
        {
          meta_verbose ("Deciding not to manage unmapped or unviewable window 0x%lx\n", xwindow);
          meta_error_trap_pop (display, TRUE);
          meta_display_ungrab (display);
          return NULL;
        }

      existing_wm_state = state;
      meta_verbose ("WM_STATE of %lx = %s\n", xwindow,
                    wm_state_to_string (existing_wm_state));
    }
  
  meta_error_trap_push_with_return (display);
  
  XAddToSaveSet (display->xdisplay, xwindow);

  event_mask =
    PropertyChangeMask | EnterWindowMask | LeaveWindowMask |
    FocusChangeMask | ColormapChangeMask;

  XSelectInput (display->xdisplay, xwindow, event_mask);

  has_shape = FALSE;
#ifdef HAVE_SHAPE
  if (META_DISPLAY_HAS_SHAPE (display))
    {
      int x_bounding, y_bounding, x_clip, y_clip;
      unsigned w_bounding, h_bounding, w_clip, h_clip;
      int bounding_shaped, clip_shaped;

      XShapeSelectInput (display->xdisplay, xwindow, ShapeNotifyMask);

      XShapeQueryExtents (display->xdisplay, xwindow,
                          &bounding_shaped, &x_bounding, &y_bounding,
                          &w_bounding, &h_bounding,
                          &clip_shaped, &x_clip, &y_clip,
                          &w_clip, &h_clip);

      has_shape = bounding_shaped != FALSE;

      meta_topic (META_DEBUG_SHAPES,
                  "Window has_shape = %d extents %d,%d %u x %u\n",
                  has_shape, x_bounding, y_bounding,
                  w_bounding, h_bounding);
    }
#endif

  /* Get rid of any borders */
  if (attrs->border_width != 0)
    XSetWindowBorderWidth (display->xdisplay, xwindow, 0);

  /* Get rid of weird gravities */
  if (attrs->win_gravity != NorthWestGravity)
    {
      XSetWindowAttributes set_attrs;
      
      set_attrs.win_gravity = NorthWestGravity;
      
      XChangeWindowAttributes (display->xdisplay,
                               xwindow,
                               CWWinGravity,
                               &set_attrs);
    }
  
  if (meta_error_trap_pop_with_return (display, FALSE) != Success)
    {
      meta_verbose ("Window 0x%lx disappeared just as we tried to manage it\n",
                    xwindow);
      meta_error_trap_pop (display, FALSE);
      meta_display_ungrab (display);
      return NULL;
    }

  g_assert (!attrs->override_redirect);
  
  window = g_new (MetaWindow, 1);

  window->constructing = TRUE;
  
  window->dialog_pid = -1;
  window->dialog_pipe = -1;
  
  window->xwindow = xwindow;
  
  /* this is in window->screen->display, but that's too annoying to
   * type
   */
  window->display = display;
  window->workspace = NULL;

#ifdef HAVE_XSYNC
  window->sync_request_counter = None;
  window->sync_request_serial = 0;
  window->sync_request_time.tv_sec = 0;
  window->sync_request_time.tv_usec = 0;
#endif
  
  window->screen = NULL;
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *scr = tmp->data;

      if (scr->xroot == attrs->root)
        {
          window->screen = tmp->data;
          break;
        }
      
      tmp = tmp->next;
    }
  
  g_assert (window->screen);

  window->desc = g_strdup_printf ("0x%lx", window->xwindow);

  /* avoid tons of stack updates */
  meta_stack_freeze (window->screen->stack);

  window->has_shape = has_shape;
  
  window->rect.x = attrs->x;
  window->rect.y = attrs->y;
  window->rect.width = attrs->width;
  window->rect.height = attrs->height;

  /* And border width, size_hints are the "request" */
  window->border_width = attrs->border_width;
  window->size_hints.x = attrs->x;
  window->size_hints.y = attrs->y;
  window->size_hints.width = attrs->width;
  window->size_hints.height = attrs->height;
  /* initialize the remaining size_hints as if size_hints.flags were zero */
  meta_set_normal_hints (window, NULL);

  /* And this is our unmaximized size */
  window->saved_rect = window->rect;
  window->user_rect = window->rect;
  
  window->depth = attrs->depth;
  window->xvisual = attrs->visual;
  window->colormap = attrs->colormap;
  
  window->title = NULL;
  window->icon_name = NULL;
  window->icon = NULL;
  window->mini_icon = NULL;
  meta_icon_cache_init (&window->icon_cache);
  window->wm_hints_pixmap = None;
  window->wm_hints_mask = None;

  window->frame = NULL;
  window->has_focus = FALSE;

  window->user_has_move_resized = FALSE;
  
  window->maximized_horizontally = FALSE;
  window->maximized_vertically = FALSE;
  window->maximize_horizontally_after_placement = FALSE;
  window->maximize_vertically_after_placement = FALSE;
  window->fullscreen = FALSE;
  window->require_fully_onscreen = TRUE;
  window->require_on_single_xinerama = TRUE;
  window->require_titlebar_visible = TRUE;
  window->on_all_workspaces = FALSE;
  window->shaded = FALSE;
  window->initially_iconic = FALSE;
  window->minimized = FALSE;
  window->was_minimized = FALSE;
  window->tab_unminimized = FALSE;
  window->iconic = FALSE;
  window->mapped = attrs->map_state != IsUnmapped;
  /* if already mapped, no need to worry about focus-on-first-time-showing */
  window->showing_for_first_time = !window->mapped;
  /* if already mapped we don't want to do the placement thing */
  window->placed = window->mapped;
  if (window->placed)
    meta_topic (META_DEBUG_PLACEMENT,
                "Not placing window 0x%lx since it's already mapped\n",
                xwindow);
  window->denied_focus_and_not_transient = FALSE;
  window->unmanaging = FALSE;
  window->calc_showing_queued = FALSE;
  window->move_resize_queued = FALSE;
  window->keys_grabbed = FALSE;
  window->grab_on_frame = FALSE;
  window->all_keys_grabbed = FALSE;
  window->withdrawn = FALSE;
  window->initial_workspace_set = FALSE;
  window->initial_timestamp_set = FALSE;
  window->net_wm_user_time_set = FALSE;
  window->calc_placement = FALSE;
  window->shaken_loose = FALSE;
  window->have_focus_click_grab = FALSE;
  window->disable_sync = FALSE;
  
  window->unmaps_pending = 0;
  
  window->mwm_decorated = TRUE;
  window->mwm_border_only = FALSE;
  window->mwm_has_close_func = TRUE;
  window->mwm_has_minimize_func = TRUE;
  window->mwm_has_maximize_func = TRUE;
  window->mwm_has_move_func = TRUE;
  window->mwm_has_resize_func = TRUE;
  
  window->decorated = TRUE;
  window->has_close_func = TRUE;
  window->has_minimize_func = TRUE;
  window->has_maximize_func = TRUE;
  window->has_move_func = TRUE;
  window->has_resize_func = TRUE;

  window->has_shade_func = TRUE;

  window->has_fullscreen_func = TRUE;

  window->always_sticky = FALSE;
  
  window->wm_state_modal = FALSE;
  window->skip_taskbar = FALSE;
  window->skip_pager = FALSE;
  window->wm_state_skip_taskbar = FALSE;
  window->wm_state_skip_pager = FALSE;
  window->wm_state_above = FALSE;
  window->wm_state_below = FALSE;
  window->wm_state_demands_attention = FALSE;
  
  window->res_class = NULL;
  window->res_name = NULL;
  window->role = NULL;
  window->sm_client_id = NULL;
  window->wm_client_machine = NULL;
  window->startup_id = NULL;
  
  window->net_wm_pid = -1;
  
  window->xtransient_for = None;
  window->xclient_leader = None;
  window->transient_parent_is_root_window = FALSE;
  
  window->type = META_WINDOW_NORMAL;
  window->type_atom = None;

  window->struts = NULL;

  window->using_net_wm_name              = FALSE;
  window->using_net_wm_visible_name      = FALSE;
  window->using_net_wm_icon_name         = FALSE;
  window->using_net_wm_visible_icon_name = FALSE;

  window->need_reread_icon = TRUE;
  window->update_icon_queued = FALSE;
  
  window->layer = META_LAYER_LAST; /* invalid value */
  window->stack_position = -1;
  window->initial_workspace = 0; /* not used */
  window->initial_timestamp = 0; /* not used */
  
  meta_display_register_x_window (display, &window->xwindow, window);


  /* assign the window to its group, or create a new group if needed
   */
  window->group = NULL;
  window->xgroup_leader = None;
  meta_window_compute_group (window);
  
  /* Fill these in the order we want them to be gotten.
   * we want to get window name and class first
   * so we can use them in error messages and such.
   */
  i = 0;
  initial_props[i++] = display->atom_net_wm_name;
  initial_props[i++] = XA_WM_CLASS;
  initial_props[i++] = display->atom_wm_client_machine;
  initial_props[i++] = display->atom_net_wm_pid;
  initial_props[i++] = XA_WM_NAME;
  initial_props[i++] = display->atom_net_wm_icon_name;
  initial_props[i++] = XA_WM_ICON_NAME;
  initial_props[i++] = display->atom_net_wm_desktop;
  initial_props[i++] = display->atom_net_startup_id;
  initial_props[i++] = display->atom_net_wm_sync_request_counter;
  initial_props[i++] = XA_WM_NORMAL_HINTS;
  initial_props[i++] = display->atom_wm_protocols;
  initial_props[i++] = XA_WM_HINTS;
  initial_props[i++] = display->atom_net_wm_user_time;
  initial_props[i++] = display->atom_net_wm_state;
  initial_props[i++] = display->atom_motif_wm_hints;
  initial_props[i++] = XA_WM_TRANSIENT_FOR;
  g_assert (N_INITIAL_PROPS == i);
  
  meta_window_reload_properties (window, initial_props, N_INITIAL_PROPS);
  
  update_sm_hints (window); /* must come after transient_for */
  update_role (window);
  update_net_wm_type (window);
  meta_window_update_icon_now (window);

  if (window->initially_iconic)
    {
      /* WM_HINTS said minimized */
      window->minimized = TRUE;
      meta_verbose ("Window %s asked to start out minimized\n", window->desc);
    }

  if (existing_wm_state == IconicState)
    {
      /* WM_STATE said minimized */
      window->minimized = TRUE;
      meta_verbose ("Window %s had preexisting WM_STATE = IconicState, minimizing\n",
                    window->desc);

      /* Assume window was previously placed, though perhaps it's
       * been iconic its whole life, we have no way of knowing.
       */
      window->placed = TRUE;
    }

  /* Apply any window attributes such as initial workspace
   * based on startup notification
   */
  meta_screen_apply_startup_properties (window->screen, window);
  
  if (window->decorated)
    meta_window_ensure_frame (window);

  meta_window_grab_keys (window);
  meta_display_grab_window_buttons (window->display, window->xwindow);
  meta_display_grab_focus_window_button (window->display, window);

  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK)
    {
      /* Change the default, but don't enforce this if the user
       * focuses the dock/desktop and unsticks it using key shortcuts.
       * Need to set this before adding to the workspaces so the MRU
       * lists will be updated.
       */
      window->on_all_workspaces = TRUE;
    }
  
  /* For the workspace, first honor hints,
   * if that fails put transients with parents,
   * otherwise put window on active space
   */
  
  if (window->initial_workspace_set)
    {
      if (window->initial_workspace == (int) 0xFFFFFFFF)
        {
          meta_topic (META_DEBUG_PLACEMENT,
                      "Window %s is initially on all spaces\n",
                      window->desc);
          
	  /* need to set on_all_workspaces first so that it will be
	   * added to all the MRU lists
	   */
          window->on_all_workspaces = TRUE;
          meta_workspace_add_window (window->screen->active_workspace, window);
        }
      else
        {
          meta_topic (META_DEBUG_PLACEMENT,
                      "Window %s is initially on space %d\n",
                      window->desc, window->initial_workspace);

          space =
            meta_screen_get_workspace_by_index (window->screen,
                                                window->initial_workspace);
          
          if (space)
            meta_workspace_add_window (space, window);
        }
    }
  
  if (window->workspace == NULL && 
      window->xtransient_for != None)
    {
      /* Try putting dialog on parent's workspace */
      MetaWindow *parent;

      parent = meta_display_lookup_x_window (window->display,
                                             window->xtransient_for);

      if (parent && parent->workspace)
        {
          meta_topic (META_DEBUG_PLACEMENT,
                      "Putting window %s on same workspace as parent %s\n",
                      window->desc, parent->desc);
          
          if (parent->on_all_workspaces)
            window->on_all_workspaces = TRUE;
          
	  /* this will implicitly add to the appropriate MRU lists
	   */
          meta_workspace_add_window (parent->workspace, window);
        }
    }
  
  if (window->workspace == NULL)
    {
      meta_topic (META_DEBUG_PLACEMENT,
                  "Putting window %s on active workspace\n",
                  window->desc);
      
      space = window->screen->active_workspace;

      meta_workspace_add_window (space, window);
    }
  
  /* for the various on_all_workspaces = TRUE possible above */
  meta_window_set_current_workspace_hint (window);
  
  meta_window_update_struts (window);

  /* Must add window to stack before doing move/resize, since the
   * window might have fullscreen size (i.e. should have been
   * fullscreen'd; acrobat is one such braindead case; it withdraws
   * and remaps its window whenever trying to become fullscreen...)
   * and thus constraints may try to auto-fullscreen it which also
   * means restacking it.
   */
  meta_stack_add (window->screen->stack, 
                  window);

  /* Put our state back where it should be,
   * passing TRUE for is_configure_request, ICCCM says
   * initial map is handled same as configure request
   */
  flags =
    META_IS_CONFIGURE_REQUEST | META_IS_MOVE_ACTION | META_IS_RESIZE_ACTION;
  meta_window_move_resize_internal (window,
                                    flags,
                                    window->size_hints.win_gravity,
                                    window->size_hints.x,
                                    window->size_hints.y,
                                    window->size_hints.width,
                                    window->size_hints.height);

  /* Now try applying saved stuff from the session */
  {
    const MetaWindowSessionInfo *info;

    info = meta_window_lookup_saved_state (window);

    if (info)
      {
        meta_window_apply_session_info (window, info);
        meta_window_release_saved_state (info);
      }
  }
  
  /* FIXME we have a tendency to set this then immediately
   * change it again.
   */
  set_wm_state (window, window->iconic ? IconicState : NormalState);
  set_net_wm_state (window);

  /* Sync stack changes */
  meta_stack_thaw (window->screen->stack);

  /* disable show desktop mode unless we're a desktop component */
  maybe_leave_show_desktop_mode (window);
  
  meta_window_queue_calc_showing (window);
  /* See bug 303284; a transient of the given window can already exist, in which
   * case we think it should probably be shown.
   */
  meta_window_foreach_transient (window,
                                 queue_calc_showing_func,
                                 NULL);
  /* See bug 334899; the window may have minimized ancestors which need to be
   * shown.
   */
  unminimize_window_and_all_transient_parents (window);

  meta_error_trap_pop (display, FALSE); /* pop the XSync()-reducing trap */
  meta_display_ungrab (display);
 
  window->constructing = FALSE;

  return window;
}

/* This function should only be called from the end of meta_window_new_with_attrs () */
static void
meta_window_apply_session_info (MetaWindow *window,
                                const MetaWindowSessionInfo *info)
{
  if (info->stack_position_set)
    {
      meta_topic (META_DEBUG_SM,
                  "Restoring stack position %d for window %s\n",
                  info->stack_position, window->desc);

      /* FIXME well, I'm not sure how to do this. */
    }

  if (info->minimized_set)
    {
      meta_topic (META_DEBUG_SM,
                  "Restoring minimized state %d for window %s\n",
                  info->minimized, window->desc);

      if (window->has_minimize_func && info->minimized)
        meta_window_minimize (window);
    }

  if (info->maximized_set)
    {
      meta_topic (META_DEBUG_SM,
                  "Restoring maximized state %d for window %s\n",
                  info->maximized, window->desc);
      
      if (window->has_maximize_func && info->maximized)
        {
          meta_window_maximize (window, 
                                META_MAXIMIZE_HORIZONTAL |
                                META_MAXIMIZE_VERTICAL);

          if (info->saved_rect_set)
            {
              meta_topic (META_DEBUG_SM,
                          "Restoring saved rect %d,%d %dx%d for window %s\n",
                          info->saved_rect.x,
                          info->saved_rect.y,
                          info->saved_rect.width,
                          info->saved_rect.height,
                          window->desc);
              
              window->saved_rect.x = info->saved_rect.x;
              window->saved_rect.y = info->saved_rect.y;
              window->saved_rect.width = info->saved_rect.width;
              window->saved_rect.height = info->saved_rect.height;
            }
	}
    }
  
  if (info->on_all_workspaces_set)
    {
      window->on_all_workspaces = info->on_all_workspaces;
      meta_topic (META_DEBUG_SM,
                  "Restoring sticky state %d for window %s\n",
                  window->on_all_workspaces, window->desc);
    }
  
  if (info->workspace_indices)
    {
      GSList *tmp;
      GSList *spaces;      

      spaces = NULL;
      
      tmp = info->workspace_indices;
      while (tmp != NULL)
        {
          MetaWorkspace *space;          

          space =
            meta_screen_get_workspace_by_index (window->screen,
                                                GPOINTER_TO_INT (tmp->data));
          
          if (space)
            spaces = g_slist_prepend (spaces, space);
          
          tmp = tmp->next;
        }

      if (spaces)
        {
          /* This briefly breaks the invariant that we are supposed
           * to always be on some workspace. But we paranoically
           * ensured that one of the workspaces from the session was
           * indeed valid, so we know we'll go right back to one.
           */
          if (window->workspace)
            meta_workspace_remove_window (window->workspace, window);

          /* Only restore to the first workspace if the window
           * happened to be on more than one, since we have replaces
           * window->workspaces with window->workspace
           */
          meta_workspace_add_window (spaces->data, window);              

          meta_topic (META_DEBUG_SM,
                      "Restoring saved window %s to workspace %d\n",
                      window->desc,
                      meta_workspace_index (spaces->data));
              
          g_slist_free (spaces);
        }
    }

  if (info->geometry_set)
    {
      int x, y, w, h;
      MetaMoveResizeFlags flags;
      
      window->placed = TRUE; /* don't do placement algorithms later */

      x = info->rect.x;
      y = info->rect.y;

      w = window->size_hints.base_width +
        info->rect.width * window->size_hints.width_inc;
      h = window->size_hints.base_height +
        info->rect.height * window->size_hints.height_inc;

      /* Force old gravity, ignoring anything now set */
      window->size_hints.win_gravity = info->gravity;
      
      meta_topic (META_DEBUG_SM,
                  "Restoring pos %d,%d size %d x %d for %s\n",
                  x, y, w, h, window->desc);
      
      flags = META_DO_GRAVITY_ADJUST | META_IS_MOVE_ACTION | META_IS_RESIZE_ACTION;
      meta_window_move_resize_internal (window,
                                        flags,
                                        window->size_hints.win_gravity,
                                        x, y, w, h);
    }
}

void
meta_window_free (MetaWindow  *window,
                  guint32      timestamp)
{
  GList *tmp;
  
  meta_verbose ("Unmanaging 0x%lx\n", window->xwindow);

  if (window->display->compositor)
    meta_compositor_free_window (window->display->compositor, window);
  
  if (window->display->window_with_menu == window)
    {
      meta_ui_window_menu_free (window->display->window_menu);
      window->display->window_menu = NULL;
      window->display->window_with_menu = NULL;
    }
  
  if (destroying_windows_disallowed > 0)
    meta_bug ("Tried to destroy window %s while destruction was not allowed\n",
              window->desc);
  
  window->unmanaging = TRUE;

  if (window->fullscreen)
    {
      MetaGroup *group;

      /* If the window is fullscreen, it may be forcing
       * other windows in its group to a higher layer
       */

      meta_stack_freeze (window->screen->stack);
      group = meta_window_get_group (window);
      if (group)
        meta_group_update_layers (group);
      meta_stack_thaw (window->screen->stack);
    }

  meta_window_shutdown_group (window); /* safe to do this early as
                                        * group.c won't re-add to the
                                        * group if window->unmanaging
                                        */
  
  /* If we have the focus, focus some other window.
   * This is done first, so that if the unmap causes
   * an EnterNotify the EnterNotify will have final say
   * on what gets focused, maintaining sloppy focus
   * invariants.
   */
  if (window->has_focus)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing default window since we're unmanaging %s\n",
                  window->desc);
      meta_workspace_focus_default_window (window->screen->active_workspace,
                                           window,
                                           timestamp);
    }
  else if (window->display->expected_focus_window == window)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing default window since expected focus window freed %s\n",
                  window->desc);
      window->display->expected_focus_window = NULL;
      meta_workspace_focus_default_window (window->screen->active_workspace,
                                           window,
                                           timestamp);
    }
  else
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Unmanaging window %s which doesn't currently have focus\n",
                  window->desc);
    }

  if (window->struts)
    {
      g_free (window->struts);
      window->struts = NULL;

      meta_topic (META_DEBUG_WORKAREA,
                  "Unmanaging window %s which has struts, so invalidating work areas\n",
                  window->desc);
      invalidate_work_areas (window);
    }
  
  if (window->display->grab_window == window)
    meta_display_end_grab_op (window->display, timestamp);

  g_assert (window->display->grab_window != window);
  
  if (window->display->focus_window == window)
    window->display->focus_window = NULL;

  if (window->maximized_horizontally || window->maximized_vertically)
    unmaximize_window_before_freeing (window);
  
  meta_window_unqueue_calc_showing (window);
  meta_window_unqueue_move_resize (window);
  meta_window_unqueue_update_icon (window);
  meta_window_free_delete_dialog (window);
  
  if (window->workspace)
    meta_workspace_remove_window (window->workspace, window);

  g_assert (window->workspace == NULL);

#ifndef G_DISABLE_CHECKS
  tmp = window->screen->workspaces;
  while (tmp != NULL)
    {
      MetaWorkspace *workspace = tmp->data;

      g_assert (g_list_find (workspace->windows, window) == NULL);
      g_assert (g_list_find (workspace->mru_list, window) == NULL);

      tmp = tmp->next;
    }
#endif
  
  meta_stack_remove (window->screen->stack, window);
  
  if (window->frame)
    meta_window_destroy_frame (window);
  
  if (window->withdrawn)
    {
      /* We need to clean off the window's state so it
       * won't be restored if the app maps it again.
       */
      meta_error_trap_push (window->display);
      meta_verbose ("Cleaning state from window %s\n", window->desc);
      XDeleteProperty (window->display->xdisplay,
                       window->xwindow,
                       window->display->atom_net_wm_desktop);
      XDeleteProperty (window->display->xdisplay,
                       window->xwindow,
                       window->display->atom_net_wm_state);
      set_wm_state (window, WithdrawnState);
      meta_error_trap_pop (window->display, FALSE);
    }
  else
    {
      /* We need to put WM_STATE so that others will understand it on
       * restart.
       */
      if (!window->minimized)
        {
          meta_error_trap_push (window->display);
          set_wm_state (window, NormalState);
          meta_error_trap_pop (window->display, FALSE);
        }

      /* And we need to be sure the window is mapped so other WMs
       * know that it isn't Withdrawn
       */
      meta_error_trap_push (window->display);
      XMapWindow (window->display->xdisplay,
                  window->xwindow);
      meta_error_trap_pop (window->display, FALSE);
    }

  meta_window_ungrab_keys (window);
  meta_display_ungrab_window_buttons (window->display, window->xwindow);
  meta_display_ungrab_focus_window_button (window->display, window);
  
  meta_display_unregister_x_window (window->display, window->xwindow);
  

  meta_error_trap_push (window->display);

  /* Put back anything we messed up */
  if (window->border_width != 0)
    XSetWindowBorderWidth (window->display->xdisplay,
                           window->xwindow,
                           window->border_width);

  /* No save set */
  XRemoveFromSaveSet (window->display->xdisplay,
                      window->xwindow);

  /* Don't get events on not-managed windows */
  XSelectInput (window->display->xdisplay,
                window->xwindow,
                NoEventMask);

#ifdef HAVE_SHAPE
  if (META_DISPLAY_HAS_SHAPE (window->display))
    XShapeSelectInput (window->display->xdisplay, window->xwindow, NoEventMask);
#endif
  
  meta_error_trap_pop (window->display, FALSE);

  if (window->icon)
    g_object_unref (G_OBJECT (window->icon));

  if (window->mini_icon)
    g_object_unref (G_OBJECT (window->mini_icon));

  meta_icon_cache_free (&window->icon_cache);
  
  g_free (window->sm_client_id);
  g_free (window->wm_client_machine);
  g_free (window->startup_id);
  g_free (window->role);
  g_free (window->res_class);
  g_free (window->res_name);
  g_free (window->title);
  g_free (window->icon_name);
  g_free (window->desc);
  g_free (window);
}

static void
set_wm_state (MetaWindow *window,
              int         state)
{
  unsigned long data[2];
  
  meta_verbose ("Setting wm state %s on %s\n",
                wm_state_to_string (state), window->desc);
  
  /* Metacity doesn't use icon windows, so data[1] should be None
   * according to the ICCCM 2.0 Section 4.1.3.1.
   */
  data[0] = state;
  data[1] = None;

  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_wm_state,
                   window->display->atom_wm_state,
                   32, PropModeReplace, (guchar*) data, 2);
  meta_error_trap_pop (window->display, FALSE);
}

static void
set_net_wm_state (MetaWindow *window)
{
  int i;
  unsigned long data[11];
  
  i = 0;
  if (window->shaded)
    {
      data[i] = window->display->atom_net_wm_state_shaded;
      ++i;
    }
  if (window->wm_state_modal)
    {
      data[i] = window->display->atom_net_wm_state_modal;
      ++i;
    }
  if (window->skip_pager)
    {
      data[i] = window->display->atom_net_wm_state_skip_pager;
      ++i;
    }
  if (window->skip_taskbar)
    {
      data[i] = window->display->atom_net_wm_state_skip_taskbar;
      ++i;
    }
  if (window->maximized_horizontally)
    {
      data[i] = window->display->atom_net_wm_state_maximized_horz;
      ++i;
    }
  if (window->maximized_vertically)
    {
      data[i] = window->display->atom_net_wm_state_maximized_vert;
      ++i;
    }
  if (window->fullscreen)
    {
      data[i] = window->display->atom_net_wm_state_fullscreen;
      ++i;
    }
  if (!meta_window_showing_on_its_workspace (window) || window->shaded)
    {
      data[i] = window->display->atom_net_wm_state_hidden;
      ++i;
    }
  if (window->wm_state_above)
    {
      data[i] = window->display->atom_net_wm_state_above;
      ++i;
    }
  if (window->wm_state_below)
    {
      data[i] = window->display->atom_net_wm_state_below;
      ++i;
    }
  if (window->wm_state_demands_attention)
    {
      data[i] = window->display->atom_net_wm_state_demands_attention;
      ++i;
    }

  meta_verbose ("Setting _NET_WM_STATE with %d atoms\n", i);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_state,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) data, i);
  meta_error_trap_pop (window->display, FALSE);
}

gboolean
meta_window_located_on_workspace (MetaWindow    *window,
                                  MetaWorkspace *workspace)
{
  return (window->on_all_workspaces && window->screen == workspace->screen) ||
    (window->workspace == workspace);
}

static gboolean
is_minimized_foreach (MetaWindow *window,
                      void       *data)
{
  gboolean *result = data;

  *result = window->minimized;
  if (*result)
    return FALSE; /* stop as soon as we find one */
  else
    return TRUE;
}

static gboolean
ancestor_is_minimized (MetaWindow *window)
{
  gboolean is_minimized;

  is_minimized = FALSE;

  meta_window_foreach_ancestor (window, is_minimized_foreach, &is_minimized);

  return is_minimized;
}

gboolean
meta_window_showing_on_its_workspace (MetaWindow *window)
{
  gboolean showing;
  gboolean is_desktop_or_dock;
  MetaWorkspace* workspace_of_window;

  showing = TRUE;

  /* 1. See if we're minimized */
  if (window->minimized)
    showing = FALSE;
  
  /* 2. See if we're in "show desktop" mode */
  is_desktop_or_dock = FALSE;
  is_desktop_or_dock_foreach (window,
                              &is_desktop_or_dock);

  meta_window_foreach_ancestor (window, is_desktop_or_dock_foreach,
                                &is_desktop_or_dock);

  if (window->on_all_workspaces)
    workspace_of_window = window->screen->active_workspace;
  else if (window->workspace)
    workspace_of_window = window->workspace;
  else /* This only seems to be needed for startup */
    workspace_of_window = NULL;

  if (showing &&      
      workspace_of_window && workspace_of_window->showing_desktop &&
      !is_desktop_or_dock)
    {
      meta_verbose ("We're showing the desktop on the workspace(s) that window %s is on\n",
                    window->desc);
      showing = FALSE;
    }

  /* 3. See if an ancestor is minimized (note that
   *    ancestor's "mapped" field may not be up to date
   *    since it's being computed in this same idle queue)
   */
  
  if (showing)
    {
      if (ancestor_is_minimized (window))
        showing = FALSE;
    }

#if 0
  /* 4. See if we're drawing wireframe
   */
  if (window->display->grab_window == window &&
      window->display->grab_wireframe_active)    
    showing = FALSE;
#endif

  return showing;
}

gboolean
meta_window_should_be_showing (MetaWindow  *window)
{
  gboolean on_workspace;

  meta_verbose ("Should be showing for window %s\n", window->desc);

  /* See if we're on the workspace */
  on_workspace = meta_window_located_on_workspace (window, 
                                                   window->screen->active_workspace);

  if (!on_workspace)
    meta_verbose ("Window %s is not on workspace %d\n",
                  window->desc,
                  meta_workspace_index (window->screen->active_workspace));
  else
    meta_verbose ("Window %s is on the active workspace %d\n",
                  window->desc,
                  meta_workspace_index (window->screen->active_workspace));

  if (window->on_all_workspaces)
    meta_verbose ("Window %s is on all workspaces\n", window->desc);

  return on_workspace && meta_window_showing_on_its_workspace (window);
}

static void
finish_minimize (const MetaEffect *effect,
		 gpointer	   data)
{
  MetaWindow *window = data;
  /* FIXME: It really sucks to put timestamp pinging here; it'd
   * probably make more sense in implement_showing() so that it's at
   * least not duplicated in meta_window_show; but since
   * finish_minimize is a callback making things just slightly icky, I
   * haven't done that yet.
   */
  guint32 timestamp = meta_display_get_current_time_roundtrip (window->display);
  
  meta_window_hide (window);
  if (window->has_focus)
    {
      meta_workspace_focus_default_window (window->screen->active_workspace,
                                           window, 
                                           timestamp);
    }
}

static void
implement_showing (MetaWindow *window,
                   gboolean    showing)
{
  /* Actually show/hide the window */
  meta_verbose ("Implement showing = %d for window %s\n",
                showing, window->desc);
   
  if (!showing)
    {
      gboolean on_workspace;
      
      on_workspace = meta_window_located_on_workspace (window, 
                                                       window->screen->active_workspace);
      
      /* Really this effects code should probably
       * be in meta_window_hide so the window->mapped
       * test isn't duplicated here. Anyhow, we animate
       * if we are mapped now, we are supposed to
       * be minimized, and we are on the current workspace.
       */
      if (on_workspace && window->minimized && window->mapped &&
          !meta_prefs_get_reduced_resources ())
        {
          MetaRectangle icon_rect, window_rect;
          gboolean result;
          
          /* Check if the window has an icon geometry */
          result = meta_window_get_icon_geometry (window, &icon_rect);
          
          if (!result)
            {
              /* just animate into the corner somehow - maybe
               * not a good idea...
               */              
              icon_rect.x = window->screen->rect.width;
              icon_rect.y = window->screen->rect.height;
              icon_rect.width = 1;
              icon_rect.height = 1;
            }

          meta_window_get_outer_rect (window, &window_rect);

          meta_effect_run_minimize (window,
                                    &window_rect,
                                    &icon_rect,
                                    finish_minimize,
                                    window);
        }
      else
        {
          finish_minimize (NULL, window);
        }
    }
  else
    {
      meta_window_show (window);
    }
}

void
meta_window_calc_showing (MetaWindow  *window)
{
  implement_showing (window, meta_window_should_be_showing (window));
}

static guint calc_showing_idle = 0;
static GSList *calc_showing_pending = NULL;

static int
stackcmp (gconstpointer a, gconstpointer b)
{
  MetaWindow *aw = (gpointer) a;
  MetaWindow *bw = (gpointer) b;

  if (aw->screen != bw->screen)
    return 0; /* don't care how they sort with respect to each other */
  else
    return meta_stack_windows_cmp (aw->screen->stack,
                                   aw, bw);
}

static gboolean
idle_calc_showing (gpointer data)
{
  GSList *tmp;
  GSList *copy;
  GSList *should_show;
  GSList *should_hide;
  GSList *unplaced;
  GSList *displays;

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Clearing the calc_showing queue\n");

  /* Work with a copy, for reentrancy. The allowed reentrancy isn't
   * complete; destroying a window while we're in here would result in
   * badness. But it's OK to queue/unqueue calc_showings.
   */
  copy = g_slist_copy (calc_showing_pending);
  g_slist_free (calc_showing_pending);
  calc_showing_pending = NULL;
  calc_showing_idle = 0;

  destroying_windows_disallowed += 1;
  
  /* We map windows from top to bottom and unmap from bottom to
   * top, to avoid extra expose events. The exception is
   * for unplaced windows, which have to be mapped from bottom to
   * top so placement works.
   */
  should_show = NULL;
  should_hide = NULL;
  unplaced = NULL;
  displays = NULL;

  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      if (!window->placed)
        unplaced = g_slist_prepend (unplaced, window);
      else if (meta_window_should_be_showing (window))
        should_show = g_slist_prepend (should_show, window);
      else
        should_hide = g_slist_prepend (should_hide, window);
      
      tmp = tmp->next;
    }

  /* bottom to top */
  unplaced = g_slist_sort (unplaced, stackcmp);
  should_hide = g_slist_sort (should_hide, stackcmp);
  /* top to bottom */
  should_show = g_slist_sort (should_show, stackcmp);
  should_show = g_slist_reverse (should_show);
  
  tmp = unplaced;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      meta_window_calc_showing (window);
      
      tmp = tmp->next;
    }

  tmp = should_hide;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      implement_showing (window, FALSE);
      
      tmp = tmp->next;
    }
  
  tmp = should_show;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      implement_showing (window, TRUE);
      
      tmp = tmp->next;
    }
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      /* important to set this here for reentrancy -
       * if we queue a window again while it's in "copy",
       * then queue_calc_showing will just return since
       * calc_showing_queued = TRUE still
       */
      window->calc_showing_queued = FALSE;
      
      tmp = tmp->next;
    }

  if (meta_prefs_get_focus_mode () != META_FOCUS_MODE_CLICK)
    {
      /* When display->mouse_mode is false, we want to ignore
       * EnterNotify events unless they come from mouse motion.  To do
       * that, we set a sentinel property on the root window if we're
       * not in mouse_mode.
       */
      tmp = should_show;
      while (tmp != NULL)
        {
          MetaWindow *window = tmp->data;
          
          if (!window->display->mouse_mode)
            meta_display_increment_focus_sentinel (window->display);

          tmp = tmp->next;
        }
    }

  g_slist_free (copy);

  g_slist_free (unplaced);
  g_slist_free (should_show);
  g_slist_free (should_hide);
  g_slist_free (displays);
  
  destroying_windows_disallowed -= 1;
  
  return FALSE;
}

static void
meta_window_unqueue_calc_showing (MetaWindow *window)
{
  if (!window->calc_showing_queued)
    return;

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Removing %s from the calc_showing queue\n",
              window->desc);

  /* Note that window may not actually be in move_resize_pending
   * because it may have been in "copy" inside the idle handler
   */  
  calc_showing_pending = g_slist_remove (calc_showing_pending, window);
  window->calc_showing_queued = FALSE;
  
  if (calc_showing_pending == NULL &&
      calc_showing_idle != 0)
    {
      g_source_remove (calc_showing_idle);
      calc_showing_idle = 0;
    }
}

static void
meta_window_flush_calc_showing (MetaWindow *window)
{
  if (window->calc_showing_queued)
    {
      meta_window_unqueue_calc_showing (window);
      meta_window_calc_showing (window);
    }
}

void
meta_window_queue_calc_showing (MetaWindow  *window)
{
  /* if withdrawn = TRUE then unmanaging should also be TRUE,
   * really.
   */
  if (window->unmanaging || window->withdrawn)
    return;

  if (window->calc_showing_queued)
    return;

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Putting %s in the calc_showing queue\n",
              window->desc);
  
  window->calc_showing_queued = TRUE;
  
  if (calc_showing_idle == 0)
    calc_showing_idle = g_idle_add (idle_calc_showing, NULL);

  calc_showing_pending = g_slist_prepend (calc_showing_pending, window);
}

static gboolean
intervening_user_event_occurred (MetaWindow *window)
{
  guint32 compare;
  MetaWindow *focus_window;

  focus_window = window->display->focus_window;

  meta_topic (META_DEBUG_STARTUP,
              "COMPARISON:\n"
              "  net_wm_user_time_set : %d\n"
              "  net_wm_user_time     : %u\n"
              "  initial_timestamp_set: %d\n"
              "  initial_timestamp    : %u\n",
              window->net_wm_user_time_set,
              window->net_wm_user_time,
              window->initial_timestamp_set,
              window->initial_timestamp);
  if (focus_window != NULL)
    {
      meta_topic (META_DEBUG_STARTUP,
                  "COMPARISON (continued):\n"
                  "  focus_window             : %s\n"
                  "  fw->net_wm_user_time_set : %d\n"
                  "  fw->net_wm_user_time     : %u\n",
                  focus_window->desc,
                  focus_window->net_wm_user_time_set,
                  focus_window->net_wm_user_time);
    }

  /* We expect the most common case for not focusing a new window
   * to be when a hint to not focus it has been set.  Since we can
   * deal with that case rapidly, we use special case it--this is
   * merely a preliminary optimization.  :)
   */
  if ( ((window->net_wm_user_time_set == TRUE) &&
       (window->net_wm_user_time == 0))
      ||
       ((window->initial_timestamp_set == TRUE) &&
       (window->initial_timestamp == 0)))
    {
      meta_topic (META_DEBUG_STARTUP,
                  "window %s explicitly requested no focus\n",
                  window->desc);
      return TRUE;
    }

  if (!(window->net_wm_user_time_set) && !(window->initial_timestamp_set))
    {
      meta_topic (META_DEBUG_STARTUP,
                  "no information about window %s found\n",
                  window->desc);
      return FALSE;
    }

  if (focus_window != NULL &&
      !focus_window->net_wm_user_time_set)
    {
      meta_topic (META_DEBUG_STARTUP,
                  "focus window, %s, doesn't have a user time set yet!\n",
                  window->desc);
      return FALSE;
    }

  /* To determine the "launch" time of an application,
   * startup-notification can set the TIMESTAMP and the
   * application (usually via its toolkit such as gtk or qt) can
   * set the _NET_WM_USER_TIME.  If both are set, then it means
   * the user has interacted with the application since it
   * launched, and _NET_WM_USER_TIME is the value that should be
   * used in the comparison.
   */
  compare = window->initial_timestamp_set ? window->initial_timestamp : 0;
  compare = window->net_wm_user_time_set  ? window->net_wm_user_time  : compare;

  if ((focus_window != NULL) &&
      XSERVER_TIME_IS_BEFORE (compare, focus_window->net_wm_user_time))
    {
      meta_topic (META_DEBUG_STARTUP,
                  "window %s focus prevented by other activity; %u < %u\n",
                  window->desc,
                  compare, 
                  focus_window->net_wm_user_time);
      return TRUE;
    }
  else
    {
      meta_topic (META_DEBUG_STARTUP,
                  "new window %s with no intervening events\n",
                  window->desc);
      return FALSE;
    }
}

/* This function is an ugly hack.  It's experimental in nature and ought to be
 * replaced by a real hint from the app to the WM if we decide the experimental
 * behavior is worthwhile.  The basic idea is to get more feedback about how
 * usage scenarios of "strict" focus users and what they expect.  See #326159.
 */
gboolean
__window_is_terminal (MetaWindow *window)
{
  if (window == NULL || window->res_name == NULL)
    return FALSE;

  /* gnome-terminal -- if you couldn't guess */
  if (strcmp (window->res_name, "gnome-terminal") == 0)
    return TRUE;
  /* xterm, rxvt, aterm */
  else if (strcmp (window->res_name, "XTerm") == 0)
    return TRUE;
  /* konsole, KDE's terminal program */
  else if (strcmp (window->res_name, "Konsole") == 0)
    return TRUE;
  /* rxvt-unicode */
  else if (strcmp (window->res_name, "URxvt") == 0)
    return TRUE;
  /* eterm */
  else if (strcmp (window->res_name, "Eterm") == 0)
    return TRUE;
  /* KTerm -- some terminal not KDE based; so not like Konsole */
  else if (strcmp (window->res_name, "KTerm") == 0)
    return TRUE;
  /* Multi-gnome-terminal */
  else if (strcmp (window->res_name, "Multi-gnome-terminal") == 0)
    return TRUE;
  /* mlterm ("multi lingual terminal emulator on X") */
  else if (strcmp (window->res_name, "mlterm") == 0)
    return TRUE;

  return FALSE;
}

/* This function determines what state the window should have assuming that it
 * and the focus_window have no relation
 */
static void
window_state_on_map (MetaWindow *window, 
                     gboolean *takes_focus,
                     gboolean *places_on_top)
{
  gboolean intervening_events;

  intervening_events = intervening_user_event_occurred (window);

  *takes_focus = !intervening_events;
  *places_on_top = *takes_focus;

  /* don't initially focus windows that are intended to not accept
   * focus
   */
  if (!(window->input || window->take_focus))
    {
      *takes_focus = FALSE;
      return;
    }

  /* Terminal usage may be different; some users intend to launch
   * many apps in quick succession or to just view things in the new
   * window while still interacting with the terminal.  In that case,
   * apps launched from the terminal should not take focus.  This
   * isn't quite the same as not allowing focus to transfer from
   * terminals due to new window map, but the latter is a much easier
   * approximation to enforce so we do that.
   */
  if (*takes_focus &&
      meta_prefs_get_focus_new_windows () == META_FOCUS_NEW_WINDOWS_STRICT &&
      !window->display->allow_terminal_deactivation &&
      __window_is_terminal (window->display->focus_window) &&
      !meta_window_is_ancestor_of_transient (window->display->focus_window,
                                             window))
    {
      meta_topic (META_DEBUG_FOCUS,
                  "focus_window is terminal; not focusing new window.\n");
      *takes_focus = FALSE;
      *places_on_top = FALSE;
    }

  switch (window->type)
    {
    case META_WINDOW_UTILITY:
    case META_WINDOW_TOOLBAR:
      *takes_focus = FALSE;
      *places_on_top = FALSE;
      break;
    case META_WINDOW_DOCK:
    case META_WINDOW_DESKTOP:
    case META_WINDOW_SPLASHSCREEN:
    case META_WINDOW_MENU:
      /* don't focus any of these; places_on_top may be irrelevant for some of
       * these (e.g. dock)--but you never know--the focus window might also be
       * of the same type in some weird situation...
       */
      *takes_focus = FALSE;
      break;
    case META_WINDOW_NORMAL:
    case META_WINDOW_DIALOG:
    case META_WINDOW_MODAL_DIALOG:
      /* The default is correct for these */
      break;
    }
}

static gboolean
windows_overlap (const MetaWindow *w1, const MetaWindow *w2)
{
  MetaRectangle w1rect, w2rect;
  meta_window_get_outer_rect (w1, &w1rect);
  meta_window_get_outer_rect (w2, &w2rect);
  return meta_rectangle_overlap (&w1rect, &w2rect);
}

/* XXX META_EFFECT_*_MAP */
void
meta_window_show (MetaWindow *window)
{
  gboolean did_show;
  gboolean takes_focus_on_map;
  gboolean place_on_top_on_map;
  gboolean needs_stacking_adjustment;
  MetaWindow *focus_window;
  guint32     timestamp;

  /* FIXME: It really sucks to put timestamp pinging here; it'd
   * probably make more sense in implement_showing() so that it's at
   * least not duplicated in finish_minimize.  *shrug*
   */
  timestamp = meta_display_get_current_time_roundtrip (window->display);

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Showing window %s, shaded: %d iconic: %d placed: %d\n",
              window->desc, window->shaded, window->iconic, window->placed);

  focus_window = window->display->focus_window;  /* May be NULL! */
  did_show = FALSE;
  window_state_on_map (window, &takes_focus_on_map, &place_on_top_on_map);
  needs_stacking_adjustment = FALSE;

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Window %s %s focus on map, and %s place on top on map.\n",
              window->desc,
              takes_focus_on_map ? "does" : "does not",
              place_on_top_on_map ? "does" : "does not");

  if ( !takes_focus_on_map &&
       focus_window != NULL &&
       !place_on_top_on_map &&
       window->showing_for_first_time )
    {
      if (meta_window_is_ancestor_of_transient (focus_window, window))
        {
          /* This happens for error dialogs or alerts; these need to remain on
           * top, but it would be confusing to have its ancestor remain
           * focused.
           */
          meta_topic (META_DEBUG_STARTUP,
                      "The focus window %s is an ancestor of the newly mapped "
                      "window %s which isn't being focused.  Unfocusing the "
                      "ancestor.\n",
                      focus_window->desc, window->desc);

          meta_display_focus_the_no_focus_window (window->display,
                                                  window->screen,
                                                  timestamp);
        }
      else
        {
          needs_stacking_adjustment = TRUE;
          if (!window->placed)
            window->denied_focus_and_not_transient = TRUE;
        }
    }

  if (!window->placed)
    {
      /* We have to recalc the placement here since other windows may
       * have been mapped/placed since we last did constrain_position
       */

      /* calc_placement is an efficiency hack to avoid
       * multiple placement calculations before we finally
       * show the window.
       */
      window->calc_placement = TRUE;
      meta_window_move_resize_now (window);
      window->calc_placement = FALSE;

      /* don't ever do the initial position constraint thing again.
       * This is toggled here so that initially-iconified windows
       * still get placed when they are ultimately shown.
       */
      window->placed = TRUE;

      /* Don't want to accidentally reuse the fact that we had been denied
       * focus in any future constraints unless we're denied focus again.
       */
      window->denied_focus_and_not_transient = FALSE;
    }
  
  if (needs_stacking_adjustment)
    {
      gboolean overlap;

      /* needs_stacking_adjustment is only set under certain
       * circumstances currently; if those circumstances change, this
       * code may need re-thinking too
       */
      g_assert(window->showing_for_first_time &&
               !place_on_top_on_map &&
               !takes_focus_on_map &&
               focus_window != NULL);

      /* This window isn't getting focus on map.  We may need to do some
       * special handing with it in regards to
       *   - the stacking of the window
       *   - the MRU position of the window
       *   - the demands attention setting of the window
       */
          
      overlap = windows_overlap (window, focus_window);

      /* We want alt tab to go to the denied-focus window */
      ensure_mru_position_after (window, focus_window);

      /* We don't want the denied-focus window to obscure the focus
       * window, and if we're in both click-to-focus mode and
       * raise-on-click mode then we want to maintain the invariant
       * that MRU order == stacking order.  The need for this if
       * comes from the fact that in sloppy/mouse focus the focus
       * window may not overlap other windows and also can be
       * considered "below" them; this combination means that
       * placing the denied-focus window "below" the focus window
       * in the stack when it doesn't overlap it confusingly places
       * that new window below a lot of other windows.
       */
      if (overlap || 
          (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK &&
           meta_prefs_get_raise_on_click ()))
        meta_window_stack_just_below (window, focus_window);

      /* If the window will be obscured by the focus window, then the
       * user might not notice the window appearing so set the
       * demands attention hint.
       *
       * We set the hint ourselves rather than calling 
       * meta_window_set_demands_attention() because that would cause
       * a recalculation of overlap, and a call to set_net_wm_state()
       * which we are going to call ourselves here a few lines down.
       */
      if (overlap)
        window->wm_state_demands_attention = TRUE;
    } 

  /* Shaded means the frame is mapped but the window is not */
  
  if (window->frame && !window->frame->mapped)
    {
      meta_topic (META_DEBUG_WINDOW_STATE,
                  "Frame actually needs map\n");
      window->frame->mapped = TRUE;
      meta_ui_map_frame (window->screen->ui, window->frame->xwindow);
      did_show = TRUE;
    }

  if (window->shaded)
    {
      if (window->mapped)
        {          
          meta_topic (META_DEBUG_WINDOW_STATE,
                      "%s actually needs unmap (shaded)\n", window->desc);
          meta_topic (META_DEBUG_WINDOW_STATE,
                      "Incrementing unmaps_pending on %s for shade\n",
                      window->desc);
          window->mapped = FALSE;
          window->unmaps_pending += 1;
          meta_error_trap_push (window->display);
          XUnmapWindow (window->display->xdisplay, window->xwindow);
          meta_error_trap_pop (window->display, FALSE);
        }

      if (!window->iconic)
        {
          window->iconic = TRUE;
          set_wm_state (window, IconicState);
        }
    }
  else
    {
      if (!window->mapped)
        {
          meta_topic (META_DEBUG_WINDOW_STATE,
                      "%s actually needs map\n", window->desc);
          window->mapped = TRUE;
          meta_error_trap_push (window->display);
          XMapWindow (window->display->xdisplay, window->xwindow);
          meta_error_trap_pop (window->display, FALSE);
          did_show = TRUE;
          
          if (window->was_minimized)
            {
              MetaRectangle window_rect;
              MetaRectangle icon_rect;
              
              window->was_minimized = FALSE;
              
              if (meta_window_get_icon_geometry (window, &icon_rect))
                {
                  meta_window_get_outer_rect (window, &window_rect);
                  
                  meta_effect_run_unminimize (window,
                                              &window_rect,
                                              &icon_rect,
                                              NULL, NULL);
                }
            }
        }      
      
      if (window->iconic)
        {
          window->iconic = FALSE;
          set_wm_state (window, NormalState);
        }
    }

  /* We don't want to worry about all cases from inside
   * implement_showing(); we only want to worry about focus if this
   * window has not been shown before.
   */
  if (window->showing_for_first_time)
    {
      window->showing_for_first_time = FALSE;
      if (takes_focus_on_map)
        {                
          meta_window_focus (window, timestamp);
        }
      else
        {
          /* Prevent EnterNotify events in sloppy/mouse focus from
           * erroneously focusing the window that had been denied
           * focus.  FIXME: This introduces a race; I have a couple
           * ideas for a better way to accomplish the same thing, but
           * they're more involved so do it this way for now.
           */
          meta_display_increment_focus_sentinel (window->display);
        }
    }

  set_net_wm_state (window);

  if (did_show && window->struts)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Mapped window %s with struts, so invalidating work areas\n",
                  window->desc);
      invalidate_work_areas (window);
    }
}

/* XXX META_EFFECT_*_UNMAP */
static void
meta_window_hide (MetaWindow *window)
{
  gboolean did_hide;
  
  meta_topic (META_DEBUG_WINDOW_STATE,
              "Hiding window %s\n", window->desc);

  did_hide = FALSE;
  
  if (window->frame && window->frame->mapped)
    {
      meta_topic (META_DEBUG_WINDOW_STATE, "Frame actually needs unmap\n");
      window->frame->mapped = FALSE;
      meta_ui_unmap_frame (window->screen->ui, window->frame->xwindow);
      did_hide = TRUE;
    }

  if (window->mapped)
    {
      meta_topic (META_DEBUG_WINDOW_STATE,
                  "%s actually needs unmap\n", window->desc);
      meta_topic (META_DEBUG_WINDOW_STATE,
                  "Incrementing unmaps_pending on %s for hide\n",
                  window->desc);
      window->mapped = FALSE;
      window->unmaps_pending += 1;
      meta_error_trap_push (window->display);      
      XUnmapWindow (window->display->xdisplay, window->xwindow);
      meta_error_trap_pop (window->display, FALSE);
      did_hide = TRUE;
    }

  if (!window->iconic)
    {
      window->iconic = TRUE;
      set_wm_state (window, IconicState);
    }
  
  set_net_wm_state (window);
      
  if (did_hide && window->struts)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Unmapped window %s with struts, so invalidating work areas\n",
                  window->desc);
      invalidate_work_areas (window);
    }
}

static gboolean
queue_calc_showing_func (MetaWindow *window,
                         void       *data)
{
  meta_window_queue_calc_showing (window);
  return TRUE;
}

void
meta_window_minimize (MetaWindow  *window)
{
  if (!window->minimized)
    {
      window->minimized = TRUE;
      meta_window_queue_calc_showing (window);

      meta_window_foreach_transient (window,
                                     queue_calc_showing_func,
                                     NULL);
      
      if (window->has_focus)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Focusing default window due to minimization of focus window %s\n",
                      window->desc);
        }
      else
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Minimizing window %s which doesn't have the focus\n",
                      window->desc);
        }
    }
}

void
meta_window_unminimize (MetaWindow  *window)
{
  if (window->minimized)
    {
      window->minimized = FALSE;
      window->was_minimized = TRUE;
      meta_window_queue_calc_showing (window);

      meta_window_foreach_transient (window,
                                     queue_calc_showing_func,
                                     NULL);
    }
}

static void
meta_window_save_rect (MetaWindow *window)
{
  if (!(META_WINDOW_MAXIMIZED (window) || window->fullscreen))
    {
      /* save size/pos as appropriate args for move_resize */
      if (!window->maximized_horizontally)
        {
          window->saved_rect.x      = window->rect.x;
          window->saved_rect.width  = window->rect.width;
          if (window->frame)
            window->saved_rect.x   += window->frame->rect.x;
        }
      if (!window->maximized_vertically)
        {
          window->saved_rect.y      = window->rect.y;
          window->saved_rect.height = window->rect.height;
          if (window->frame)
            window->saved_rect.y   += window->frame->rect.y;
        }
    }
}

void
meta_window_maximize_internal (MetaWindow        *window,
                               MetaMaximizeFlags  directions,
                               MetaRectangle     *saved_rect)
{
  /* At least one of the two directions ought to be set */
  gboolean maximize_horizontally, maximize_vertically;
  maximize_horizontally = directions & META_MAXIMIZE_HORIZONTAL;
  maximize_vertically   = directions & META_MAXIMIZE_VERTICAL;
  g_assert (maximize_horizontally || maximize_vertically);

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Maximizing %s%s\n",
              window->desc,
              maximize_horizontally && maximize_vertically ? "" :
                maximize_horizontally ? " horizontally" :
                  maximize_vertically ? " vertically" : "BUGGGGG");
  
  if (saved_rect != NULL)
    window->saved_rect = *saved_rect;
  else
    meta_window_save_rect (window);
  
  window->maximized_horizontally = 
    window->maximized_horizontally || maximize_horizontally;
  window->maximized_vertically = 
    window->maximized_vertically   || maximize_vertically;

  /* Fix for #336850: If the frame shape isn't reapplied, it is
   * possible that the frame will retains its rounded corners. That
   * happens if the client's size when maximized equals the unmaximized
   * size.
   */
  if (window->frame)
    window->frame->need_reapply_frame_shape = TRUE;
  
  recalc_window_features (window);
  set_net_wm_state (window);
}

void
meta_window_maximize (MetaWindow        *window,
                      MetaMaximizeFlags  directions)
{
  /* At least one of the two directions ought to be set */
  gboolean maximize_horizontally, maximize_vertically;
  maximize_horizontally = directions & META_MAXIMIZE_HORIZONTAL;
  maximize_vertically   = directions & META_MAXIMIZE_VERTICAL;
  g_assert (maximize_horizontally || maximize_vertically);

  /* Only do something if the window isn't already maximized in the
   * given direction(s).
   */
  if ((maximize_horizontally && !window->maximized_horizontally) ||
      (maximize_vertically   && !window->maximized_vertically))
    {
      if (window->shaded && maximize_vertically)
        {
          /* Shading sucks anyway; I'm not adding a timestamp argument
           * to this function just for this niche usage & corner case.
           */
          guint32 timestamp = 
            meta_display_get_current_time_roundtrip (window->display);
          meta_window_unshade (window, timestamp);
        }
      
      /* if the window hasn't been placed yet, we'll maximize it then
       */
      if (!window->placed)
	{
	  window->maximize_horizontally_after_placement = 
            window->maximize_horizontally_after_placement || 
            maximize_horizontally;
	  window->maximize_vertically_after_placement = 
            window->maximize_vertically_after_placement || 
            maximize_vertically;
	  return;
	}

      meta_window_maximize_internal (window, 
                                     directions,
                                     NULL);

      /* move_resize with new maximization constraints
       */
      meta_window_queue_move_resize (window);
    }
}

static void
unmaximize_window_before_freeing (MetaWindow        *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Unmaximizing %s just before freeing\n",
              window->desc);
      
  window->rect = window->saved_rect;
  send_configure_notify (window);

  window->maximized_horizontally = FALSE;
  window->maximized_vertically = FALSE;
  set_net_wm_state (window);
}

void
meta_window_unmaximize (MetaWindow        *window,
                        MetaMaximizeFlags  directions)
{
  /* At least one of the two directions ought to be set */
  gboolean unmaximize_horizontally, unmaximize_vertically;
  unmaximize_horizontally = directions & META_MAXIMIZE_HORIZONTAL;
  unmaximize_vertically   = directions & META_MAXIMIZE_VERTICAL;
  g_assert (unmaximize_horizontally || unmaximize_vertically);

  /* Only do something if the window isn't already maximized in the
   * given direction(s).
   */
  if ((unmaximize_horizontally && window->maximized_horizontally) ||
      (unmaximize_vertically   && window->maximized_vertically))
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Unmaximizing %s%s\n",
                  window->desc,
                  unmaximize_horizontally && unmaximize_vertically ? "" :
                    unmaximize_horizontally ? " horizontally" :
                      unmaximize_vertically ? " vertically" : "BUGGGGG");
      
      window->maximized_horizontally = 
        window->maximized_horizontally && !unmaximize_horizontally;
      window->maximized_vertically = 
        window->maximized_vertically   && !unmaximize_vertically;

      /* When we unmaximize, if we're doing a mouse move also we could
       * get the window suddenly jumping to the upper left corner of
       * the workspace, since that's where it was when the grab op
       * started.  So we need to update the grab state.
       */
      if (meta_grab_op_is_moving (window->display->grab_op) &&
          window->display->grab_window == window)
        {
          window->display->grab_anchor_window_pos = window->saved_rect;
        }

      meta_window_move_resize (window,
                               FALSE,
                               window->saved_rect.x,
                               window->saved_rect.y,
                               window->saved_rect.width,
                               window->saved_rect.height);

      if (window->display->grab_wireframe_active)
        {
          window->display->grab_wireframe_rect = window->saved_rect;
        }

      recalc_window_features (window);
      set_net_wm_state (window);
    }
}

void
meta_window_make_above (MetaWindow  *window)
{
  window->wm_state_above = TRUE;
  meta_window_update_layer (window);
  meta_window_raise (window);
  set_net_wm_state (window);
}

void
meta_window_unmake_above (MetaWindow  *window)
{
  window->wm_state_above = FALSE;
  meta_window_raise (window);
  meta_window_update_layer (window);
  set_net_wm_state (window);
}

void
meta_window_make_fullscreen_internal (MetaWindow  *window)
{
  if (!window->fullscreen)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Fullscreening %s\n", window->desc);

      if (window->shaded)
        {
          /* Shading sucks anyway; I'm not adding a timestamp argument
           * to this function just for this niche usage & corner case.
           */
          guint32 timestamp = 
            meta_display_get_current_time_roundtrip (window->display);
          meta_window_unshade (window, timestamp);
        }

      meta_window_save_rect (window);
      
      window->fullscreen = TRUE;

      meta_stack_freeze (window->screen->stack);
      meta_window_update_layer (window);
      
      meta_window_raise (window);
      meta_stack_thaw (window->screen->stack);
      
      recalc_window_features (window);
      set_net_wm_state (window);
    }
}

void
meta_window_make_fullscreen (MetaWindow  *window)
{
  if (!window->fullscreen)
    {
      meta_window_make_fullscreen_internal (window);
      /* move_resize with new constraints
       */
      meta_window_queue_move_resize (window);
    }
}

void
meta_window_unmake_fullscreen (MetaWindow  *window)
{
  if (window->fullscreen)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Unfullscreening %s\n", window->desc);
      
      window->fullscreen = FALSE;

      meta_window_move_resize (window,
                               FALSE,
                               window->saved_rect.x,
                               window->saved_rect.y,
                               window->saved_rect.width,
                               window->saved_rect.height);

      meta_window_update_layer (window);
      
      recalc_window_features (window);
      set_net_wm_state (window);
    }
}

void
meta_window_shade (MetaWindow  *window,
                   guint32      timestamp)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Shading %s\n", window->desc);
  if (!window->shaded)
    {
#if 0
      if (window->mapped)
        {
          /* Animation */
          MetaRectangle starting_size;
          MetaRectangle titlebar_size;
          
          meta_window_get_outer_rect (window, &starting_size);
          if (window->frame)
            {
              starting_size.y += window->frame->child_y;
              starting_size.height -= window->frame->child_y;
            }
          titlebar_size = starting_size;
          titlebar_size.height = 0;
          
          meta_effects_draw_box_animation (window->screen,
                                           &starting_size,
                                           &titlebar_size,
                                           META_SHADE_ANIMATION_LENGTH,
                                           META_BOX_ANIM_SLIDE_UP);
        }
#endif
      
      window->shaded = TRUE;

      meta_window_queue_move_resize (window);
      meta_window_queue_calc_showing (window);

      /* After queuing the calc showing, since _focus flushes it,
       * and we need to focus the frame
       */
      meta_topic (META_DEBUG_FOCUS,
                  "Re-focusing window %s after shading it\n",
                  window->desc);
      meta_window_focus (window, timestamp);
      
      set_net_wm_state (window);
    }
}

void
meta_window_unshade (MetaWindow  *window,
                     guint32      timestamp)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Unshading %s\n", window->desc);
  if (window->shaded)
    {
      window->shaded = FALSE;
      meta_window_queue_move_resize (window);
      meta_window_queue_calc_showing (window);

      /* focus the window */
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing window %s after unshading it\n",
                  window->desc);
      meta_window_focus (window, timestamp);

      set_net_wm_state (window);
    }
}

static gboolean
unminimize_func (MetaWindow *window,
                 void       *data)
{
  meta_window_unminimize (window);
  return TRUE;
}

static void
unminimize_window_and_all_transient_parents (MetaWindow *window)
{
  meta_window_unminimize (window);
  meta_window_foreach_ancestor (window, unminimize_func, NULL);
}

static void
window_activate (MetaWindow     *window,
                 guint32         timestamp,
                 MetaClientType  source_indication,
                 MetaWorkspace  *workspace)
{
  gboolean can_ignore_outdated_timestamps;
  meta_topic (META_DEBUG_FOCUS,
              "_NET_ACTIVE_WINDOW message sent for %s at time %u "
              "by client type %u.\n",
              window->desc, timestamp, source_indication);

  /* Older EWMH spec didn't specify a timestamp; we decide to honor these only
   * if the app specifies that it is a pager.
   *
   * Update: Unconditionally honor 0 timestamps for now; we'll fight
   * that battle later.  Just remove the "FALSE &&" in order to only
   * honor 0 timestamps for pagers.
   */
  can_ignore_outdated_timestamps =
    (timestamp != 0 || (FALSE && source_indication != META_CLIENT_TYPE_PAGER));
  if (XSERVER_TIME_IS_BEFORE (timestamp, window->display->last_user_time) &&
      can_ignore_outdated_timestamps)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "last_user_time (%u) is more recent; ignoring "
                  " _NET_ACTIVE_WINDOW message.\n",
                  window->display->last_user_time);
      meta_window_set_demands_attention(window);
      return;
    }

  /* For those stupid pagers, get a valid timestamp and show a warning */  
  if (timestamp == 0)
    {
      meta_warning ("meta_window_activate called by a pager with a 0 timestamp; "
                    "the pager needs to be fixed.\n");
      timestamp = meta_display_get_current_time_roundtrip (window->display);
    }

  meta_window_set_user_time (window, timestamp);

  /* disable show desktop mode unless we're a desktop component */
  maybe_leave_show_desktop_mode (window);
 
  /* Get window on current or given workspace */
  if (workspace == NULL)
    workspace = window->screen->active_workspace;
  if (!meta_window_located_on_workspace (window, workspace))
    meta_window_change_workspace (window, workspace);
  
  if (window->shaded)
    meta_window_unshade (window, timestamp);

  unminimize_window_and_all_transient_parents (window);

  if (meta_prefs_get_raise_on_click () ||
      source_indication == META_CLIENT_TYPE_PAGER)
    meta_window_raise (window);

  meta_topic (META_DEBUG_FOCUS,
              "Focusing window %s due to activation\n",
              window->desc);
  meta_window_focus (window, timestamp);
}

/* This function exists since most of the functionality in window_activate
 * is useful for Metacity, but Metacity shouldn't need to specify a client
 * type for itself.  ;-)
 */
void
meta_window_activate (MetaWindow     *window,
                      guint32         timestamp)
{
  /* We're not really a pager, but the behavior we want is the same as if
   * we were such.  If we change the pager behavior later, we could revisit
   * this and just add extra flags to window_activate.
   */
  window_activate (window, timestamp, META_CLIENT_TYPE_PAGER, NULL);
}

void
meta_window_activate_with_workspace (MetaWindow     *window,
                                     guint32         timestamp,
                                     MetaWorkspace  *workspace)
{
  /* We're not really a pager, but the behavior we want is the same as if
   * we were such.  If we change the pager behavior later, we could revisit
   * this and just add extra flags to window_activate.
   */
  window_activate (window, timestamp, META_CLIENT_TYPE_APPLICATION, workspace);
}

/* Manually fix all the weirdness explained in the big comment at the
 * beginning of meta_window_move_resize_internal() giving positions
 * expected by meta_window_constrain (i.e. positions & sizes of the
 * internal or client window).
 */
static void
adjust_for_gravity (MetaWindow        *window,
                    MetaFrameGeometry *fgeom,
                    gboolean           coords_assume_border,
                    int                gravity,
                    MetaRectangle     *rect)
{
  int ref_x, ref_y;
  int bw;
  int child_x, child_y;
  int frame_width, frame_height;
  
  if (coords_assume_border)
    bw = window->border_width;
  else
    bw = 0;

  if (fgeom)
    {
      child_x = fgeom->left_width;
      child_y = fgeom->top_height;
      frame_width = child_x + rect->width + fgeom->right_width;
      frame_height = child_y + rect->height + fgeom->bottom_height;
    }
  else
    {
      child_x = 0;
      child_y = 0;
      frame_width = rect->width;
      frame_height = rect->height;
    }
  
  /* We're computing position to pass to window_move, which is
   * the position of the client window (StaticGravity basically)
   *
   * (see WM spec description of gravity computation, but note that
   * their formulas assume we're honoring the border width, rather
   * than compensating for having turned it off)
   */
  switch (gravity)
    {
    case NorthWestGravity:
      ref_x = rect->x;
      ref_y = rect->y;
      break;
    case NorthGravity:
      ref_x = rect->x + rect->width / 2 + bw;
      ref_y = rect->y;
      break;
    case NorthEastGravity:
      ref_x = rect->x + rect->width + bw * 2;
      ref_y = rect->y;
      break;
    case WestGravity:
      ref_x = rect->x;
      ref_y = rect->y + rect->height / 2 + bw;
      break;
    case CenterGravity:
      ref_x = rect->x + rect->width / 2 + bw;
      ref_y = rect->y + rect->height / 2 + bw;
      break;
    case EastGravity:
      ref_x = rect->x + rect->width + bw * 2;
      ref_y = rect->y + rect->height / 2 + bw;
      break;
    case SouthWestGravity:
      ref_x = rect->x;
      ref_y = rect->y + rect->height + bw * 2;
      break;
    case SouthGravity:
      ref_x = rect->x + rect->width / 2 + bw;
      ref_y = rect->y + rect->height + bw * 2;
      break;
    case SouthEastGravity:
      ref_x = rect->x + rect->width + bw * 2;
      ref_y = rect->y + rect->height + bw * 2;
      break;
    case StaticGravity:
    default:
      ref_x = rect->x;
      ref_y = rect->y;
      break;
    }

  switch (gravity)
    {
    case NorthWestGravity:
      rect->x = ref_x + child_x;
      rect->y = ref_y + child_y;
      break;
    case NorthGravity:
      rect->x = ref_x - frame_width / 2 + child_x;
      rect->y = ref_y + child_y;
      break;
    case NorthEastGravity:
      rect->x = ref_x - frame_width + child_x;
      rect->y = ref_y + child_y;
      break;
    case WestGravity:
      rect->x = ref_x + child_x;
      rect->y = ref_y - frame_height / 2 + child_y;
      break;
    case CenterGravity:
      rect->x = ref_x - frame_width / 2 + child_x;
      rect->y = ref_y - frame_height / 2 + child_y;
      break;
    case EastGravity:
      rect->x = ref_x - frame_width + child_x;
      rect->y = ref_y - frame_height / 2 + child_y;
      break;
    case SouthWestGravity:
      rect->x = ref_x + child_x;
      rect->y = ref_y - frame_height + child_y;
      break;
    case SouthGravity:
      rect->x = ref_x - frame_width / 2 + child_x;
      rect->y = ref_y - frame_height + child_y;
      break;
    case SouthEastGravity:
      rect->x = ref_x - frame_width + child_x;
      rect->y = ref_y - frame_height + child_y;
      break;
    case StaticGravity:
    default:
      rect->x = ref_x;
      rect->y = ref_y;
      break;
    }
}

static gboolean
static_gravity_works (MetaDisplay *display)
{
  return display->static_gravity_works;
}

#ifdef HAVE_XSYNC
static void
send_sync_request (MetaWindow *window)
{
  XSyncValue value;
  XClientMessageEvent ev;

  window->sync_request_serial++;

  XSyncIntToValue (&value, window->sync_request_serial);
  
  ev.type = ClientMessage;
  ev.window = window->xwindow;
  ev.message_type = window->display->atom_wm_protocols;
  ev.format = 32;
  ev.data.l[0] = window->display->atom_net_wm_sync_request;
  /* FIXME: meta_display_get_current_time() is bad, but since calls
   * come from meta_window_move_resize_internal (which in turn come
   * from all over), I'm not sure what we can do to fix it.  Do we
   * want to use _roundtrip, though?
   */
  ev.data.l[1] = meta_display_get_current_time (window->display);
  ev.data.l[2] = XSyncValueLow32 (value);
  ev.data.l[3] = XSyncValueHigh32 (value);

  /* We don't need to trap errors here as we are already
   * inside an error_trap_push()/pop() pair.
   */
  XSendEvent (window->display->xdisplay,
	      window->xwindow, False, 0, (XEvent*) &ev);

  g_get_current_time (&window->sync_request_time);
}
#endif

static void
meta_window_move_resize_internal (MetaWindow  *window,
                                  MetaMoveResizeFlags flags,
                                  int          resize_gravity,
                                  int          root_x_nw,
                                  int          root_y_nw,
                                  int          w,
                                  int          h)
{
  /* meta_window_move_resize_internal gets called with very different
   * meanings for root_x_nw and root_y_nw.  w & h are always the area of
   * the inner or client window (i.e. excluding the frame) and the
   * resize_gravity is always the gravity associated with the resize or
   * move_resize request (the gravity is ignored for move-only operations).
   * But the location is different because of how this function gets
   * called; note that in all cases what we want to find out is the upper
   * left corner of the position of the inner window:
   *
   *   Case | Called from (flags; resize_gravity)
   *   -----+-----------------------------------------------
   *    1   | A resize only ConfigureRequest
   *    1   | meta_window_resize
   *    1   | meta_window_resize_with_gravity
   *    2   | New window
   *    2   | Session restore
   *    2   | A not-resize-only ConfigureRequest
   *    3   | meta_window_move
   *    3   | meta_window_move_resize
   *    4   | various functions via handle_net_moveresize_window() in display.c
   *
   * For each of the cases, root_x_nw and root_y_nw must be treated as follows:
   *
   *   (1) They should be entirely ignored; instead the previous position
   *       and size of the window should be resized according to the given
   *       gravity in order to determine the new position of the window.
   *   (2) Needs to be fixed up by adjust_for_gravity() as these
   *       coordinates are relative to some corner or side of the outer
   *       window (except for the case of StaticGravity) and we want to
   *       know the location of the upper left corner of the inner window.
   *   (3) These values are already the desired positon of the NW corner
   *       of the inner window
   *   (4) The place that calls this function this way must be fixed; it is
   *       wrong.
   */
  XWindowChanges values;
  unsigned int mask;
  gboolean need_configure_notify;
  MetaFrameGeometry fgeom;
  gboolean need_move_client = FALSE;
  gboolean need_move_frame = FALSE;
  gboolean need_resize_client = FALSE;
  gboolean need_resize_frame = FALSE;
  int frame_size_dx;
  int frame_size_dy;
  int size_dx;
  int size_dy;
  gboolean is_configure_request;
  gboolean do_gravity_adjust;
  gboolean is_user_action;
  gboolean configure_frame_first;
  gboolean use_static_gravity;
  /* used for the configure request, but may not be final
   * destination due to StaticGravity etc.
   */
  int client_move_x;
  int client_move_y;
  MetaRectangle new_rect;
  MetaRectangle old_rect;
  
  is_configure_request = (flags & META_IS_CONFIGURE_REQUEST) != 0;
  do_gravity_adjust = (flags & META_DO_GRAVITY_ADJUST) != 0;
  is_user_action = (flags & META_IS_USER_ACTION) != 0;

  /* The action has to be a move or a resize or both... */
  g_assert (flags & (META_IS_MOVE_ACTION | META_IS_RESIZE_ACTION));
  
  /* We don't need it in the idle queue anymore. */
  meta_window_unqueue_move_resize (window);

  old_rect = window->rect;
  meta_window_get_position (window, &old_rect.x, &old_rect.y);
  
  meta_topic (META_DEBUG_GEOMETRY,
              "Move/resize %s to %d,%d %dx%d%s%s from %d,%d %dx%d\n",
              window->desc, root_x_nw, root_y_nw, w, h,
              is_configure_request ? " (configure request)" : "",
              is_user_action ? " (user move/resize)" : "",
              old_rect.x, old_rect.y, old_rect.width, old_rect.height);
  
  if (window->frame)
    meta_frame_calc_geometry (window->frame,
                              &fgeom);

  new_rect.x = root_x_nw;
  new_rect.y = root_y_nw;
  new_rect.width  = w;
  new_rect.height = h;

  /* If this is a resize only, the position should be ignored and
   * instead obtained by resizing the old rectangle according to the
   * relevant gravity.
   */
  if ((flags & (META_IS_MOVE_ACTION | META_IS_RESIZE_ACTION)) == 
      META_IS_RESIZE_ACTION)
    { 
      meta_rectangle_resize_with_gravity (&old_rect,
                                          &new_rect,
                                          resize_gravity,
                                          new_rect.width,
                                          new_rect.height);

      meta_topic (META_DEBUG_GEOMETRY,
                  "Compensated for gravity in resize action; new pos %d,%d\n",
                  new_rect.x, new_rect.y);
    }
  else if (is_configure_request || do_gravity_adjust)
    {      
      adjust_for_gravity (window,
                          window->frame ? &fgeom : NULL,
                          /* configure request coords assume
                           * the border width existed
                           */
                          is_configure_request,
                          window->size_hints.win_gravity,
                          &new_rect);

      meta_topic (META_DEBUG_GEOMETRY,
                  "Compensated for configure_request/do_gravity_adjust needing "
                  "weird positioning; new pos %d,%d\n",
                  new_rect.x, new_rect.y);
    }

  meta_window_constrain (window,
                         window->frame ? &fgeom : NULL,
                         flags,
                         resize_gravity,
                         &old_rect,
                         &new_rect);

  w = new_rect.width;
  h = new_rect.height;
  root_x_nw = new_rect.x;
  root_y_nw = new_rect.y;
  
  if (w != window->rect.width ||
      h != window->rect.height)
    need_resize_client = TRUE;  
  
  window->rect.width = w;
  window->rect.height = h;
  
  if (window->frame)
    {
      int new_w, new_h;

      new_w = window->rect.width + fgeom.left_width + fgeom.right_width;

      if (window->shaded)
        new_h = fgeom.top_height;
      else
        new_h = window->rect.height + fgeom.top_height + fgeom.bottom_height;

      frame_size_dx = new_w - window->frame->rect.width;
      frame_size_dy = new_h - window->frame->rect.height;

      need_resize_frame = (frame_size_dx != 0 || frame_size_dy != 0);
      
      window->frame->rect.width = new_w;
      window->frame->rect.height = new_h;

      meta_topic (META_DEBUG_GEOMETRY,
                  "Calculated frame size %dx%d\n",
                  window->frame->rect.width,
                  window->frame->rect.height);
    }
  else
    {
      frame_size_dx = 0;
      frame_size_dy = 0;
    }

  /* For nice effect, when growing the window we want to move/resize
   * the frame first, when shrinking the window we want to move/resize
   * the client first. If we grow one way and shrink the other,
   * see which way we're moving "more"
   *
   * Mail from Owen subject "Suggestion: Gravity and resizing from the left"
   * http://mail.gnome.org/archives/wm-spec-list/1999-November/msg00088.html
   *
   * An annoying fact you need to know in this code is that StaticGravity
   * does nothing if you _only_ resize or _only_ move the frame;
   * it must move _and_ resize, otherwise you get NorthWestGravity
   * behavior. The move and resize must actually occur, it is not
   * enough to set CWX | CWWidth but pass in the current size/pos.
   */
      
  if (window->frame)
    {
      int new_x, new_y;
      int frame_pos_dx, frame_pos_dy;
      
      /* Compute new frame coords */
      new_x = root_x_nw - fgeom.left_width;
      new_y = root_y_nw - fgeom.top_height;

      frame_pos_dx = new_x - window->frame->rect.x;
      frame_pos_dy = new_y - window->frame->rect.y;

      need_move_frame = (frame_pos_dx != 0 || frame_pos_dy != 0);
      
      window->frame->rect.x = new_x;
      window->frame->rect.y = new_y;      

      /* If frame will both move and resize, then StaticGravity
       * on the child window will kick in and implicitly move
       * the child with respect to the frame. The implicit
       * move will keep the child in the same place with
       * respect to the root window. If frame only moves
       * or only resizes, then the child will just move along
       * with the frame.
       */

      /* window->rect.x, window->rect.y are relative to frame,
       * remember they are the server coords
       */
          
      new_x = fgeom.left_width;
      new_y = fgeom.top_height;

      if (need_resize_frame && need_move_frame &&
          static_gravity_works (window->display))
        {
          /* static gravity kicks in because frame
           * is both moved and resized
           */
          /* when we move the frame by frame_pos_dx, frame_pos_dy the
           * client will implicitly move relative to frame by the
           * inverse delta.
           * 
           * When moving client then frame, we move the client by the
           * frame delta, to be canceled out by the implicit move by
           * the inverse frame delta, resulting in a client at new_x,
           * new_y.
           *
           * When moving frame then client, we move the client
           * by the same delta as the frame, because the client
           * was "left behind" by the frame - resulting in a client
           * at new_x, new_y.
           *
           * In both cases we need to move the client window
           * in all cases where we had to move the frame window.
           */
          
          client_move_x = new_x + frame_pos_dx;
          client_move_y = new_y + frame_pos_dy;

          if (need_move_frame)
            need_move_client = TRUE;

          use_static_gravity = TRUE;
        }
      else
        {
          client_move_x = new_x;
          client_move_y = new_y;

          if (client_move_x != window->rect.x ||
              client_move_y != window->rect.y)
            need_move_client = TRUE;

          use_static_gravity = FALSE;
        }

      /* This is the final target position, but not necessarily what
       * we pass to XConfigureWindow, due to StaticGravity implicit
       * movement.
       */      
      window->rect.x = new_x;
      window->rect.y = new_y;
    }
  else
    {
      if (root_x_nw != window->rect.x ||
          root_y_nw != window->rect.y)
        need_move_client = TRUE;
      
      window->rect.x = root_x_nw;
      window->rect.y = root_y_nw;

      client_move_x = window->rect.x;
      client_move_y = window->rect.y;

      use_static_gravity = FALSE;
    }

  /* If frame extents have changed, fill in other frame fields and
     change frame's extents property. */
  if (window->frame &&
      (window->frame->child_x != fgeom.left_width ||
       window->frame->child_y != fgeom.top_height ||
       window->frame->right_width != fgeom.right_width ||
       window->frame->bottom_height != fgeom.bottom_height))
    {
      window->frame->child_x = fgeom.left_width;
      window->frame->child_y = fgeom.top_height;
      window->frame->right_width = fgeom.right_width;
      window->frame->bottom_height = fgeom.bottom_height;

      update_net_frame_extents (window);
    }

  /* See ICCCM 4.1.5 for when to send ConfigureNotify */
  
  need_configure_notify = FALSE;
  
  /* If this is a configure request and we change nothing, then we
   * must send configure notify.
   */
  if  (is_configure_request &&
       !(need_move_client || need_move_frame ||
         need_resize_client || need_resize_frame ||
         window->border_width != 0))
    need_configure_notify = TRUE;

  /* We must send configure notify if we move but don't resize, since
   * the client window may not get a real event
   */
  if ((need_move_client || need_move_frame) &&
      !(need_resize_client || need_resize_frame))
    need_configure_notify = TRUE;
  
  /* The rest of this function syncs our new size/pos with X as
   * efficiently as possible
   */

  /* configure frame first if we grow more than we shrink
   */
  size_dx = w - window->rect.width;
  size_dy = h - window->rect.height;

  configure_frame_first = (size_dx + size_dy >= 0);

  if (use_static_gravity)
    meta_window_set_gravity (window, StaticGravity);  
  
  if (configure_frame_first && window->frame)
    meta_frame_sync_to_window (window->frame,
                               resize_gravity,
                               need_move_frame, need_resize_frame);

  values.border_width = 0;
  values.x = client_move_x;
  values.y = client_move_y;
  values.width = window->rect.width;
  values.height = window->rect.height;
  
  mask = 0;
  if (is_configure_request && window->border_width != 0)
    mask |= CWBorderWidth; /* must force to 0 */
  if (need_move_client)
    mask |= (CWX | CWY);
  if (need_resize_client)
    mask |= (CWWidth | CWHeight);

  if (mask != 0)
    {
      {
        int newx, newy;
        meta_window_get_position (window, &newx, &newy);
        meta_topic (META_DEBUG_GEOMETRY,
                    "Syncing new client geometry %d,%d %dx%d, border: %s pos: %s size: %s\n",
                    newx, newy,
                    window->rect.width, window->rect.height,
                    mask & CWBorderWidth ? "true" : "false",
                    need_move_client ? "true" : "false",
                    need_resize_client ? "true" : "false");
      }
      
      meta_error_trap_push (window->display);

#ifdef HAVE_XSYNC
      if (window->sync_request_counter != None &&
	  window->display->grab_sync_request_alarm != None &&
	  window->sync_request_time.tv_usec == 0 &&
	  window->sync_request_time.tv_sec == 0)
	{
	  /* turn off updating */	
	  if (window->display->compositor)
	    meta_compositor_set_updates (window->display->compositor, window, FALSE);

	  send_sync_request (window);
	}
#endif

      XConfigureWindow (window->display->xdisplay,
                        window->xwindow,
                        mask,
                        &values);
      
      meta_error_trap_pop (window->display, FALSE);
    }

  if (!configure_frame_first && window->frame)
    meta_frame_sync_to_window (window->frame,
                               resize_gravity,
                               need_move_frame, need_resize_frame);  

  /* Put gravity back to be nice to lesser window managers */
  if (use_static_gravity)
    meta_window_set_gravity (window, NorthWestGravity);  
  
  if (need_configure_notify)
    send_configure_notify (window);

  if (is_user_action)
    {
      window->user_has_move_resized = TRUE;

      window->user_rect.width = window->rect.width;
      window->user_rect.height = window->rect.height;

      meta_window_get_position (window, 
                                &window->user_rect.x,
                                &window->user_rect.y);
    }
  
  if (need_move_frame || need_resize_frame ||
      need_move_client || need_resize_client)
    {
      int newx, newy;
      meta_window_get_position (window, &newx, &newy);
      meta_topic (META_DEBUG_GEOMETRY,
                  "New size/position %d,%d %dx%d (user %d,%d %dx%d)\n",
                  newx, newy, window->rect.width, window->rect.height,
                  window->user_rect.x, window->user_rect.y,
                  window->user_rect.width, window->user_rect.height);
    }
  else
    {
      meta_topic (META_DEBUG_GEOMETRY, "Size/position not modified\n");
    }
  
  if (window->display->grab_wireframe_active)
    meta_window_update_wireframe (window, root_x_nw, root_y_nw, w, h);
  else
    meta_window_refresh_resize_popup (window);
  
  /* Invariants leaving this function are:
   *   a) window->rect and frame->rect reflect the actual
   *      server-side size/pos of window->xwindow and frame->xwindow
   *   b) all constraints are obeyed by window->rect and frame->rect
   */
}

void
meta_window_resize (MetaWindow  *window,
                    gboolean     user_op,
                    int          w,
                    int          h)
{
  int x, y;
  MetaMoveResizeFlags flags;

  meta_window_get_position (window, &x, &y);
  
  flags = (user_op ? META_IS_USER_ACTION : 0) | META_IS_RESIZE_ACTION;
  meta_window_move_resize_internal (window,
                                    flags,
                                    NorthWestGravity,
                                    x, y, w, h);
}

void
meta_window_move (MetaWindow  *window,
                  gboolean     user_op,
                  int          root_x_nw,
                  int          root_y_nw)
{
  MetaMoveResizeFlags flags = 
    (user_op ? META_IS_USER_ACTION : 0) | META_IS_MOVE_ACTION;
  meta_window_move_resize_internal (window,
                                    flags,
                                    NorthWestGravity,
                                    root_x_nw, root_y_nw,
                                    window->rect.width,
                                    window->rect.height);
}

void
meta_window_move_resize (MetaWindow  *window,
                         gboolean     user_op,
                         int          root_x_nw,
                         int          root_y_nw,
                         int          w,
                         int          h)
{
  MetaMoveResizeFlags flags = 
    (user_op ? META_IS_USER_ACTION : 0) | 
    META_IS_MOVE_ACTION | META_IS_RESIZE_ACTION;
  meta_window_move_resize_internal (window,
                                    flags,
                                    NorthWestGravity,
                                    root_x_nw, root_y_nw,
                                    w, h);
}

void
meta_window_resize_with_gravity (MetaWindow *window,
                                 gboolean     user_op,
                                 int          w,
                                 int          h,
                                 int          gravity)
{
  int x, y;
  MetaMoveResizeFlags flags;

  meta_window_get_position (window, &x, &y);
  
  flags = (user_op ? META_IS_USER_ACTION : 0) | META_IS_RESIZE_ACTION;
  meta_window_move_resize_internal (window,
                                    flags,
                                    gravity,
                                    x, y, w, h);
}

static void
meta_window_move_resize_now (MetaWindow  *window)
{
  int x, y;

  /* If constraints have changed then we'll snap back to wherever
   * the user had the window
   */
  meta_window_get_user_position (window, &x, &y);

  /* This used to use the user width/height if the user hadn't resized,
   * but it turns out that breaks things pretty often, because configure
   * requests from the app or size hints changes from the app frequently
   * reflect user actions such as changing terminal font size
   * or expanding a disclosure triangle.
   */
  meta_window_move_resize (window, FALSE, x, y,
                           window->rect.width,
                           window->rect.height);
}

static guint move_resize_idle = 0;
static GSList *move_resize_pending = NULL;

static gboolean
idle_move_resize (gpointer data)
{
  GSList *tmp;
  GSList *copy;

  meta_topic (META_DEBUG_GEOMETRY, "Clearing the move_resize queue\n");

  /* Work with a copy, for reentrancy. The allowed reentrancy isn't
   * complete; destroying a window while we're in here would result in
   * badness. But it's OK to queue/unqueue move_resizes.
   */
  copy = g_slist_copy (move_resize_pending);
  g_slist_free (move_resize_pending);
  move_resize_pending = NULL;
  move_resize_idle = 0;

  destroying_windows_disallowed += 1;
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      /* As a side effect, sets window->move_resize_queued = FALSE */
      meta_window_move_resize_now (window); 
      
      tmp = tmp->next;
    }

  g_slist_free (copy);

  destroying_windows_disallowed -= 1;
  
  return FALSE;
}

static void
meta_window_unqueue_move_resize (MetaWindow *window)
{
  if (!window->move_resize_queued)
    return;

  meta_topic (META_DEBUG_GEOMETRY,
              "Removing %s from the move_resize queue\n",
              window->desc);

  /* Note that window may not actually be in move_resize_pending
   * because it may have been in "copy" inside the idle handler
   */
  move_resize_pending = g_slist_remove (move_resize_pending, window);
  window->move_resize_queued = FALSE;
  
  if (move_resize_pending == NULL &&
      move_resize_idle != 0)
    {
      g_source_remove (move_resize_idle);
      move_resize_idle = 0;
    }
}

/* The move/resize queue is only used when we need to
 * recheck the constraints on the window, e.g. when
 * maximizing or when changing struts. Configure requests
 * and such always have to be handled synchronously,
 * they can't be done via a queue.
 */
void
meta_window_queue_move_resize (MetaWindow  *window)
{
  if (window->unmanaging)
    return;

  if (window->move_resize_queued)
    return;

  meta_topic (META_DEBUG_GEOMETRY,
              "Putting %s in the move_resize queue\n",
              window->desc);
  
  window->move_resize_queued = TRUE;
  
  if (move_resize_idle == 0)
    move_resize_idle = g_idle_add_full (META_PRIORITY_RESIZE,
                                        idle_move_resize, NULL, NULL);
  
  move_resize_pending = g_slist_prepend (move_resize_pending, window);
}

void
meta_window_get_position (MetaWindow  *window,
                          int         *x,
                          int         *y)
{
  if (window->frame)
    {
      if (x)
        *x = window->frame->rect.x + window->frame->child_x;
      if (y)
        *y = window->frame->rect.y + window->frame->child_y;
    }
  else
    {
      if (x)
        *x = window->rect.x;
      if (y)
        *y = window->rect.y;
    }
}

void
meta_window_get_user_position  (MetaWindow  *window,
                                int         *x,
                                int         *y)
{
  if (window->user_has_move_resized)
    {
      if (x)
        *x = window->user_rect.x;
      if (y)
        *y = window->user_rect.y;
    }
  else
    {
      meta_window_get_position (window, x, y);
    }
}

void
meta_window_get_gravity_position (MetaWindow  *window,
                                  int         *root_x,
                                  int         *root_y)
{
  MetaRectangle frame_extents;
  int w, h;
  int x, y;
  
  w = window->rect.width;
  h = window->rect.height;

  if (window->size_hints.win_gravity == StaticGravity)
    {
      frame_extents = window->rect;
      if (window->frame)
        {
          frame_extents.x = window->frame->rect.x + window->frame->child_x;
          frame_extents.y = window->frame->rect.y + window->frame->child_y;
        }
    }
  else
    {
      if (window->frame == NULL)
        frame_extents = window->rect;
      else
        frame_extents = window->frame->rect;
    }

  x = frame_extents.x;
  y = frame_extents.y;
  
  switch (window->size_hints.win_gravity)
    {
    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      /* Find center of frame. */
      x += frame_extents.width / 2;
      /* Center client window on that point. */
      x -= w / 2;
      break;
      
    case SouthEastGravity:
    case EastGravity:
    case NorthEastGravity:
      /* Find right edge of frame */
      x += frame_extents.width;
      /* Align left edge of client at that point. */
      x -= w;
      break;
    default:
      break;
    }
  
  switch (window->size_hints.win_gravity)
    {
    case WestGravity:
    case CenterGravity:
    case EastGravity:
      /* Find center of frame. */
      y += frame_extents.height / 2;
      /* Center client window there. */
      y -= h / 2;
      break;
    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      /* Find south edge of frame */
      y += frame_extents.height;
      /* Place bottom edge of client there */
      y -= h;
      break;
    default:
      break;
    }
  
  if (root_x)
    *root_x = x;
  if (root_y)
    *root_y = y;
}

void
meta_window_get_geometry (MetaWindow  *window,
                          int         *x,
                          int         *y,
                          int         *width,
                          int         *height)
{
  meta_window_get_gravity_position (window, x, y);

  *width = (window->rect.width - window->size_hints.base_width) /
    window->size_hints.width_inc;
  *height = (window->rect.height - window->size_hints.base_height) /
    window->size_hints.height_inc;
}

void
meta_window_get_outer_rect (const MetaWindow *window,
                            MetaRectangle    *rect)
{
  if (window->frame)
    *rect = window->frame->rect;
  else
    *rect = window->rect;
}

void
meta_window_get_xor_rect (MetaWindow          *window,
                          const MetaRectangle *grab_wireframe_rect,
                          MetaRectangle       *xor_rect)
{
  if (window->frame)
    {
      xor_rect->x = grab_wireframe_rect->x - window->frame->child_x;
      xor_rect->y = grab_wireframe_rect->y - window->frame->child_y;
      xor_rect->width = grab_wireframe_rect->width + window->frame->child_x + window->frame->right_width;

      if (window->shaded)
        xor_rect->height = window->frame->child_y;
      else
        xor_rect->height = grab_wireframe_rect->height + window->frame->child_y + window->frame->bottom_height;
    }
  else
    *xor_rect = *grab_wireframe_rect;
}

/* Figure out the numbers that show up in the
 * resize popup when in reduced resources mode.
 */
static void
meta_window_get_wireframe_geometry (MetaWindow    *window,
                                    int           *width,
                                    int           *height)
{

  if (!window->display->grab_wireframe_active)
    return;

  if ((width == NULL) || (height == NULL))
    return;

  if ((window->display->grab_window->size_hints.width_inc <= 1) ||
      (window->display->grab_window->size_hints.height_inc <= 1))
    {
      *width = -1;
      *height = -1;
      return;
    }

  *width = window->display->grab_wireframe_rect.width - 
      window->display->grab_window->size_hints.base_width;
  *width /= window->display->grab_window->size_hints.width_inc;

  *height = window->display->grab_wireframe_rect.height -
      window->display->grab_window->size_hints.base_height;
  *height /= window->display->grab_window->size_hints.height_inc;
}

/* XXX META_EFFECT_ALT_TAB, well, this and others */
void
meta_window_begin_wireframe (MetaWindow *window)
{

  MetaRectangle new_xor;
  int display_width, display_height;

  window->display->grab_wireframe_rect = window->rect;
  meta_window_get_position (window, 
                            &window->display->grab_wireframe_rect.x,
                            &window->display->grab_wireframe_rect.y);

  meta_window_get_xor_rect (window, &window->display->grab_wireframe_rect,
                            &new_xor);
  meta_window_get_wireframe_geometry (window, &display_width, &display_height);

  meta_effects_begin_wireframe (window->screen,
                                &new_xor, display_width, display_height);

  window->display->grab_wireframe_last_xor_rect = new_xor;
  window->display->grab_wireframe_last_display_width = display_width;
  window->display->grab_wireframe_last_display_height = display_height;
}

void
meta_window_update_wireframe (MetaWindow *window,
                              int         x,
                              int         y,
                              int         width,
                              int         height)
{

  MetaRectangle new_xor;
  int display_width, display_height;

  window->display->grab_wireframe_rect.x = x;
  window->display->grab_wireframe_rect.y = y;
  window->display->grab_wireframe_rect.width = width;
  window->display->grab_wireframe_rect.height = height;

  meta_window_get_xor_rect (window, &window->display->grab_wireframe_rect,
                            &new_xor);
  meta_window_get_wireframe_geometry (window, &display_width, &display_height);

  meta_effects_update_wireframe (window->screen,
                                 &window->display->grab_wireframe_last_xor_rect,
                                 window->display->grab_wireframe_last_display_width,
                                 window->display->grab_wireframe_last_display_height,
                                 &new_xor, display_width, display_height);

  window->display->grab_wireframe_last_xor_rect = new_xor;
  window->display->grab_wireframe_last_display_width = display_width;
  window->display->grab_wireframe_last_display_height = display_height;
}

void
meta_window_end_wireframe (MetaWindow *window)
{
  meta_effects_end_wireframe (window->display->grab_window->screen,
                              &window->display->grab_wireframe_last_xor_rect,
                              window->display->grab_wireframe_last_display_width,
                              window->display->grab_wireframe_last_display_height);
}

const char*
meta_window_get_startup_id (MetaWindow *window)
{
  if (window->startup_id == NULL)
    {
      MetaGroup *group;

      group = meta_window_get_group (window);

      if (group != NULL)
        return meta_group_get_startup_id (group);
    }

  return window->startup_id;
}

static MetaWindow*
get_modal_transient (MetaWindow *window)
{
  GSList *windows;
  GSList *tmp;
  MetaWindow *modal_transient;

  /* A window can't be the transient of itself, but this is just for
   * convenience in the loop below; we manually fix things up at the
   * end if no real modal transient was found.
   */
  modal_transient = window;

  windows = meta_display_list_windows (window->display);
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *transient = tmp->data;
      
      if (transient->xtransient_for == modal_transient->xwindow &&
          transient->wm_state_modal)
        {
          modal_transient = transient;
          tmp = windows;
          continue;
        }
      
      tmp = tmp->next;
    }

  g_slist_free (windows);

  if (window == modal_transient)
    modal_transient = NULL;

  return modal_transient;
}

/* XXX META_EFFECT_FOCUS */
void
meta_window_focus (MetaWindow  *window,
                   guint32      timestamp)
{  
  MetaWindow *modal_transient;

  meta_topic (META_DEBUG_FOCUS,
              "Setting input focus to window %s, input: %d take_focus: %d\n",
              window->desc, window->input, window->take_focus);
  
  if (window->display->grab_window &&
      window->display->grab_window->all_keys_grabbed)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Current focus window %s has global keygrab, not focusing window %s after all\n",
                  window->display->grab_window->desc, window->desc);
      return;
    }

  modal_transient = get_modal_transient (window);
  if (modal_transient != NULL &&
      !modal_transient->unmanaging)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "%s has %s as a modal transient, so focusing it instead.\n",
                  window->desc, modal_transient->desc);
      if (!modal_transient->on_all_workspaces &&
          modal_transient->workspace != window->screen->active_workspace)
        meta_window_change_workspace (modal_transient, 
                                      window->screen->active_workspace);
      window = modal_transient;
    }

  meta_window_flush_calc_showing (window);

  if (!window->mapped && !window->shaded)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Window %s is not showing, not focusing after all\n",
                  window->desc);
      return;
    }

  /* For output-only or shaded windows, focus the frame.
   * This seems to result in the client window getting key events
   * though, so I don't know if it's icccm-compliant.
   *
   * Still, we have to do this or keynav breaks for these windows.
   */
  if (window->frame &&
      (window->shaded ||
       !(window->input || window->take_focus)))
    {
      if (window->frame)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Focusing frame of %s\n", window->desc);
          meta_display_set_input_focus_window (window->display,
                                               window,
                                               TRUE,
                                               timestamp);
        }
    }
  else
    {
      meta_error_trap_push (window->display);
      
      if (window->input)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Setting input focus on %s since input = true\n",
                      window->desc);
          meta_display_set_input_focus_window (window->display,
                                               window,
                                               FALSE,
                                               timestamp);
        }
      
      if (window->take_focus)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Sending WM_TAKE_FOCUS to %s since take_focus = true\n",
                      window->desc);
          meta_window_send_icccm_message (window,
                                          window->display->atom_wm_take_focus,
                                          timestamp);
          window->display->expected_focus_window = window;
        }
      
      meta_error_trap_pop (window->display, FALSE);
    }

  if (window->wm_state_demands_attention)
    meta_window_unset_demands_attention(window);

  meta_effect_run_focus(window, NULL, NULL);
}

static void
meta_window_change_workspace_without_transients (MetaWindow    *window,
                                                 MetaWorkspace *workspace)
{
  meta_verbose ("Changing window %s to workspace %d\n",
                window->desc, meta_workspace_index (workspace));
  
  /* unstick if stuck. meta_window_unstick would call 
   * meta_window_change_workspace recursively if the window
   * is not in the active workspace.
   */
  if (window->on_all_workspaces)
    meta_window_unstick (window);

  /* See if we're already on this space. If not, make sure we are */
  if (window->workspace != workspace)
    {
      meta_workspace_remove_window (window->workspace, window);
      meta_workspace_add_window (workspace, window);
    }
}

static gboolean
change_workspace_foreach (MetaWindow *window,
                          void       *data)
{
  meta_window_change_workspace_without_transients (window, data);
  return TRUE;
}

void
meta_window_change_workspace (MetaWindow    *window,
                              MetaWorkspace *workspace)
{
  meta_window_change_workspace_without_transients (window, workspace);

  meta_window_foreach_transient (window, change_workspace_foreach,
                                 workspace);
  meta_window_foreach_ancestor (window, change_workspace_foreach,
                                workspace);
}

static void
window_stick_impl (MetaWindow  *window)
{
  GList *tmp; 
  MetaWorkspace *workspace;

  meta_verbose ("Sticking window %s current on_all_workspaces = %d\n",
                window->desc, window->on_all_workspaces);
  
  if (window->on_all_workspaces)
    return;

  /* We don't change window->workspaces, because we revert
   * to that original workspace list if on_all_workspaces is
   * toggled back off.
   */
  window->on_all_workspaces = TRUE;

  /* We do, however, change the MRU lists of all the workspaces
   */
  tmp = window->screen->workspaces;
  while (tmp)
    {
      workspace = (MetaWorkspace *) tmp->data;
      if (!g_list_find (workspace->mru_list, window))
        workspace->mru_list = g_list_prepend (workspace->mru_list, window);

      tmp = tmp->next;
    }

  meta_window_set_current_workspace_hint (window);
  
  meta_window_queue_calc_showing (window);
}

static void
window_unstick_impl (MetaWindow  *window)
{
  GList *tmp;
  MetaWorkspace *workspace;

  if (!window->on_all_workspaces)
    return;

  /* Revert to window->workspaces */

  window->on_all_workspaces = FALSE;

  /* Remove window from MRU lists that it doesn't belong in */
  tmp = window->screen->workspaces;
  while (tmp)
    {
      workspace = (MetaWorkspace *) tmp->data;
      if (window->workspace != workspace)
        workspace->mru_list = g_list_remove (workspace->mru_list, window);
      tmp = tmp->next;
    }

  /* We change ourselves to the active workspace, since otherwise you'd get
   * a weird window-vaporization effect. Once we have UI for being
   * on more than one workspace this should probably be add_workspace
   * not change_workspace.
   */
  if (window->screen->active_workspace != window->workspace)
    meta_window_change_workspace (window, window->screen->active_workspace);
  
  meta_window_set_current_workspace_hint (window);
  
  meta_window_queue_calc_showing (window);
}

static gboolean
stick_foreach_func (MetaWindow *window,
                    void       *data)
{
  gboolean stick;

  stick = *(gboolean*)data;
  if (stick)
    window_stick_impl (window);
  else
    window_unstick_impl (window);
  return TRUE;
}

void
meta_window_stick (MetaWindow  *window)
{
  gboolean stick = TRUE;
  window_stick_impl (window);
  meta_window_foreach_transient (window,
                                 stick_foreach_func,
                                 &stick);
}

void
meta_window_unstick (MetaWindow  *window)
{
  gboolean stick = FALSE;
  window_unstick_impl (window);
  meta_window_foreach_transient (window,
                                 stick_foreach_func,
                                 &stick);
}

unsigned long
meta_window_get_net_wm_desktop (MetaWindow *window)
{
  if (window->on_all_workspaces)
    return 0xFFFFFFFF;
  else
    return meta_workspace_index (window->workspace);
}

static void
update_net_frame_extents (MetaWindow *window)
{
  unsigned long data[4] = { 0, 0, 0, 0 };

  if (window->frame)
    {
      /* Left */
      data[0] = window->frame->child_x;
      /* Right */
      data[1] = window->frame->right_width;
      /* Top */
      data[2] = window->frame->child_y;
      /* Bottom */
      data[3] = window->frame->bottom_height;
    }

  meta_topic (META_DEBUG_GEOMETRY,
              "Setting _NET_FRAME_EXTENTS on managed window 0x%lx "
              "to left = %lu, right = %lu, top = %lu, bottom = %lu\n",
              window->xwindow, data[0], data[1], data[2], data[3]);

  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_frame_extents,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 4);
  meta_error_trap_pop (window->display, FALSE);
}

void
meta_window_set_current_workspace_hint (MetaWindow *window)
{
  /* FIXME if on more than one workspace, we claim to be "sticky",
   * the WM spec doesn't say what to do here.
   */
  unsigned long data[1];

  if (window->workspace == NULL)
    {
      /* this happens when unmanaging windows */      
      return;
    }
  
  data[0] = meta_window_get_net_wm_desktop (window);

  meta_verbose ("Setting _NET_WM_DESKTOP of %s to %lu\n",
                window->desc, data[0]);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_desktop,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (window->display, FALSE);
}

static gboolean
find_root_ancestor (MetaWindow *window,
                    void       *data)
{
  MetaWindow **ancestor = data;

  /* Overwrite the previously "most-root" ancestor with the new one found */
  *ancestor = window;

  /* We want this to continue until meta_window_foreach_ancestor quits because
   * there are no more valid ancestors.
   */
  return TRUE;
}

MetaWindow *
meta_window_find_root_ancestor (MetaWindow *window)
{
  MetaWindow *ancestor;
  ancestor = window;
  meta_window_foreach_ancestor (window, find_root_ancestor, &ancestor);
  return ancestor;
}

void
meta_window_raise (MetaWindow  *window)
{
  MetaWindow *ancestor;
  ancestor = meta_window_find_root_ancestor (window);

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Raising window %s, ancestor of %s\n", 
              ancestor->desc, window->desc);

  /* Raise the ancestor of the window (if the window has no ancestor,
   * then ancestor will be set to the window itself); do this because
   * it's weird to see windows from other apps stacked between a child
   * and parent window of the currently active app.  The stacking
   * constraints in stack.c then magically take care of raising all
   * the child windows appropriately.
   */
  if (window->screen->stack == ancestor->screen->stack)
    meta_stack_raise (window->screen->stack, ancestor);
  else
    {
      meta_warning (
                    "Either stacks aren't per screen or some window has a weird "
                    "transient_for hint; window->screen->stack != "
                    "ancestor->screen->stack.  window = %s, ancestor = %s.\n",
                    window->desc, ancestor->desc);
      /* We could raise the window here, but don't want to do that twice and
       * so we let the case below handle that.
       */
    }

  /* Okay, so stacking constraints misses one case: If a window has
   * two children and we want to raise one of those children, then
   * raising the ancestor isn't enough; we need to also raise the
   * correct child.  See bug 307875.
   */
  if (window != ancestor)
    meta_stack_raise (window->screen->stack, window);
}

void
meta_window_lower (MetaWindow  *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Lowering window %s\n", window->desc);

  meta_stack_lower (window->screen->stack, window);
}

void
meta_window_send_icccm_message (MetaWindow *window,
                                Atom        atom,
                                guint32     timestamp)
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
    
    ev.type = ClientMessage;
    ev.window = window->xwindow;
    ev.message_type = window->display->atom_wm_protocols;
    ev.format = 32;
    ev.data.l[0] = atom;
    ev.data.l[1] = timestamp;

    meta_error_trap_push (window->display);
    XSendEvent (window->display->xdisplay,
                window->xwindow, False, 0, (XEvent*) &ev);
    meta_error_trap_pop (window->display, FALSE);
}

gboolean
meta_window_configure_request (MetaWindow *window,
                               XEvent     *event)
{
  int x, y, width, height;
  gboolean only_resize;
  gboolean allow_position_change;
  gboolean in_grab_op;
  MetaMoveResizeFlags flags;

  /* We ignore configure requests while the user is moving/resizing
   * the window, since these represent the app sucking and fighting
   * the user, most likely due to a bug in the app (e.g. pfaedit
   * seemed to do this)
   *
   * Still have to do the ConfigureNotify and all, but pretend the
   * app asked for the current size/position instead of the new one.
   */  
  in_grab_op = FALSE;
  if (window->display->grab_op != META_GRAB_OP_NONE &&
      window == window->display->grab_window)
    {
      switch (window->display->grab_op)
        {
        case META_GRAB_OP_MOVING:
        case META_GRAB_OP_RESIZING_SE:
        case META_GRAB_OP_RESIZING_S:
        case META_GRAB_OP_RESIZING_SW:
        case META_GRAB_OP_RESIZING_N:
        case META_GRAB_OP_RESIZING_NE:
        case META_GRAB_OP_RESIZING_NW:
        case META_GRAB_OP_RESIZING_W:
        case META_GRAB_OP_RESIZING_E:
          in_grab_op = TRUE;
          break;
        default:
          break;
        }
    }
  
  /* it's essential to use only the explicitly-set fields,
   * and otherwise use our current up-to-date position.
   *
   * Otherwise you get spurious position changes when the app changes
   * size, for example, if window->rect is not in sync with the
   * server-side position in effect when the configure request was
   * generated.
   */

  meta_window_get_gravity_position (window, &x, &y);

  only_resize = TRUE;

  allow_position_change = FALSE;
  
  if (meta_prefs_get_disable_workarounds ())
    {
      if (window->type == META_WINDOW_DIALOG ||
          window->type == META_WINDOW_MODAL_DIALOG ||
          window->type == META_WINDOW_SPLASHSCREEN)
        ; /* No position change for these */
      else if ((window->size_hints.flags & PPosition) ||
               /* USPosition is just stale if window is placed;
                * no --geometry involved here.
                */
               ((window->size_hints.flags & USPosition) &&
                !window->placed))
        allow_position_change = TRUE;
    }
  else
    {
      allow_position_change = TRUE;
    }

  if (in_grab_op)
    allow_position_change = FALSE;
  
  if (allow_position_change)
    {
      if (event->xconfigurerequest.value_mask & CWX)
        x = event->xconfigurerequest.x;
      
      if (event->xconfigurerequest.value_mask & CWY)
        y = event->xconfigurerequest.y;

      if (event->xconfigurerequest.value_mask & (CWX | CWY))
        {
          only_resize = FALSE;

          /* Once manually positioned, windows shouldn't be placed
           * by the window manager.
           */
          window->placed = TRUE;
        }
    }
  else
    {
      meta_topic (META_DEBUG_GEOMETRY,
		  "Not allowing position change for window %s PPosition 0x%lx USPosition 0x%lx type %u\n", 
		  window->desc, window->size_hints.flags & PPosition, 
		  window->size_hints.flags & USPosition,
		  window->type);     
    }

  width = window->rect.width;
  height = window->rect.height;

  if (!in_grab_op)
    {
      if (event->xconfigurerequest.value_mask & CWWidth)
        width = event->xconfigurerequest.width;
      
      if (event->xconfigurerequest.value_mask & CWHeight)
        height = event->xconfigurerequest.height;
    }

  /* ICCCM 4.1.5 */
  
  /* Note that x, y is the corner of the window border,
   * and width, height is the size of the window inside
   * its border, but that we always deny border requests
   * and give windows a border of 0. But we save the
   * requested border here.
   */
  if (event->xconfigurerequest.value_mask & CWBorderWidth)
    window->border_width = event->xconfigurerequest.border_width;

  /* We're ignoring the value_mask here, since sizes
   * not in the mask will be the current window geometry.
   */
  
  window->size_hints.x = x;
  window->size_hints.y = y;
  window->size_hints.width = width;
  window->size_hints.height = height;

  /* FIXME passing the gravity on only_resize thing is kind of crack-rock.
   * Basically I now have several ways of handling gravity, and things
   * don't make too much sense. I think I am doing the math in a couple
   * places and could do it in only one function, and remove some of the
   * move_resize_internal arguments.
   *
   * UPDATE (2005-09-17): See the huge comment at the beginning of
   * meta_window_move_resize_internal() which explains why the current
   * setup requires the only_resize thing.  Yeah, it'd be much better to
   * have a different setup for meta_window_move_resize_internal()...
   */
  
  flags = META_IS_CONFIGURE_REQUEST;
  if (event->xconfigurerequest.value_mask & (CWX | CWY))
    flags |= META_IS_MOVE_ACTION;
  if (event->xconfigurerequest.value_mask & (CWWidth | CWHeight))
    flags |= META_IS_RESIZE_ACTION;

  if (flags & (META_IS_MOVE_ACTION | META_IS_RESIZE_ACTION))
    meta_window_move_resize_internal (window, 
                                      flags,
                                      window->size_hints.win_gravity,
                                      window->size_hints.x,
                                      window->size_hints.y,
                                      window->size_hints.width,
                                      window->size_hints.height);

  /* Handle stacking. We only handle raises/lowers, mostly because
   * stack.c really can't deal with anything else.  I guess we'll fix
   * that if a client turns up that really requires it. Only a very
   * few clients even require the raise/lower (and in fact all client
   * attempts to deal with stacking order are essentially broken,
   * since they have no idea what other clients are involved or how
   * the stack looks).
   *
   * I'm pretty sure no interesting client uses TopIf, BottomIf, or
   * Opposite anyway, so the only possible missing thing is
   * Above/Below with a sibling set. For now we just pretend there's
   * never a sibling set and always do the full raise/lower instead of
   * the raise-just-above/below-sibling.
   */
  if (event->xconfigurerequest.value_mask & CWStackMode)
    {
      MetaWindow *active_window;
      active_window = window->display->expected_focus_window;
      if (meta_prefs_get_disable_workarounds () ||
          !meta_prefs_get_raise_on_click ())
        {
          meta_topic (META_DEBUG_STACK,
                      "%s sent an xconfigure stacking request; this is "
                      "broken behavior and the request is being ignored.\n",
                      window->desc);
        }
      else if (active_window &&
               !meta_window_same_application (window, active_window) &&
               XSERVER_TIME_IS_BEFORE (window->net_wm_user_time,
                                       active_window->net_wm_user_time))
        {
          meta_topic (META_DEBUG_STACK,
                      "Ignoring xconfigure stacking request from %s (with "
                      "user_time %u); currently active application is %s (with "
                      "user_time %u).\n",
                      window->desc,
                      window->net_wm_user_time,
                      active_window->desc,
                      active_window->net_wm_user_time);
          if (event->xconfigurerequest.detail == Above)
            meta_window_set_demands_attention(window);
        }
      else
        {
          switch (event->xconfigurerequest.detail)
            {
            case Above:
              meta_window_raise (window);
              break;
            case Below:
              meta_window_lower (window);
              break;
            case TopIf:
            case BottomIf:
            case Opposite:
              break;
            }
        }
    }      
  
  return TRUE;
}

gboolean
meta_window_property_notify (MetaWindow *window,
                             XEvent     *event)
{
  return process_property_notify (window, &event->xproperty);  
}

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10

gboolean
meta_window_client_message (MetaWindow *window,
                            XEvent     *event)
{
  MetaDisplay *display;

  display = window->display;

  if (event->xclient.message_type ==
      display->atom_net_close_window)
    {
      guint32 timestamp;

      if (event->xclient.data.l[0] != 0)
	timestamp = event->xclient.data.l[0];
      else
        {
          meta_warning ("Receiving a NET_CLOSE_WINDOW message for %s without "
                        "a timestamp!  This means some buggy (outdated) "
                        "application is on the loose!\n",
                        window->desc);
          timestamp = meta_display_get_current_time (window->display);
        }
      
      meta_window_delete (window, timestamp);

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_wm_desktop)
    {
      int space;
      MetaWorkspace *workspace;
              
      space = event->xclient.data.l[0];
              
      meta_verbose ("Request to move %s to workspace %d\n",
                    window->desc, space);

      workspace =
        meta_screen_get_workspace_by_index (window->screen,
                                            space);

      if (workspace)
        {
          if (window->on_all_workspaces)
            meta_window_unstick (window);
          meta_window_change_workspace (window, workspace);
        }
      else if (space == (int) 0xFFFFFFFF)
        {
          meta_window_stick (window);
        }
      else
        {
          meta_verbose ("No such workspace %d for screen\n", space);
        }

      meta_verbose ("Window %s now on_all_workspaces = %d\n",
                    window->desc, window->on_all_workspaces);
      
      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_wm_state)
    {
      gulong action;
      Atom first;
      Atom second;

      action = event->xclient.data.l[0];
      first = event->xclient.data.l[1];
      second = event->xclient.data.l[2];
      
      if (meta_is_verbose ())
        {
          char *str1;
          char *str2;

          meta_error_trap_push_with_return (display);
          str1 = XGetAtomName (display->xdisplay, first);
          if (meta_error_trap_pop_with_return (display, TRUE) != Success)
            str1 = NULL;

          meta_error_trap_push_with_return (display);
          str2 = XGetAtomName (display->xdisplay, second); 
          if (meta_error_trap_pop_with_return (display, TRUE) != Success)
            str2 = NULL;
          
          meta_verbose ("Request to change _NET_WM_STATE action %lu atom1: %s atom2: %s\n",
                        action,
                        str1 ? str1 : "(unknown)",
                        str2 ? str2 : "(unknown)");

          meta_XFree (str1);
          meta_XFree (str2);
        }

      if (first == display->atom_net_wm_state_shaded ||
          second == display->atom_net_wm_state_shaded)
        {
          gboolean shade;
          guint32 timestamp;

          /* Stupid protocol has no timestamp; of course, shading
           * sucks anyway so who really cares that we're forced to do
           * a roundtrip here? 
           */
          timestamp = meta_display_get_current_time_roundtrip (window->display);

          shade = (action == _NET_WM_STATE_ADD ||
                   (action == _NET_WM_STATE_TOGGLE && !window->shaded));
          if (shade && window->has_shade_func)
            meta_window_shade (window, timestamp);
          else
            meta_window_unshade (window, timestamp);
        }

      if (first == display->atom_net_wm_state_fullscreen ||
          second == display->atom_net_wm_state_fullscreen)
        {
          gboolean make_fullscreen;

          make_fullscreen = (action == _NET_WM_STATE_ADD ||
                             (action == _NET_WM_STATE_TOGGLE && !window->fullscreen));
          if (make_fullscreen && window->has_fullscreen_func)
            meta_window_make_fullscreen (window);
          else
            meta_window_unmake_fullscreen (window);
        }
      
      if (first == display->atom_net_wm_state_maximized_horz ||
          second == display->atom_net_wm_state_maximized_horz)
        {
          gboolean max;

          max = (action == _NET_WM_STATE_ADD ||
                 (action == _NET_WM_STATE_TOGGLE && 
                  !window->maximized_horizontally));
          if (max && window->has_maximize_func)
            {
              if (meta_prefs_get_raise_on_click ())
                meta_window_raise (window);
              meta_window_maximize (window, META_MAXIMIZE_HORIZONTAL);
            }
          else
            {
              if (meta_prefs_get_raise_on_click ())
                meta_window_raise (window);
              meta_window_unmaximize (window, META_MAXIMIZE_HORIZONTAL);
            }
        }

      if (first == display->atom_net_wm_state_maximized_vert ||
          second == display->atom_net_wm_state_maximized_vert)
        {
          gboolean max;

          max = (action == _NET_WM_STATE_ADD ||
                 (action == _NET_WM_STATE_TOGGLE && 
                  !window->maximized_vertically));
          if (max && window->has_maximize_func)
            {
              if (meta_prefs_get_raise_on_click ())
                meta_window_raise (window);
              meta_window_maximize (window, META_MAXIMIZE_VERTICAL);
            }
          else
            {
              if (meta_prefs_get_raise_on_click ())
                meta_window_raise (window);
              meta_window_unmaximize (window, META_MAXIMIZE_VERTICAL);
            }
        }

      if (first == display->atom_net_wm_state_modal ||
          second == display->atom_net_wm_state_modal)
        {
          window->wm_state_modal =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_modal);
          
          recalc_window_type (window);
          meta_window_queue_move_resize (window);
        }

      if (first == display->atom_net_wm_state_skip_pager ||
          second == display->atom_net_wm_state_skip_pager)
        {
          window->wm_state_skip_pager = 
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->skip_pager);

          recalc_window_features (window);
          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_skip_taskbar ||
          second == display->atom_net_wm_state_skip_taskbar)
        {
          window->wm_state_skip_taskbar =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->skip_taskbar);

          recalc_window_features (window);
          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_above ||
          second == display->atom_net_wm_state_above)
        {
          window->wm_state_above = 
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_above);

          meta_window_update_layer (window);
          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_below ||
          second == display->atom_net_wm_state_below)
        {
          window->wm_state_below = 
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_below);

          meta_window_update_layer (window);
          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_demands_attention ||
          second == display->atom_net_wm_state_demands_attention)
        {
          if ((action == _NET_WM_STATE_ADD) ||
              (action == _NET_WM_STATE_TOGGLE && !window->wm_state_demands_attention))
            meta_window_set_demands_attention(window);
          else
            meta_window_unset_demands_attention(window);
        }
      
      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_wm_change_state)
    {
      meta_verbose ("WM_CHANGE_STATE client message, state: %ld\n",
                    event->xclient.data.l[0]);
      if (event->xclient.data.l[0] == IconicState &&
          window->has_minimize_func)
        meta_window_minimize (window);

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_wm_moveresize)
    {
      int x_root;
      int y_root;
      int action;
      MetaGrabOp op;
      int button;
      guint32 timestamp;
      
      x_root = event->xclient.data.l[0];
      y_root = event->xclient.data.l[1];
      action = event->xclient.data.l[2];
      button = event->xclient.data.l[3];

      /* FIXME: What a braindead protocol; no timestamp?!? */
      timestamp = meta_display_get_current_time_roundtrip (display);
      meta_warning ("Received a _NET_WM_MOVERESIZE message for %s; these "
                    "messages lack timestamps and therefore suck.\n",
                    window->desc);
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Received _NET_WM_MOVERESIZE message on %s, %d,%d action = %d, button %d\n",
                  window->desc,
                  x_root, y_root, action, button);
      
      op = META_GRAB_OP_NONE;
      switch (action)
        {
        case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
          op = META_GRAB_OP_RESIZING_NW;
          break;
        case _NET_WM_MOVERESIZE_SIZE_TOP:
          op = META_GRAB_OP_RESIZING_N;
          break;
        case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
          op = META_GRAB_OP_RESIZING_NE;
          break;
        case _NET_WM_MOVERESIZE_SIZE_RIGHT:
          op = META_GRAB_OP_RESIZING_E;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
          op = META_GRAB_OP_RESIZING_SE;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOM:          
          op = META_GRAB_OP_RESIZING_S;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
          op = META_GRAB_OP_RESIZING_SW;
          break;
        case _NET_WM_MOVERESIZE_SIZE_LEFT:
          op = META_GRAB_OP_RESIZING_W;
          break;
        case _NET_WM_MOVERESIZE_MOVE:
          op = META_GRAB_OP_MOVING;
          break;
        case _NET_WM_MOVERESIZE_SIZE_KEYBOARD:
          op = META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN;
          break;
        case _NET_WM_MOVERESIZE_MOVE_KEYBOARD:
          op = META_GRAB_OP_KEYBOARD_MOVING;
          break;
        default:
          break;
        }

      if (op != META_GRAB_OP_NONE &&
          ((window->has_move_func && op == META_GRAB_OP_KEYBOARD_MOVING) ||
           (window->has_resize_func && op == META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN)))
        {
          meta_window_begin_grab_op (window, op, timestamp);
        }
      else if (op != META_GRAB_OP_NONE &&
               ((window->has_move_func && op == META_GRAB_OP_MOVING) ||
               (window->has_resize_func && 
                (op != META_GRAB_OP_MOVING && 
                 op != META_GRAB_OP_KEYBOARD_MOVING))))
        {
          /*
           * the button SHOULD already be included in the message
           */
          if (button == 0)
            {
              int x, y, query_root_x, query_root_y;
              Window root, child;
              guint mask;

              /* The race conditions in this _NET_WM_MOVERESIZE thing
               * are mind-boggling
               */
              mask = 0;
              meta_error_trap_push (window->display);
              XQueryPointer (window->display->xdisplay,
                             window->xwindow,
                             &root, &child,
                             &query_root_x, &query_root_y,
                             &x, &y,
                             &mask);
              meta_error_trap_pop (window->display, TRUE);

              if (mask & Button1Mask)
                button = 1;
              else if (mask & Button2Mask)
                button = 2;
              else if (mask & Button3Mask)
                button = 3;
              else
                button = 0;
            }

          if (button != 0)
            {
              meta_topic (META_DEBUG_WINDOW_OPS,
                          "Beginning move/resize with button = %d\n", button);
              meta_display_begin_grab_op (window->display,
                                          window->screen,
                                          window,
                                          op,
                                          FALSE, 0 /* event_serial */,
                                          button, 0,
                                          timestamp,
                                          x_root,
                                          y_root);
            }
        }

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_active_window)
    {
      MetaClientType source_indication;
      guint32        timestamp;

      meta_verbose ("_NET_ACTIVE_WINDOW request for window '%s', activating\n",
                    window->desc);

      source_indication = event->xclient.data.l[0];
      timestamp = event->xclient.data.l[1];

      if (source_indication > META_CLIENT_TYPE_MAX_RECOGNIZED)
        source_indication = META_CLIENT_TYPE_UNKNOWN;

      if (timestamp == 0)
        {
          /* Client using older EWMH _NET_ACTIVE_WINDOW without a timestamp */
          meta_warning ("Buggy client sent a _NET_ACTIVE_WINDOW message with a "
                        "timestamp of 0 for %s\n",
                        window->desc);
          timestamp = meta_display_get_current_time (display);
        }

      window_activate (window, timestamp, source_indication, NULL);
      return TRUE;
    }
  
  return FALSE;
}

gboolean
meta_window_notify_focus (MetaWindow *window,
                          XEvent     *event)
{
  /* note the event can be on either the window or the frame,
   * we focus the frame for shaded windows
   */
  
  /* The event can be FocusIn, FocusOut, or UnmapNotify.
   * On UnmapNotify we have to pretend it's focus out,
   * because we won't get a focus out if it occurs, apparently.
   */

  /* We ignore grabs, though this is questionable.
   * It may be better to increase the intelligence of
   * the focus window tracking.
   *
   * The problem is that keybindings for windows are done with
   * XGrabKey, which means focus_window disappears and the front of
   * the MRU list gets confused from what the user expects once a
   * keybinding is used.
   */  
  meta_topic (META_DEBUG_FOCUS,
              "Focus %s event received on %s 0x%lx (%s) "
              "mode %s detail %s\n",
              event->type == FocusIn ? "in" :
              event->type == FocusOut ? "out" :
              event->type == UnmapNotify ? "unmap" :
              "???",
              window->desc, event->xany.window,
              event->xany.window == window->xwindow ?
              "client window" :
              (window->frame && event->xany.window == window->frame->xwindow) ?
              "frame window" :
              "unknown window",
              event->type != UnmapNotify ?
              meta_event_mode_to_string (event->xfocus.mode) : "n/a",
              event->type != UnmapNotify ?
              meta_event_detail_to_string (event->xfocus.detail) : "n/a");

  /* FIXME our pointer tracking is broken; see how
   * gtk+/gdk/x11/gdkevents-x11.c or XFree86/xc/programs/xterm/misc.c
   * handle it for the correct way.  In brief you need to track
   * pointer focus and regular focus, and handle EnterNotify in
   * PointerRoot mode with no window manager.  However as noted above,
   * accurate focus tracking will break things because we want to keep
   * windows "focused" when using keybindings on them, and also we
   * sometimes "focus" a window by focusing its frame or
   * no_focus_window; so this all needs rethinking massively.
   *
   * My suggestion is to change it so that we clearly separate
   * actual keyboard focus tracking using the xterm algorithm,
   * and metacity's "pretend" focus window, and go through all
   * the code and decide which one should be used in each place;
   * a hard bit is deciding on a policy for that.
   *
   * http://bugzilla.gnome.org/show_bug.cgi?id=90382
   */
  
  if ((event->type == FocusIn ||
       event->type == FocusOut) &&
      (event->xfocus.mode == NotifyGrab ||
       event->xfocus.mode == NotifyUngrab ||
       /* From WindowMaker, ignore all funky pointer root events */
       event->xfocus.detail > NotifyNonlinearVirtual))
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Ignoring focus event generated by a grab or other weirdness\n");
      return TRUE;
    }
    
  if (event->type == FocusIn)
    {
      if (window != window->display->focus_window)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "* Focus --> %s\n", window->desc);
          window->display->focus_window = window;
          window->has_focus = TRUE;

          /* Move to the front of the focusing workspace's MRU list.
           * We should only be "removing" it from the MRU list if it's
           * not already there.  Note that it's possible that we might
           * be processing this FocusIn after we've changed to a
           * different workspace; we should therefore update the MRU
           * list only if the window is actually on the active
           * workspace.
           */
          if (window->screen->active_workspace &&
              meta_window_located_on_workspace (window, 
                                                window->screen->active_workspace))
            {
              GList* link;
              link = g_list_find (window->screen->active_workspace->mru_list, 
                                  window);
              g_assert (link);

              window->screen->active_workspace->mru_list = 
                g_list_remove_link (window->screen->active_workspace->mru_list,
                                    link);
              g_list_free (link);

              window->screen->active_workspace->mru_list = 
                g_list_prepend (window->screen->active_workspace->mru_list, 
                                window);
            }

          if (window->frame)
            meta_frame_queue_draw (window->frame);
          
          meta_error_trap_push (window->display);
          XInstallColormap (window->display->xdisplay,
                            window->colormap);
          meta_error_trap_pop (window->display, FALSE);

          /* move into FOCUSED_WINDOW layer */
          meta_window_update_layer (window);

          /* Ungrab click to focus button since the sync grab can interfere
           * with some things you might do inside the focused window, by
           * causing the client to get funky enter/leave events.
           */
          if (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK)
            meta_display_ungrab_focus_window_button (window->display, window);
        }
    }
  else if (event->type == FocusOut ||
           event->type == UnmapNotify)
    {
      if (event->type == FocusOut &&
          event->xfocus.detail == NotifyInferior)
        {
          /* This event means the client moved focus to a subwindow */
          meta_topic (META_DEBUG_FOCUS,
                      "Ignoring focus out on %s with NotifyInferior\n",
                      window->desc);
          return TRUE;
        }
      
      if (window == window->display->focus_window)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "%s is now the previous focus window due to being focused out or unmapped\n",
                      window->desc);

          meta_topic (META_DEBUG_FOCUS,
                      "* Focus --> NULL (was %s)\n", window->desc);
          
          window->display->focus_window = NULL;
          window->has_focus = FALSE;
          if (window->frame)
            meta_frame_queue_draw (window->frame);

          meta_error_trap_push (window->display);
          XUninstallColormap (window->display->xdisplay,
                              window->colormap);
          meta_error_trap_pop (window->display, FALSE);

          /* move out of FOCUSED_WINDOW layer */
          meta_window_update_layer (window);

          /* Re-grab for click to focus, if necessary */
          if (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK)
            meta_display_grab_focus_window_button (window->display, window);
       }
    }

  /* Now set _NET_ACTIVE_WINDOW hint */
  meta_display_update_active_window_hint (window->display);
  
  return FALSE;
}

static gboolean
process_property_notify (MetaWindow     *window,
                         XPropertyEvent *event)
{
  /* First, property notifies to ignore because we shouldn't honor
   * new values
   */
  if (event->atom == window->display->atom_net_wm_state)
    {
      meta_verbose ("Property notify on %s for _NET_WM_STATE, ignoring (we should be the one who set the property in the first place)\n",
                    window->desc);
      return TRUE;
    }

  /* Second, property notifies we want to use.
   * FIXME once we move entirely to the window-props.h framework, we
   * can just call reload on the property in the event and get rid of
   * this if-else chain.
   */
  
  if (event->atom == XA_WM_NAME)
    {
      meta_verbose ("Property notify on %s for WM_NAME\n", window->desc);

      /* don't bother reloading WM_NAME if using _NET_WM_NAME already */
      if (!window->using_net_wm_name)
        meta_window_reload_property (window, XA_WM_NAME);
    }
  else if (event->atom == window->display->atom_net_wm_name)
    {
      meta_verbose ("Property notify on %s for NET_WM_NAME\n", window->desc);
      meta_window_reload_property (window, window->display->atom_net_wm_name);
      
      /* if _NET_WM_NAME was unset, reload WM_NAME */
      if (!window->using_net_wm_name)
        meta_window_reload_property (window, XA_WM_NAME);      
    }
  else if (event->atom == XA_WM_ICON_NAME)
    {
      meta_verbose ("Property notify on %s for WM_ICON_NAME\n", window->desc);

      /* don't bother reloading WM_ICON_NAME if using _NET_WM_ICON_NAME already */
      if (!window->using_net_wm_icon_name)
        meta_window_reload_property (window, XA_WM_ICON_NAME);
    }
  else if (event->atom == window->display->atom_net_wm_icon_name)
    {
      meta_verbose ("Property notify on %s for NET_WM_ICON_NAME\n", window->desc);
      meta_window_reload_property (window, window->display->atom_net_wm_icon_name);
      
      /* if _NET_WM_ICON_NAME was unset, reload WM_ICON_NAME */
      if (!window->using_net_wm_icon_name)
        meta_window_reload_property (window, XA_WM_ICON_NAME);
    }  
  else if (event->atom == XA_WM_NORMAL_HINTS)
    {
      meta_verbose ("Property notify on %s for WM_NORMAL_HINTS\n", window->desc);
      
      meta_window_reload_property (window, XA_WM_NORMAL_HINTS);
      
      /* See if we need to constrain current size */
      meta_window_queue_move_resize (window);
    }
  else if (event->atom == window->display->atom_wm_protocols)
    {
      meta_verbose ("Property notify on %s for WM_PROTOCOLS\n", window->desc);
      
      meta_window_reload_property (window, window->display->atom_wm_protocols);
    }
  else if (event->atom == XA_WM_HINTS)
    {
      meta_verbose ("Property notify on %s for WM_HINTS\n", window->desc);

      meta_window_reload_property (window, XA_WM_HINTS);
    }
  else if (event->atom == window->display->atom_motif_wm_hints)
    {
      meta_verbose ("Property notify on %s for MOTIF_WM_HINTS\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_motif_wm_hints);
    }
  else if (event->atom == XA_WM_CLASS)
    {
      meta_verbose ("Property notify on %s for WM_CLASS\n", window->desc);
      
      meta_window_reload_property (window, XA_WM_CLASS);
    }
  else if (event->atom == XA_WM_TRANSIENT_FOR)
    {
      meta_verbose ("Property notify on %s for WM_TRANSIENT_FOR\n", window->desc);
      
      meta_window_reload_property (window, XA_WM_TRANSIENT_FOR);
    }
  else if (event->atom ==
           window->display->atom_wm_window_role)
    {
      meta_verbose ("Property notify on %s for WM_WINDOW_ROLE\n", window->desc);
      
      update_role (window);
    }
  else if (event->atom ==
           window->display->atom_wm_client_leader ||
           event->atom ==
           window->display->atom_sm_client_id)
    {
      meta_warning ("Broken client! Window %s changed client leader window or SM client ID\n", window->desc);
    }
  else if (event->atom ==
           window->display->atom_net_wm_window_type)
    {
      meta_verbose ("Property notify on %s for NET_WM_WINDOW_TYPE\n", window->desc);
      update_net_wm_type (window);
    }
  else if (event->atom == window->display->atom_net_wm_icon)
    {
      meta_verbose ("Property notify on %s for NET_WM_ICON\n", window->desc);
      meta_icon_cache_property_changed (&window->icon_cache,
                                        window->display,
                                        event->atom);
      meta_window_queue_update_icon (window);
    }
  else if (event->atom == window->display->atom_kwm_win_icon)
    {
      meta_verbose ("Property notify on %s for KWM_WIN_ICON\n", window->desc);

      meta_icon_cache_property_changed (&window->icon_cache,
                                        window->display,
                                        event->atom);
      meta_window_queue_update_icon (window);
    }
  else if ((event->atom == window->display->atom_net_wm_strut) ||
	   (event->atom == window->display->atom_net_wm_strut_partial))
    {
      meta_verbose ("Property notify on %s for _NET_WM_STRUT\n", window->desc);
      meta_window_update_struts (window);
    }
  else if (event->atom == window->display->atom_net_startup_id)
    {
      meta_verbose ("Property notify on %s for _NET_STARTUP_ID\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_net_startup_id);
    }
  else if (event->atom == window->display->atom_net_wm_sync_request_counter)
    {
      meta_verbose ("Property notify on %s for _NET_WM_SYNC_REQUEST_COUNTER\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_net_wm_sync_request_counter);
    }
  else if (event->atom == window->display->atom_net_wm_user_time)
    {
      meta_verbose ("Property notify on %s for _NET_WM_USER_TIME\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_net_wm_user_time);
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

  meta_topic (META_DEBUG_GEOMETRY,
              "Sending synthetic configure notify to %s with x: %d y: %d w: %d h: %d\n",
              window->desc,
              event.xconfigure.x, event.xconfigure.y,
              event.xconfigure.width, event.xconfigure.height);
  
  meta_error_trap_push (window->display);
  XSendEvent (window->display->xdisplay,
              window->xwindow,
              False, StructureNotifyMask, &event);
  meta_error_trap_pop (window->display, FALSE);
}

gboolean
meta_window_get_icon_geometry (MetaWindow    *window,
                               MetaRectangle *rect)
{
  gulong *geometry = NULL;
  int nitems;

  if (meta_prop_get_cardinal_list (window->display,
                                   window->xwindow,
                                   window->display->atom_net_wm_icon_geometry,
                                   &geometry, &nitems))
    {
      if (nitems != 4)
        {
          meta_verbose ("_NET_WM_ICON_GEOMETRY on %s has %d values instead of 4\n",
                        window->desc, nitems);
          meta_XFree (geometry);
          return FALSE;
        }
  
      if (rect)
        {
          rect->x = geometry[0];
          rect->y = geometry[1];
          rect->width = geometry[2];
          rect->height = geometry[3];
        }

      meta_XFree (geometry);

      return TRUE;
    }

  return FALSE;
}

static Window
read_client_leader (MetaDisplay *display,
                    Window       xwindow)
{
  Window retval = None;
  
  meta_prop_get_window (display, xwindow,
                        display->atom_wm_client_leader,
                        &retval);

  return retval;
}

typedef struct
{
  Window leader;  
} ClientLeaderData;

static gboolean
find_client_leader_func (MetaWindow *ancestor,
                         void       *data)
{
  ClientLeaderData *d;

  d = data;

  d->leader = read_client_leader (ancestor->display,
                                  ancestor->xwindow);

  /* keep going if no client leader found */
  return d->leader == None;
}

static void
update_sm_hints (MetaWindow *window)
{
  Window leader;
  
  window->xclient_leader = None;
  window->sm_client_id = NULL;

  /* If not on the current window, we can get the client
   * leader from transient parents. If we find a client
   * leader, we read the SM_CLIENT_ID from it.
   */
  leader = read_client_leader (window->display, window->xwindow);
  if (leader == None)
    {
      ClientLeaderData d;
      d.leader = None;
      meta_window_foreach_ancestor (window, find_client_leader_func,
                                    &d);
      leader = d.leader;
    }
      
  if (leader != None)
    {
      char *str;
      
      window->xclient_leader = leader;

      if (meta_prop_get_latin1_string (window->display, leader,
                                       window->display->atom_sm_client_id,
                                       &str))
        {
          window->sm_client_id = g_strdup (str);
          meta_XFree (str);
        }
    }
  else
    {
      meta_verbose ("Didn't find a client leader for %s\n", window->desc);

      if (!meta_prefs_get_disable_workarounds ())
        {
          /* Some broken apps (kdelibs fault?) set SM_CLIENT_ID on the app
           * instead of the client leader
           */
          char *str;

          str = NULL;
          if (meta_prop_get_latin1_string (window->display, window->xwindow,
                                           window->display->atom_sm_client_id,
                                           &str))
            {
              if (window->sm_client_id == NULL) /* first time through */
                meta_warning (_("Window %s sets SM_CLIENT_ID on itself, instead of on the WM_CLIENT_LEADER window as specified in the ICCCM.\n"),
                              window->desc);
              
              window->sm_client_id = g_strdup (str);
              meta_XFree (str);
            }
        }
    }

  meta_verbose ("Window %s client leader: 0x%lx SM_CLIENT_ID: '%s'\n",
                window->desc, window->xclient_leader,
                window->sm_client_id ? window->sm_client_id : "none");
}

static void
update_role (MetaWindow *window)
{
  char *str;
  
  if (window->role)
    g_free (window->role);
  window->role = NULL;

  if (meta_prop_get_latin1_string (window->display, window->xwindow,
                                   window->display->atom_wm_window_role,
                                   &str))
    {
      window->role = g_strdup (str);
      meta_XFree (str);
    }

  meta_verbose ("Updated role of %s to '%s'\n",
                window->desc, window->role ? window->role : "null");
}

static void
update_net_wm_type (MetaWindow *window)
{
  int n_atoms;
  Atom *atoms;
  int i;

  window->type_atom = None;
  n_atoms = 0;
  atoms = NULL;
  
  meta_prop_get_atom_list (window->display, window->xwindow, 
                           window->display->atom_net_wm_window_type,
                           &atoms, &n_atoms);

  i = 0;
  while (i < n_atoms)
    {
      /* We break as soon as we find one we recognize,
       * supposed to prefer those near the front of the list
       */
      if (atoms[i] == window->display->atom_net_wm_window_type_desktop ||
          atoms[i] == window->display->atom_net_wm_window_type_dock ||
          atoms[i] == window->display->atom_net_wm_window_type_toolbar ||
          atoms[i] == window->display->atom_net_wm_window_type_menu ||
          atoms[i] == window->display->atom_net_wm_window_type_dialog ||
          atoms[i] == window->display->atom_net_wm_window_type_normal ||
          atoms[i] == window->display->atom_net_wm_window_type_utility ||
          atoms[i] == window->display->atom_net_wm_window_type_splash)
        {
          window->type_atom = atoms[i];
          break;
        }
      
      ++i;
    }
  
  meta_XFree (atoms);

  if (meta_is_verbose ())
    {
      char *str;

      str = NULL;
      if (window->type_atom != None)
        {
          meta_error_trap_push (window->display);
          str = XGetAtomName (window->display->xdisplay, window->type_atom);
          meta_error_trap_pop (window->display, TRUE);
        }

      meta_verbose ("Window %s type atom %s\n", window->desc,
                    str ? str : "(none)");

      if (str)
        meta_XFree (str);
    }
  
  recalc_window_type (window);
}

static void
redraw_icon (MetaWindow *window)
{
  /* We could probably be smart and just redraw the icon here,
   * instead of the whole frame.
   */
  if (window->frame && (window->mapped || window->frame->mapped))
    meta_ui_queue_frame_draw (window->screen->ui, window->frame->xwindow);
}

static void
meta_window_update_icon_now (MetaWindow *window)
{
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;

  icon = NULL;
  mini_icon = NULL;
  
  if (meta_read_icons (window->screen,
                       window->xwindow,
                       &window->icon_cache,
                       window->wm_hints_pixmap,
                       window->wm_hints_mask,
                       &icon,
                       META_ICON_WIDTH, META_ICON_HEIGHT,
                       &mini_icon,
                       META_MINI_ICON_WIDTH,
                       META_MINI_ICON_HEIGHT))
    {
      if (window->icon)
        g_object_unref (G_OBJECT (window->icon));
      
      if (window->mini_icon)
        g_object_unref (G_OBJECT (window->mini_icon));
      
      window->icon = icon;
      window->mini_icon = mini_icon;

      redraw_icon (window);
    }
  
  g_assert (window->icon);
  g_assert (window->mini_icon);
}

static guint update_icon_idle = 0;
static GSList *update_icon_pending = NULL;

static gboolean
idle_update_icon (gpointer data)
{
  GSList *tmp;
  GSList *copy;

  meta_topic (META_DEBUG_GEOMETRY, "Clearing the update_icon queue\n");

  /* Work with a copy, for reentrancy. The allowed reentrancy isn't
   * complete; destroying a window while we're in here would result in
   * badness. But it's OK to queue/unqueue update_icons.
   */
  copy = g_slist_copy (update_icon_pending);
  g_slist_free (update_icon_pending);
  update_icon_pending = NULL;
  update_icon_idle = 0;

  destroying_windows_disallowed += 1;
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      meta_window_update_icon_now (window); 
      window->update_icon_queued = FALSE;
      
      tmp = tmp->next;
    }

  g_slist_free (copy);

  destroying_windows_disallowed -= 1;
  
  return FALSE;
}

static void
meta_window_unqueue_update_icon (MetaWindow *window)
{
  if (!window->update_icon_queued)
    return;

  meta_topic (META_DEBUG_GEOMETRY,
              "Removing %s from the update_icon queue\n",
              window->desc);

  /* Note that window may not actually be in update_icon_pending
   * because it may have been in "copy" inside the idle handler
   */
  update_icon_pending = g_slist_remove (update_icon_pending, window);
  window->update_icon_queued = FALSE;
  
  if (update_icon_pending == NULL &&
      update_icon_idle != 0)
    {
      g_source_remove (update_icon_idle);
      update_icon_idle = 0;
    }
}

void
meta_window_queue_update_icon (MetaWindow *window)
{
  if (window->unmanaging)
    return;

  if (window->update_icon_queued)
    return;

  meta_topic (META_DEBUG_GEOMETRY,
              "Putting %s in the update_icon queue\n",
              window->desc);
  
  window->update_icon_queued = TRUE;
  
  if (update_icon_idle == 0)
    update_icon_idle = g_idle_add (idle_update_icon, NULL);
  
  update_icon_pending = g_slist_prepend (update_icon_pending, window);
}

GList*
meta_window_get_workspaces (MetaWindow *window)
{
  if (window->on_all_workspaces)
    return window->screen->workspaces;
  else
    return window->workspace->list_containing_self;
}

static void
invalidate_work_areas (MetaWindow *window)
{
  GList *tmp;

  tmp = meta_window_get_workspaces (window);
  
  while (tmp != NULL)
    {
      meta_workspace_invalidate_work_area (tmp->data);
      tmp = tmp->next;
    }
}

void
meta_window_update_struts (MetaWindow *window)
{
  gulong *struts = NULL;
  int nitems;
  gboolean old_has_struts;
  gboolean new_has_struts;

  MetaRectangle old_left;
  MetaRectangle old_right;
  MetaRectangle old_top;
  MetaRectangle old_bottom;

  MetaRectangle new_left;
  MetaRectangle new_right;
  MetaRectangle new_top;
  MetaRectangle new_bottom;

  meta_verbose ("Updating struts for %s\n", window->desc);
  
  if (window->struts)
    {
      old_has_struts = TRUE;
      old_left = window->struts->left;
      old_right = window->struts->right;
      old_top = window->struts->top;
      old_bottom = window->struts->bottom;
    }
  else
    {
      old_has_struts = FALSE;
    }

  new_has_struts = FALSE;
  new_left = window->screen->rect;
  new_left.width = 0;

  new_right = window->screen->rect;
  new_right.width = 0;
  new_right.x = window->screen->rect.width;

  new_top = window->screen->rect;
  new_top.height = 0;

  new_bottom = window->screen->rect;
  new_bottom.height = 0;
  new_bottom.y = window->screen->rect.height;
  
  if (meta_prop_get_cardinal_list (window->display,
                                   window->xwindow,
                                   window->display->atom_net_wm_strut_partial,
                                   &struts, &nitems))
    {
      if (nitems != 12)
        {
          meta_verbose ("_NET_WM_STRUT_PARTIAL on %s has %d values instead of 12\n",
                        window->desc, nitems);
        }
      else
        {
          new_has_struts = TRUE;
          new_left.width = (int) struts[0];
          new_right.width = (int) struts[1];
          new_top.height = (int)struts[2];
          new_bottom.height = (int)struts[3];
          new_right.x  = window->screen->rect.width  - new_right.width;
          new_bottom.y = window->screen->rect.height - new_bottom.height;
          new_left.y = struts[4];
          new_left.height = struts[5] - new_left.y + 1;
          new_right.y = struts[6];
          new_right.height = struts[7] - new_right.y + 1;
          new_top.x = struts[8];
          new_top.width = struts[9] - new_top.x + 1;
          new_bottom.x = struts[10];
          new_bottom.width = struts[11] - new_bottom.x + 1;
          
          meta_verbose ("_NET_WM_STRUT_PARTIAL struts %d %d %d %d for window %s\n",
                        new_left.width,
                        new_right.width,
                        new_top.height,
                        new_bottom.height,
                        window->desc);
          
        }
      meta_XFree (struts);
    }
  else
    {
      meta_verbose ("No _NET_WM_STRUT property for %s\n",
                    window->desc);
    }

  if (!new_has_struts)
    {
      if (meta_prop_get_cardinal_list (window->display,
                                       window->xwindow,
                                       window->display->atom_net_wm_strut,
                                       &struts, &nitems))
        {
          if (nitems != 4)
            {
              meta_verbose ("_NET_WM_STRUT on %s has %d values instead of 4\n",
                            window->desc, nitems);
            }
          else
            {
              new_has_struts = TRUE;
              new_left.width = (int) struts[0];
              new_right.width = (int) struts[1];
              new_top.height = (int)struts[2];
              new_bottom.height = (int)struts[3];
              new_right.x  = window->screen->rect.width  - new_right.width;
              new_bottom.y = window->screen->rect.height - new_bottom.height;
              
              meta_verbose ("_NET_WM_STRUT struts %d %d %d %d for window %s\n",
                            new_left.width,
                            new_right.width,
                            new_top.height,
                            new_bottom.height,
                            window->desc);
              
            }
          meta_XFree (struts);
        }
      else
        {
          meta_verbose ("No _NET_WM_STRUT property for %s\n",
                        window->desc);
        }
    }
 
  if (old_has_struts != new_has_struts ||
      (new_has_struts && old_has_struts &&
       (!meta_rectangle_equal(&old_left, &new_left) ||
        !meta_rectangle_equal(&old_right, &new_right) ||
        !meta_rectangle_equal(&old_top, &new_top) ||
        !meta_rectangle_equal(&old_bottom, &new_bottom))))
    {
      if (new_has_struts)
        {
          if (!window->struts)
            window->struts = g_new (MetaStruts, 1);
              
          window->struts->left = new_left;
          window->struts->right = new_right;
          window->struts->top = new_top;
          window->struts->bottom = new_bottom;
        }
      else
        {
          g_free (window->struts);
          window->struts = NULL;
        }
      meta_topic (META_DEBUG_WORKAREA,
                  "Invalidating work areas of window %s due to struts update\n",
                  window->desc);
      invalidate_work_areas (window);
    }
  else
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Struts on %s were unchanged\n", window->desc);
    }
}

void
meta_window_recalc_window_type (MetaWindow *window)
{
  recalc_window_type (window);
}

static void
recalc_window_type (MetaWindow *window)
{
  MetaWindowType old_type;

  old_type = window->type;
  
  if (window->type_atom != None)
    {
      if (window->type_atom  == window->display->atom_net_wm_window_type_desktop)
        window->type = META_WINDOW_DESKTOP;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_dock)
        window->type = META_WINDOW_DOCK;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_toolbar)
        window->type = META_WINDOW_TOOLBAR;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_menu)
        window->type = META_WINDOW_MENU;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_dialog)
        window->type = META_WINDOW_DIALOG;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_normal)
        window->type = META_WINDOW_NORMAL;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_utility)
        window->type = META_WINDOW_UTILITY;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_splash)
        window->type = META_WINDOW_SPLASHSCREEN;
      else
        meta_bug ("Set a type atom for %s that wasn't handled in recalc_window_type\n",
                  window->desc);
    }
  else if (window->xtransient_for != None)
    {
      window->type = META_WINDOW_DIALOG;
    }
  else
    {
      window->type = META_WINDOW_NORMAL;
    }

  if (window->type == META_WINDOW_DIALOG &&
      window->wm_state_modal)
    window->type = META_WINDOW_MODAL_DIALOG;
  
  meta_verbose ("Calculated type %u for %s, old type %u\n",
                window->type, window->desc, old_type);

  if (old_type != window->type)
    {
      recalc_window_features (window);
      
      set_net_wm_state (window);
      
      /* Update frame */
      if (window->decorated)
        meta_window_ensure_frame (window);
      else
        meta_window_destroy_frame (window);
      
      /* update stacking constraints */
      meta_window_update_layer (window);

      meta_window_grab_keys (window);
    }
}

static void
set_allowed_actions_hint (MetaWindow *window)
{
#define MAX_N_ACTIONS 10
  unsigned long data[MAX_N_ACTIONS];
  int i;

  i = 0;
  if (window->has_move_func)
    {
      data[i] = window->display->atom_net_wm_action_move;
      ++i;
    }
  if (window->has_resize_func)
    {
      data[i] = window->display->atom_net_wm_action_resize;
      ++i;
      data[i] = window->display->atom_net_wm_action_fullscreen;
      ++i;
    }
  if (window->has_minimize_func)
    {
      data[i] = window->display->atom_net_wm_action_minimize;
      ++i;
    }
  if (window->has_shade_func)
    {
      data[i] = window->display->atom_net_wm_action_shade;
      ++i;
    }
  /* sticky according to EWMH is different from metacity's sticky;
   * metacity doesn't support EWMH sticky
   */
  if (window->has_maximize_func)
    {
      data[i] = window->display->atom_net_wm_action_maximize_horz;
      ++i;
      data[i] = window->display->atom_net_wm_action_maximize_vert;
      ++i;
    }
  /* We always allow this */
  data[i] = window->display->atom_net_wm_action_change_desktop;
  ++i;
  if (window->has_close_func)
    {
      data[i] = window->display->atom_net_wm_action_close;
      ++i;
    }

  g_assert (i <= MAX_N_ACTIONS);

  meta_verbose ("Setting _NET_WM_ALLOWED_ACTIONS with %d atoms\n", i);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_allowed_actions,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) data, i);
  meta_error_trap_pop (window->display, FALSE);
#undef MAX_N_ACTIONS
}

void
meta_window_recalc_features (MetaWindow *window)
{
  recalc_window_features (window);
}

static void
recalc_window_features (MetaWindow *window)
{
  gboolean old_has_close_func;
  gboolean old_has_minimize_func;
  gboolean old_has_move_func;
  gboolean old_has_resize_func;
  gboolean old_has_shade_func;
  gboolean old_always_sticky;

  old_has_close_func = window->has_close_func;
  old_has_minimize_func = window->has_minimize_func;
  old_has_move_func = window->has_move_func;
  old_has_resize_func = window->has_resize_func;
  old_has_shade_func = window->has_shade_func;
  old_always_sticky = window->always_sticky;

  /* Use MWM hints initially */
  window->decorated = window->mwm_decorated;
  window->border_only = window->mwm_border_only;
  window->has_close_func = window->mwm_has_close_func;
  window->has_minimize_func = window->mwm_has_minimize_func;
  window->has_maximize_func = window->mwm_has_maximize_func;
  window->has_move_func = window->mwm_has_move_func;
    
  window->has_resize_func = TRUE;  

  /* If min_size == max_size, then don't allow resize */
  if (window->size_hints.min_width == window->size_hints.max_width &&
      window->size_hints.min_height == window->size_hints.max_height)
    window->has_resize_func = FALSE;
  else if (!window->mwm_has_resize_func)
    {
      /* We ignore mwm_has_resize_func because WM_NORMAL_HINTS is the
       * authoritative source for that info. Some apps such as mplayer or
       * xine disable resize via MWM but not WM_NORMAL_HINTS, but that
       * leads to e.g. us not fullscreening their windows.  Apps that set
       * MWM but not WM_NORMAL_HINTS are basically broken. We complain
       * about these apps but make them work.
       */
      
      meta_warning (_("Window %s sets an MWM hint indicating it isn't resizable, but sets min size %d x %d and max size %d x %d; this doesn't make much sense.\n"),
                    window->desc,
                    window->size_hints.min_width,
                    window->size_hints.min_height,
                    window->size_hints.max_width,
                    window->size_hints.max_height);
    }

  window->has_shade_func = TRUE;
  window->has_fullscreen_func = TRUE;

  window->always_sticky = FALSE;
  
  /* Semantic category overrides the MWM hints */
  if (window->type == META_WINDOW_TOOLBAR)
    window->decorated = FALSE;

  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK)
    window->always_sticky = TRUE;
  
  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK ||
      window->type == META_WINDOW_SPLASHSCREEN)
    {
      window->decorated = FALSE;
      window->has_close_func = FALSE;
      window->has_shade_func = FALSE;

      /* FIXME this keeps panels and things from using
       * NET_WM_MOVERESIZE; the problem is that some
       * panels (edge panels) have fixed possible locations,
       * and others ("floating panels") do not.
       *
       * Perhaps we should require edge panels to explicitly
       * disable movement?
       */
      window->has_move_func = FALSE;
      window->has_resize_func = FALSE;
    }
  
  if (window->type != META_WINDOW_NORMAL)
    {
      window->has_minimize_func = FALSE;
      window->has_maximize_func = FALSE;
      window->has_fullscreen_func = FALSE;
    }

  if (!window->has_resize_func)
    {      
      window->has_maximize_func = FALSE;
      
      /* don't allow fullscreen if we can't resize, unless the size
       * is entire screen size (kind of broken, because we
       * actually fullscreen to xinerama head size not screen size)
       */
      if (window->size_hints.min_width == window->screen->rect.width &&
          window->size_hints.min_height == window->screen->rect.height)
        ; /* leave fullscreen available */
      else
        window->has_fullscreen_func = FALSE;
    }

  /* We leave fullscreen windows decorated, just push the frame outside
   * the screen. This avoids flickering to unparent them.
   *
   * Note that setting has_resize_func = FALSE here must come after the
   * above code that may disable fullscreen, because if the window
   * is not resizable purely due to fullscreen, we don't want to
   * disable fullscreen mode.
   */
  if (window->fullscreen)
    {
      window->has_shade_func = FALSE;
      window->has_move_func = FALSE;
      window->has_resize_func = FALSE;
      window->has_maximize_func = FALSE;
    }

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Window %s fullscreen = %d not resizable, maximizable = %d fullscreenable = %d min size %dx%d max size %dx%d\n",
              window->desc,
              window->fullscreen,
              window->has_maximize_func, window->has_fullscreen_func,
              window->size_hints.min_width,
              window->size_hints.min_height,
              window->size_hints.max_width,
              window->size_hints.max_height);
  
  /* no shading if not decorated */
  if (!window->decorated || window->border_only)
    window->has_shade_func = FALSE;

  window->skip_taskbar = FALSE;
  window->skip_pager = FALSE;

  if (window->wm_state_skip_taskbar)
    window->skip_taskbar = TRUE;
  
  if (window->wm_state_skip_pager)
    window->skip_pager = TRUE;
  
  switch (window->type)
    {
      /* Force skip taskbar/pager on these window types */
    case META_WINDOW_DESKTOP:
    case META_WINDOW_DOCK:
    case META_WINDOW_TOOLBAR:
    case META_WINDOW_MENU:
    case META_WINDOW_UTILITY:
    case META_WINDOW_SPLASHSCREEN:
      window->skip_taskbar = TRUE;
      window->skip_pager = TRUE;
      break;

    case META_WINDOW_DIALOG:
    case META_WINDOW_MODAL_DIALOG:
      /* only skip taskbar if we have a real transient parent */
      if (window->xtransient_for != None &&
          window->xtransient_for != window->screen->xroot)
        window->skip_taskbar = TRUE;
      break;
      
    case META_WINDOW_NORMAL:
      break;
    }

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Window %s decorated = %d border_only = %d has_close = %d has_minimize = %d has_maximize = %d has_move = %d has_shade = %d skip_taskbar = %d skip_pager = %d\n",
              window->desc,
              window->decorated,
              window->border_only,
              window->has_close_func,
              window->has_minimize_func,
              window->has_maximize_func,
              window->has_move_func,
              window->has_shade_func,
              window->skip_taskbar,
              window->skip_pager);
  
  /* FIXME:
   * Lame workaround for recalc_window_features
   * being used overzealously. The fix is to
   * only recalc_window_features when something
   * has actually changed.
   */
  if (old_has_close_func != window->has_close_func       ||
      old_has_minimize_func != window->has_minimize_func ||
      old_has_move_func != window->has_move_func         ||
      old_has_resize_func != window->has_resize_func     ||
      old_has_shade_func != window->has_shade_func       ||
      old_always_sticky != window->always_sticky)
    set_allowed_actions_hint (window);
    
  /* FIXME perhaps should ensure if we don't have a shade func,
   * we aren't shaded, etc.
   */
}

static void
menu_callback (MetaWindowMenu *menu,
               Display        *xdisplay,
               Window          client_xwindow,
               guint32         timestamp,
               MetaMenuOp      op,
               int             workspace_index,
               gpointer        data)
{
  MetaDisplay *display;
  MetaWindow *window;
  MetaWorkspace *workspace;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, client_xwindow);
  workspace = NULL;
  
  if (window != NULL) /* window can be NULL */
    {
      meta_verbose ("Menu op %u on %s\n", op, window->desc);
      
      /* op can be 0 for none */
      switch (op)
        {
        case META_MENU_OP_DELETE:
          meta_window_delete (window, timestamp);
          break;

        case META_MENU_OP_MINIMIZE:
          meta_window_minimize (window);
          break;

        case META_MENU_OP_UNMAXIMIZE:
          meta_window_unmaximize (window,
                                  META_MAXIMIZE_HORIZONTAL |
                                  META_MAXIMIZE_VERTICAL);
          break;
      
        case META_MENU_OP_MAXIMIZE:
          meta_window_maximize (window,
                                META_MAXIMIZE_HORIZONTAL |
                                META_MAXIMIZE_VERTICAL);
          break;

        case META_MENU_OP_UNSHADE:
          meta_window_unshade (window, timestamp);
          break;
      
        case META_MENU_OP_SHADE:
          meta_window_shade (window, timestamp);
          break;
      
        case META_MENU_OP_MOVE_LEFT:
          workspace = meta_workspace_get_neighbor (window->screen->active_workspace,
                                                   META_MOTION_LEFT);
          break;

        case META_MENU_OP_MOVE_RIGHT:
          workspace = meta_workspace_get_neighbor (window->screen->active_workspace,
                                                   META_MOTION_RIGHT);
          break;

        case META_MENU_OP_MOVE_UP:
          workspace = meta_workspace_get_neighbor (window->screen->active_workspace,
                                                   META_MOTION_UP);
          break;

        case META_MENU_OP_MOVE_DOWN:
          workspace = meta_workspace_get_neighbor (window->screen->active_workspace,
                                                   META_MOTION_DOWN);
          break;

        case META_MENU_OP_WORKSPACES:
          workspace = meta_screen_get_workspace_by_index (window->screen,
                                                          workspace_index);
          break;

        case META_MENU_OP_STICK:
          meta_window_stick (window);
          break;

        case META_MENU_OP_UNSTICK:
          meta_window_unstick (window);
          break;

        case META_MENU_OP_ABOVE:
          meta_window_make_above (window);
          break;

        case META_MENU_OP_UNABOVE:
          meta_window_unmake_above (window);
          break;

        case META_MENU_OP_MOVE:
          meta_window_begin_grab_op (window,
                                     META_GRAB_OP_KEYBOARD_MOVING,
                                     timestamp);
          break;

        case META_MENU_OP_RESIZE:
          meta_window_begin_grab_op (window,
                                     META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN,
                                     timestamp);
          break;

        case META_MENU_OP_RECOVER:
          meta_window_shove_titlebar_onscreen (window);
          break;
          
        case 0:
          /* nothing */
          break;
          
        default:
          meta_warning (G_STRLOC": Unknown window op\n");
          break;
        }

      if (workspace)
	{
	  meta_window_change_workspace (window,
					workspace);
#if 0
	  meta_workspace_activate (workspace);
	  meta_window_raise (window);
#endif
	}
    }
  else
    {
      meta_verbose ("Menu callback on nonexistent window\n");
    }

  if (display->window_menu == menu)
    {
      display->window_menu = NULL;
      display->window_with_menu = NULL;
    }
  
  meta_ui_window_menu_free (menu);
}

void
meta_window_show_menu (MetaWindow *window,
                       int         root_x,
                       int         root_y,
                       int         button,
                       guint32     timestamp)
{
  MetaMenuOp ops;
  MetaMenuOp insensitive;
  MetaWindowMenu *menu;
  MetaWorkspaceLayout layout;
  int n_workspaces;
  
  if (window->display->window_menu)
    {
      meta_ui_window_menu_free (window->display->window_menu);
      window->display->window_menu = NULL;
      window->display->window_with_menu = NULL;
    }

  ops = 0;
  insensitive = 0;

  ops |= (META_MENU_OP_DELETE | META_MENU_OP_MINIMIZE | META_MENU_OP_MOVE | META_MENU_OP_RESIZE);

  if (!meta_window_titlebar_is_onscreen (window) &&
      window->type != META_WINDOW_DOCK &&
      window->type != META_WINDOW_DESKTOP)
    ops |= META_MENU_OP_RECOVER;

  n_workspaces = meta_screen_get_n_workspaces (window->screen);

  if (n_workspaces > 1)
    ops |= META_MENU_OP_WORKSPACES;

  meta_screen_calc_workspace_layout (window->screen,
                                     n_workspaces,
                                     meta_workspace_index ( window->screen->active_workspace),
                                     &layout);

  if (!window->on_all_workspaces)
    {

      if (layout.current_col > 0)
        ops |= META_MENU_OP_MOVE_LEFT;
      if ((layout.current_col < layout.cols - 1) &&
          (layout.current_row * layout.cols + (layout.current_col + 1) < n_workspaces))
        ops |= META_MENU_OP_MOVE_RIGHT;
      if (layout.current_row > 0)
        ops |= META_MENU_OP_MOVE_UP;
      if ((layout.current_row < layout.rows - 1) &&
          ((layout.current_row + 1) * layout.cols + layout.current_col < n_workspaces))
        ops |= META_MENU_OP_MOVE_DOWN;
    }

  meta_screen_free_workspace_layout (&layout);

  if (META_WINDOW_MAXIMIZED (window))
    ops |= META_MENU_OP_UNMAXIMIZE;
  else
    ops |= META_MENU_OP_MAXIMIZE;
  
#if 0
  if (window->shaded)
    ops |= META_MENU_OP_UNSHADE;
  else
    ops |= META_MENU_OP_SHADE;
#endif

  if (window->on_all_workspaces)
    ops |= META_MENU_OP_UNSTICK;
  else
    ops |= META_MENU_OP_STICK;

  if (window->wm_state_above)
    ops |= META_MENU_OP_UNABOVE;
  else
    ops |= META_MENU_OP_ABOVE;
  
  if (!window->has_maximize_func)
    insensitive |= META_MENU_OP_UNMAXIMIZE | META_MENU_OP_MAXIMIZE;
  
  if (!window->has_minimize_func)
    insensitive |= META_MENU_OP_MINIMIZE;
  
  if (!window->has_close_func)
    insensitive |= META_MENU_OP_DELETE;

  if (!window->has_shade_func)
    insensitive |= META_MENU_OP_SHADE | META_MENU_OP_UNSHADE;

  if (!META_WINDOW_ALLOWS_MOVE (window))
    insensitive |= META_MENU_OP_MOVE;

  if (!META_WINDOW_ALLOWS_RESIZE (window))
    insensitive |= META_MENU_OP_RESIZE;

   if (window->always_sticky)
     insensitive |= META_MENU_OP_UNSTICK | META_MENU_OP_WORKSPACES;

  if ((window->type == META_WINDOW_DESKTOP) ||
      (window->type == META_WINDOW_DOCK) ||
      (window->type == META_WINDOW_SPLASHSCREEN))
    insensitive |= META_MENU_OP_ABOVE | META_MENU_OP_UNABOVE;

  /* If all operations are disabled, just quit without showing the menu.
   * This is the case, for example, with META_WINDOW_DESKTOP windows.
   */
  if ((ops & ~insensitive) == 0)
    return;
  
  menu =
    meta_ui_window_menu_new (window->screen->ui,
                             window->xwindow,
                             ops,
                             insensitive,
                             meta_window_get_net_wm_desktop (window),
                             meta_screen_get_n_workspaces (window->screen),
                             menu_callback,
                             NULL); 

  window->display->window_menu = menu;
  window->display->window_with_menu = window;
  
  meta_verbose ("Popping up window menu for %s\n", window->desc);
  
  meta_ui_window_menu_popup (menu, root_x, root_y, button, timestamp);
}

void
meta_window_shove_titlebar_onscreen (MetaWindow *window)
{
  MetaRectangle  outer_rect;
  GList         *onscreen_region;
  int            horiz_amount, vert_amount;
  int            newx, newy;

  /* If there's no titlebar, don't bother */
  if (!window->frame)
    return;

  /* Get the basic info we need */
  meta_window_get_outer_rect (window, &outer_rect);
  onscreen_region = window->screen->active_workspace->screen_region;

  /* Extend the region (just in case the window is too big to fit on the
   * screen), then shove the window on screen, then return the region to
   * normal.
   */
  horiz_amount = outer_rect.width;
  vert_amount  = outer_rect.height;
  meta_rectangle_expand_region (onscreen_region,
                                horiz_amount,
                                horiz_amount, 
                                0,
                                vert_amount);
  meta_rectangle_shove_into_region(onscreen_region,
                                   FIXED_DIRECTION_X,
                                   &outer_rect);
  meta_rectangle_expand_region (onscreen_region,
                                -horiz_amount,
                                -horiz_amount, 
                                0,
                                -vert_amount);

  newx = outer_rect.x + window->frame->child_x;
  newy = outer_rect.y + window->frame->child_y;
  meta_window_move_resize (window,
                           FALSE,
                           newx,
                           newy,
                           window->rect.width, 
                           window->rect.height);
}

gboolean
meta_window_titlebar_is_onscreen (MetaWindow *window)
{
  MetaRectangle  titlebar_rect;
  GList         *onscreen_region;
  int            titlebar_size;
  gboolean       is_onscreen;

  const int min_height_needed  = 8;
  const int min_width_percent  = 0.5;
  const int min_width_absolute = 50;

  /* Titlebar can't be offscreen if there is no titlebar... */
  if (!window->frame)
    return FALSE;
  
  /* Get the rectangle corresponding to the titlebar */
  meta_window_get_outer_rect (window, &titlebar_rect);
  titlebar_rect.height = window->frame->child_y;
  titlebar_size = meta_rectangle_area (&titlebar_rect);

  /* Run through the spanning rectangles for the screen and see if one of
   * them overlaps with the titlebar sufficiently to consider it onscreen.
   */
  is_onscreen = FALSE;
  onscreen_region = window->screen->active_workspace->screen_region;
  while (onscreen_region)
    {
      MetaRectangle *spanning_rect = onscreen_region->data;
      MetaRectangle overlap;
      
      meta_rectangle_intersect (&titlebar_rect, spanning_rect, &overlap);
      if (overlap.height > MIN (titlebar_rect.height, min_height_needed) &&
          overlap.width  > MIN (titlebar_rect.width * min_width_percent, 
                                min_width_absolute))
        {
          is_onscreen = TRUE;
          break;
        }
        
      onscreen_region = onscreen_region->next;
    }

  return is_onscreen;
}

static double
timeval_to_ms (const GTimeVal *timeval)
{
  return (timeval->tv_sec * G_USEC_PER_SEC + timeval->tv_usec) / 1000.0;
}

static double
time_diff (const GTimeVal *first,
	   const GTimeVal *second)
{
  double first_ms = timeval_to_ms (first);
  double second_ms = timeval_to_ms (second);

  return first_ms - second_ms;
}

static gboolean
check_moveresize_frequency (MetaWindow *window, 
			    gdouble    *remaining)
{
  GTimeVal current_time;
  
  g_get_current_time (&current_time);

#ifdef HAVE_XSYNC
  if (!window->disable_sync &&
      window->display->grab_sync_request_alarm != None)
    {
      if (window->sync_request_time.tv_sec != 0 ||
	  window->sync_request_time.tv_usec != 0)
	{
	  double elapsed =
	    time_diff (&current_time, &window->sync_request_time);

	  if (elapsed < 1000.0)
	    {
	      /* We want to be sure that the timeout happens at
	       * a time where elapsed will definitely be
	       * greater than 1000, so we can disable sync
	       */
	      if (remaining)
		*remaining = 1000.0 - elapsed + 100;
	      
	      return FALSE;
	    }
	  else
	    {
	      /* We have now waited for more than a second for the
	       * application to respond to the sync request
	       */
	      window->disable_sync = TRUE;
	      return TRUE;
	    }
	}
      else
	{
	  /* No outstanding sync requests. Go ahead and resize
	   */
	  return TRUE;
	}
    }
  else
#endif /* HAVE_XSYNC */
    {
      const double max_resizes_per_second = 25.0;
      const double ms_between_resizes = 1000.0 / max_resizes_per_second;
      double elapsed;

      elapsed = time_diff (&current_time, &window->display->grab_last_moveresize_time);

      if (elapsed >= 0.0 && elapsed < ms_between_resizes)
	{
	  meta_topic (META_DEBUG_RESIZING,
		      "Delaying move/resize as only %g of %g ms elapsed\n",
		      elapsed, ms_between_resizes);
	  
	  if (remaining)
	    *remaining = (ms_between_resizes - elapsed);

	  return FALSE;
	}
      
      meta_topic (META_DEBUG_RESIZING,
		  " Checked moveresize freq, allowing move/resize now (%g of %g seconds elapsed)\n",
		  elapsed / 1000.0, 1.0 / max_resizes_per_second);
      
      return TRUE;
    }
}

static gboolean
update_move_timeout (gpointer data)
{
  MetaWindow *window = data;

  update_move (window, 
               window->display->grab_last_user_action_was_snap,
               window->display->grab_latest_motion_x,
               window->display->grab_latest_motion_y);

  return FALSE;
}

static void
update_move (MetaWindow  *window,
             gboolean     snap,
             int          x,
             int          y)
{
  int dx, dy;
  int new_x, new_y;
  MetaRectangle old;
  int shake_threshold;
  MetaDisplay *display = window->display;
  
  display->grab_latest_motion_x = x;
  display->grab_latest_motion_y = y;
  
  dx = x - display->grab_anchor_root_x;
  dy = y - display->grab_anchor_root_y;

  new_x = display->grab_anchor_window_pos.x + dx;
  new_y = display->grab_anchor_window_pos.y + dy;

  meta_verbose ("x,y = %d,%d anchor ptr %d,%d anchor pos %d,%d dx,dy %d,%d\n",
                x, y,
                display->grab_anchor_root_x,
                display->grab_anchor_root_y,
                display->grab_anchor_window_pos.x,
                display->grab_anchor_window_pos.y,
                dx, dy);

  /* Don't bother doing anything if no move has been specified.  (This
   * happens often, even in keyboard moving, due to the warping of the
   * pointer.
   */
  if (dx == 0 && dy == 0)
    return;

  /* shake loose (unmaximize) maximized window if dragged beyond the threshold
   * in the Y direction. You can't pull a window loose via X motion.
   */

#define DRAG_THRESHOLD_TO_SHAKE_THRESHOLD_FACTOR 6
  shake_threshold = meta_ui_get_drag_threshold (window->screen->ui) *
    DRAG_THRESHOLD_TO_SHAKE_THRESHOLD_FACTOR;
    
  if (META_WINDOW_MAXIMIZED (window) && ABS (dy) >= shake_threshold)
    {
      double prop;

      /* Shake loose */
      window->shaken_loose = TRUE;
                  
      /* move the unmaximized window to the cursor */
      prop = 
        ((double)(x - display->grab_initial_window_pos.x)) / 
        ((double)display->grab_initial_window_pos.width);

      display->grab_initial_window_pos.x = 
        x - window->saved_rect.width * prop;
      display->grab_initial_window_pos.y = y;

      if (window->frame)
        {
          display->grab_initial_window_pos.y += window->frame->child_y / 2;
        }

      window->saved_rect.x = display->grab_initial_window_pos.x;
      window->saved_rect.y = display->grab_initial_window_pos.y;
      display->grab_anchor_root_x = x;
      display->grab_anchor_root_y = y;

      meta_window_unmaximize (window,
                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);

      return;
    }
  /* remaximize window on an other xinerama monitor if window has
   * been shaken loose or it is still maximized (then move straight)
   */
  else if (window->shaken_loose || META_WINDOW_MAXIMIZED (window))
    {
      const MetaXineramaScreenInfo *wxinerama;
      MetaRectangle work_area;
      int monitor;

      wxinerama = meta_screen_get_xinerama_for_window (window->screen, window);

      for (monitor = 0; monitor < window->screen->n_xinerama_infos; monitor++)
        {
          meta_window_get_work_area_for_xinerama (window, monitor, &work_area);

          /* check if cursor is near the top of a xinerama work area */ 
          if (x >= work_area.x &&
              x < (work_area.x + work_area.width) &&
              y >= work_area.y &&
              y < (work_area.y + shake_threshold))
            {
              /* move the saved rect if window will become maximized on an
               * other monitor so user isn't surprised on a later unmaximize
               */
              if (wxinerama->number != monitor)
                {
                  window->saved_rect.x = work_area.x;
                  window->saved_rect.y = work_area.y;
                  
                  if (window->frame) 
                    {
                      window->saved_rect.x += window->frame->child_x;
                      window->saved_rect.y += window->frame->child_y;
                    }

                  meta_window_unmaximize (window,
                                          META_MAXIMIZE_HORIZONTAL |
                                          META_MAXIMIZE_VERTICAL);
                }

              display->grab_initial_window_pos = work_area;
              display->grab_anchor_root_x = x;
              display->grab_anchor_root_y = y;
              window->shaken_loose = FALSE;
              
              meta_window_maximize (window,
                                    META_MAXIMIZE_HORIZONTAL |
                                    META_MAXIMIZE_VERTICAL);

              return;
            }
        }
    }

  if (display->grab_wireframe_active)
    old = display->grab_wireframe_rect;
  else
    {
      old = window->rect;
      meta_window_get_position (window, &old.x, &old.y);
    }

  /* Don't allow movement in the maximized directions */
  if (window->maximized_horizontally)
    new_x = old.x;
  if (window->maximized_vertically)
    new_y = old.y;

  /* Do any edge resistance/snapping */
  meta_window_edge_resistance_for_move (window, 
                                        old.x,
                                        old.y,
                                        &new_x,
                                        &new_y,
                                        update_move_timeout,
                                        snap,
                                        FALSE);

  if (display->compositor)
    {
      int root_x = new_x - display->grab_anchor_window_pos.x + display->grab_anchor_root_x;
      int root_y = new_y - display->grab_anchor_window_pos.y + display->grab_anchor_root_y;
      
      meta_compositor_update_move (display->compositor,
				   window, root_x, root_y);
    }
  
  if (display->grab_wireframe_active)
    meta_window_update_wireframe (window, new_x, new_y, 
                                  display->grab_wireframe_rect.width,
                                  display->grab_wireframe_rect.height);
  else
    meta_window_move (window, TRUE, new_x, new_y);
}

static gboolean
update_resize_timeout (gpointer data)
{
  MetaWindow *window = data;

  update_resize (window, 
                 window->display->grab_last_user_action_was_snap,
                 window->display->grab_latest_motion_x,
                 window->display->grab_latest_motion_y,
                 TRUE);
  return FALSE;
}

static void
update_resize (MetaWindow *window,
               gboolean    snap,
               int x, int y,
               gboolean force)
{
  int dx, dy;
  int new_w, new_h;
  int gravity;
  MetaRectangle old;
  int new_x, new_y;
  double remaining;
  
  window->display->grab_latest_motion_x = x;
  window->display->grab_latest_motion_y = y;
  
  dx = x - window->display->grab_anchor_root_x;
  dy = y - window->display->grab_anchor_root_y;

  new_w = window->display->grab_anchor_window_pos.width;
  new_h = window->display->grab_anchor_window_pos.height;

  /* Don't bother doing anything if no move has been specified.  (This
   * happens often, even in keyboard resizing, due to the warping of the
   * pointer.
   */
  if (dx == 0 && dy == 0)
    return;
  
  /* FIXME this is only used in wireframe mode */
  new_x = window->display->grab_anchor_window_pos.x;
  new_y = window->display->grab_anchor_window_pos.y;

  if (window->display->grab_op == META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN)
    {
      if ((dx > 0) && (dy > 0))
        {
          window->display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_SE;
          meta_window_update_keyboard_resize (window, TRUE);
        }
      else if ((dx < 0) && (dy > 0))
        {
          window->display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_SW;
          meta_window_update_keyboard_resize (window, TRUE);
        }
      else if ((dx > 0) && (dy < 0))
        {
          window->display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_NE;
          meta_window_update_keyboard_resize (window, TRUE);
        }
      else if ((dx < 0) && (dy < 0))
        {
          window->display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_NW;
          meta_window_update_keyboard_resize (window, TRUE);
        }
      else if (dx < 0)
        {
          window->display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          meta_window_update_keyboard_resize (window, TRUE);
        }
      else if (dx > 0)
        {
          window->display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          meta_window_update_keyboard_resize (window, TRUE);
        }
      else if (dy > 0)
        {
          window->display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          meta_window_update_keyboard_resize (window, TRUE);
        }
      else if (dy < 0)
        {
          window->display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_N;
          meta_window_update_keyboard_resize (window, TRUE);
        }
    }
  
  /* FIXME: This stupidity only needed because of wireframe mode and
   * the fact that wireframe isn't making use of
   * meta_rectangle_resize_with_gravity().  If we were to use that, we
   * could just increment new_w and new_h by dx and dy in all cases.
   */
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
      new_w += dx;
      break;

    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
      new_w -= dx;
      new_x += dx;
      break;
      
    default:
      break;
    }
  
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
      new_h += dy;
      break;
      
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      new_h -= dy;
      new_y += dy;
      break;
    default:
      break;
    }

  if (!check_moveresize_frequency (window, &remaining) && !force)
    {
      /* we are ignoring an event here, so we schedule a
       * compensation event when we would otherwise not ignore
       * an event. Otherwise we can become stuck if the user never
       * generates another event.
       */
      if (!window->display->grab_resize_timeout_id)
	{
	  window->display->grab_resize_timeout_id = 
	    g_timeout_add ((int)remaining, update_resize_timeout, window);
	}

      return;
    }

  /* If we get here, it means the client should have redrawn itself */
  if (window->display->compositor)
    meta_compositor_set_updates (window->display->compositor, window, TRUE);

  /* Remove any scheduled compensation events */
  if (window->display->grab_resize_timeout_id)
    {
      g_source_remove (window->display->grab_resize_timeout_id);
      window->display->grab_resize_timeout_id = 0;
    }
  
  if (window->display->grab_wireframe_active)
    old = window->display->grab_wireframe_rect;
  else
    old = window->rect;  /* Don't actually care about x,y */

  /* One sided resizing ought to actually be one-sided, despite the fact that
   * aspect ratio windows don't interact nicely with the above stuff.  So,
   * to avoid some nasty flicker, we enforce that.
   */
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_RESIZING_N:
      new_w = old.width;
      break;
      
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_RESIZING_W:
      new_h = old.height;
      break;

    default:
      break;
    }

  /* compute gravity of client during operation */
  gravity = meta_resize_gravity_from_grab_op (window->display->grab_op);
  g_assert (gravity >= 0);
  
  /* Do any edge resistance/snapping */
  meta_window_edge_resistance_for_resize (window,
                                          old.width,
                                          old.height,
                                          &new_w,
                                          &new_h,
                                          gravity,
                                          update_resize_timeout,
                                          snap,
                                          FALSE);

  if (window->display->grab_wireframe_active)
    {
      if ((new_x + new_w <= new_x) || (new_y + new_h <= new_y))
        return;

      /* FIXME This is crap. For example, the wireframe isn't
       * constrained in the way that a real resize would be. An
       * obvious elegant solution is to unmap the window during
       * wireframe, but still resize it; however, that probably
       * confuses broken clients that have problems with opaque
       * resize, they probably don't track their visibility.
       */
      meta_window_update_wireframe (window, new_x, new_y, new_w, new_h);
    }
  else
    {
      /* We don't need to update unless the specified width and height
       * are actually different from what we had before.
       */
      if (old.width != new_w || old.height != new_h)
        meta_window_resize_with_gravity (window, TRUE, new_w, new_h, gravity);
    }

  /* Store the latest resize time, if we actually resized. */
  if (window->rect.width != old.width || window->rect.height != old.height)
    g_get_current_time (&window->display->grab_last_moveresize_time);
}

typedef struct
{
  const XEvent *current_event;
  int           count;
  guint32       last_time;
} EventScannerData;

static Bool
find_last_time_predicate (Display  *display,
                          XEvent   *xevent,
                          XPointer  arg)
{
  EventScannerData *esd = (void*) arg;

  if (esd->current_event->type == xevent->type &&
      esd->current_event->xany.window == xevent->xany.window)
    {
      esd->count += 1;
      esd->last_time = xevent->xmotion.time;
    }

  return False;
}

static gboolean
check_use_this_motion_notify (MetaWindow *window,
                              XEvent     *event)
{
  EventScannerData esd;
  XEvent useless;

  /* This code is copied from Owen's GDK code. */
  
  if (window->display->grab_motion_notify_time != 0)
    {
      /* == is really the right test, but I'm all for paranoia */
      if (window->display->grab_motion_notify_time <=
          event->xmotion.time)
        {
          meta_topic (META_DEBUG_RESIZING,
                      "Arrived at event with time %u (waiting for %u), using it\n",
                      (unsigned int)event->xmotion.time,
                      window->display->grab_motion_notify_time);
          window->display->grab_motion_notify_time = 0;
          return TRUE;
        }
      else
        return FALSE; /* haven't reached the saved timestamp yet */
    }
  
  esd.current_event = event;
  esd.count = 0;
  esd.last_time = 0;

  /* "useless" isn't filled in because the predicate never returns True */
  XCheckIfEvent (window->display->xdisplay,
                 &useless,
                 find_last_time_predicate,
                 (XPointer) &esd);

  if (esd.count > 0)
    meta_topic (META_DEBUG_RESIZING,
                "Will skip %d motion events and use the event with time %u\n",
                esd.count, (unsigned int) esd.last_time);
  
  if (esd.last_time == 0)
    return TRUE;
  else
    {
      /* Save this timestamp, and ignore all motion notify
       * until we get to the one with this stamp.
       */
      window->display->grab_motion_notify_time = esd.last_time;
      return FALSE;
    }
}

void
meta_window_handle_mouse_grab_op_event (MetaWindow *window,
                                        XEvent     *event)
{
#ifdef HAVE_XSYNC
  if (event->type == (window->display->xsync_event_base + XSyncAlarmNotify))
    {
      meta_topic (META_DEBUG_RESIZING,
                  "Alarm event received last motion x = %d y = %d\n",
                  window->display->grab_latest_motion_x,
                  window->display->grab_latest_motion_y);

      /* If sync was previously disabled, turn it back on and hope
       * the application has come to its senses (maybe it was just
       * busy with a pagefault or a long computation).
       */
      window->disable_sync = FALSE;
      window->sync_request_time.tv_sec = 0;
      window->sync_request_time.tv_usec = 0;
      
      /* This means we are ready for another configure. */
      switch (window->display->grab_op)
        {
        case META_GRAB_OP_RESIZING_E:
        case META_GRAB_OP_RESIZING_W:
        case META_GRAB_OP_RESIZING_S:
        case META_GRAB_OP_RESIZING_N:
        case META_GRAB_OP_RESIZING_SE:
        case META_GRAB_OP_RESIZING_SW:
        case META_GRAB_OP_RESIZING_NE:
        case META_GRAB_OP_RESIZING_NW:
        case META_GRAB_OP_KEYBOARD_RESIZING_S:
        case META_GRAB_OP_KEYBOARD_RESIZING_N:
        case META_GRAB_OP_KEYBOARD_RESIZING_W:
        case META_GRAB_OP_KEYBOARD_RESIZING_E:
        case META_GRAB_OP_KEYBOARD_RESIZING_SE:
        case META_GRAB_OP_KEYBOARD_RESIZING_NE:
        case META_GRAB_OP_KEYBOARD_RESIZING_SW:
        case META_GRAB_OP_KEYBOARD_RESIZING_NW:
          /* no pointer round trip here, to keep in sync */
          update_resize (window,
                         window->display->grab_last_user_action_was_snap,
                         window->display->grab_latest_motion_x,
                         window->display->grab_latest_motion_y,
                         TRUE);
          break;
          
        default:
          break;
        }
    }
#endif /* HAVE_XSYNC */
  
  switch (event->type)
    {
    case ButtonRelease:
      meta_display_check_threshold_reached (window->display,
                                            event->xbutton.x_root,
                                            event->xbutton.y_root);
      /* If the user was snap moving then ignore the button release
       * because they may have let go of shift before releasing the
       * mouse button and they almost certainly do not want a
       * non-snapped movement to occur from the button release.
       */
      if (!window->display->grab_last_user_action_was_snap)
        {
          if (meta_grab_op_is_moving (window->display->grab_op))
            {
              if (event->xbutton.root == window->screen->xroot)
                update_move (window, event->xbutton.state & ShiftMask,
                             event->xbutton.x_root, event->xbutton.y_root);
            }
          else if (meta_grab_op_is_resizing (window->display->grab_op))
            {
              if (event->xbutton.root == window->screen->xroot)
                update_resize (window,
                               event->xbutton.state & ShiftMask,
                               event->xbutton.x_root,
                               event->xbutton.y_root,
                               TRUE);
	      if (window->display->compositor)
		meta_compositor_set_updates (window->display->compositor, window, TRUE);
            }
        }

      meta_display_end_grab_op (window->display, event->xbutton.time);
      break;    

    case MotionNotify:
      meta_display_check_threshold_reached (window->display,
                                            event->xmotion.x_root,
                                            event->xmotion.y_root);
      if (meta_grab_op_is_moving (window->display->grab_op))
        {
          if (event->xmotion.root == window->screen->xroot)
            {
              if (check_use_this_motion_notify (window,
                                                event))
                update_move (window,
                             event->xmotion.state & ShiftMask,
                             event->xmotion.x_root,
                             event->xmotion.y_root);
            }
        }
      else if (meta_grab_op_is_resizing (window->display->grab_op))
        {
          if (event->xmotion.root == window->screen->xroot)
            {
              if (check_use_this_motion_notify (window,
                                                event))
                update_resize (window,
                               event->xmotion.state & ShiftMask,
                               event->xmotion.x_root,
                               event->xmotion.y_root,
                               FALSE);
            }
        }
      break;

    default:
      break;
    }
}

void
meta_window_set_gravity (MetaWindow *window,
                         int         gravity)
{
  XSetWindowAttributes attrs;

  meta_verbose ("Setting gravity of %s to %d\n", window->desc, gravity);

  attrs.win_gravity = gravity;
  
  meta_error_trap_push (window->display);

  XChangeWindowAttributes (window->display->xdisplay,
                           window->xwindow,
                           CWWinGravity,
                           &attrs);
  
  meta_error_trap_pop (window->display, FALSE);
}

static void
get_work_area_xinerama (MetaWindow    *window,
                        MetaRectangle *area,
                        int            which_xinerama)
{
  GList *tmp;  
  
  g_assert (which_xinerama >= 0);

  /* Initialize to the whole xinerama */
  *area = window->screen->xinerama_infos[which_xinerama].rect;
  
  tmp = meta_window_get_workspaces (window);  
  while (tmp != NULL)
    {
      MetaRectangle workspace_work_area;
      meta_workspace_get_work_area_for_xinerama (tmp->data,
                                                 which_xinerama,
                                                 &workspace_work_area);
      meta_rectangle_intersect (area,
                                &workspace_work_area,
                                area);
      tmp = tmp->next;
    }
  
  meta_topic (META_DEBUG_WORKAREA,
              "Window %s xinerama %d has work area %d,%d %d x %d\n",
              window->desc, which_xinerama,
              area->x, area->y, area->width, area->height);
}

void
meta_window_get_work_area_current_xinerama (MetaWindow    *window,
					    MetaRectangle *area)
{
  const MetaXineramaScreenInfo *xinerama = NULL;
  xinerama = meta_screen_get_xinerama_for_window (window->screen,
						  window);

  meta_window_get_work_area_for_xinerama (window,
                                          xinerama->number,
                                          area);
}

void
meta_window_get_work_area_for_xinerama (MetaWindow    *window,
					int            which_xinerama,
					MetaRectangle *area)
{
  g_return_if_fail (which_xinerama >= 0);
  
  get_work_area_xinerama (window,
                          area,
                          which_xinerama);
}

void
meta_window_get_work_area_all_xineramas (MetaWindow    *window,
                                         MetaRectangle *area)
{
  GList *tmp;  

  /* Initialize to the whole screen */
  *area = window->screen->rect;
  
  tmp = meta_window_get_workspaces (window);  
  while (tmp != NULL)
    {
      MetaRectangle workspace_work_area;
      meta_workspace_get_work_area_all_xineramas (tmp->data,
                                                  &workspace_work_area);
      meta_rectangle_intersect (area,
                                &workspace_work_area,
                                area);
      tmp = tmp->next;
    }

  meta_topic (META_DEBUG_WORKAREA,
              "Window %s has whole-screen work area %d,%d %d x %d\n",
              window->desc, area->x, area->y, area->width, area->height);
}


gboolean
meta_window_same_application (MetaWindow *window,
                              MetaWindow *other_window)
{
  return
    meta_window_get_group (window) ==
    meta_window_get_group (other_window);
}

void
meta_window_refresh_resize_popup (MetaWindow *window)
{
  if (window->display->grab_op == META_GRAB_OP_NONE)
    return;

  if (window->display->grab_window != window)
    return;

  /* We shouldn't ever get called when the wireframe is active
   * because that's handled by a different code path in effects.c
   */
  if (window->display->grab_wireframe_active)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "refresh_resize_popup called when wireframe active\n");
      return;
    }
  
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      break;

    default:
      /* Not resizing */
      return;
    }
      
  if (window->display->grab_resize_popup == NULL)
    {
      if (window->size_hints.width_inc > 1 ||
          window->size_hints.height_inc > 1)
        window->display->grab_resize_popup =
          meta_ui_resize_popup_new (window->display->xdisplay,
                                    window->screen->number);
    }
  
  if (window->display->grab_resize_popup != NULL)
    {
      MetaRectangle rect;
      
      if (window->display->grab_wireframe_active)
        {
          rect = window->display->grab_wireframe_rect;
        }
      else
        {
          meta_window_get_position (window, &rect.x, &rect.y);
          rect.width = window->rect.width;
          rect.height = window->rect.height;
        }
      
      meta_ui_resize_popup_set (window->display->grab_resize_popup,
                                rect,
                                window->size_hints.base_width,
                                window->size_hints.base_height,
                                window->size_hints.width_inc,
                                window->size_hints.height_inc);

      meta_ui_resize_popup_set_showing (window->display->grab_resize_popup,
                                        TRUE);
    }
}

void
meta_window_foreach_transient (MetaWindow            *window,
                               MetaWindowForeachFunc  func,
                               void                  *data)
{
  GSList *windows;
  GSList *tmp;

  windows = meta_display_list_windows (window->display);

  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *transient = tmp->data;
      
      if (meta_window_is_ancestor_of_transient (window, transient))
        {
          if (!(* func) (transient, data))
            break;
        }
      
      tmp = tmp->next;
    }

  g_slist_free (windows);
}

void
meta_window_foreach_ancestor (MetaWindow            *window,
                              MetaWindowForeachFunc  func,
                              void                  *data)
{
  MetaWindow *w;
  MetaWindow *tortoise;
  
  w = window;
  tortoise = window;
  while (TRUE)
    {
      if (w->xtransient_for == None ||
          w->transient_parent_is_root_window)
        break;
      
      w = meta_display_lookup_x_window (w->display, w->xtransient_for);
      
      if (w == NULL || w == tortoise)
        break;

      if (!(* func) (w, data))
        break;      
      
      if (w->xtransient_for == None ||
          w->transient_parent_is_root_window)
        break;

      w = meta_display_lookup_x_window (w->display, w->xtransient_for);
      
      if (w == NULL || w == tortoise)
        break;

      if (!(* func) (w, data))
        break;
      
      tortoise = meta_display_lookup_x_window (tortoise->display,
                                               tortoise->xtransient_for);

      /* "w" should have already covered all ground covered by the
       * tortoise, so the following must hold.
       */
      g_assert (tortoise != NULL);
      g_assert (tortoise->xtransient_for != None);
      g_assert (!tortoise->transient_parent_is_root_window);
    }
}

typedef struct
{
  MetaWindow *ancestor;
  gboolean found;
} FindAncestorData;

static gboolean
find_ancestor_func (MetaWindow *window,
                    void       *data)
{
  FindAncestorData *d = data;

  if (window == d->ancestor)
    {
      d->found = TRUE;
      return FALSE;
    }

  return TRUE;
}

gboolean
meta_window_is_ancestor_of_transient (MetaWindow *window,
                                      MetaWindow *transient)
{
  FindAncestorData d;

  d.ancestor = window;
  d.found = FALSE;

  meta_window_foreach_ancestor (transient, find_ancestor_func, &d);

  return d.found;
}

/* Warp pointer to location appropriate for grab,
 * return root coordinates where pointer ended up.
 */
static gboolean
warp_grab_pointer (MetaWindow          *window,
                   MetaGrabOp           grab_op,
                   int                 *x,
                   int                 *y)
{
  MetaRectangle rect;

  /* We may not have done begin_grab_op yet, i.e. may not be in a grab
   */
  
  if (window == window->display->grab_window &&
      window->display->grab_wireframe_active)
    {
      meta_window_get_xor_rect (window, &window->display->grab_wireframe_rect,
                                &rect);
    }
  else
    {
      meta_window_get_outer_rect (window, &rect);
    }
  
  switch (grab_op)
    {
      case META_GRAB_OP_KEYBOARD_MOVING:
      case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
        *x = rect.width / 2;
        *y = rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_S:
        *x = rect.width / 2;
        *y = rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_N:
        *x = rect.width / 2;
        *y = 0;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_W:
        *x = 0;
        *y = rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_E:
        *x = rect.width;
        *y = rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_SE:
        *x = rect.width;
        *y = rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_NE:
        *x = rect.width;
        *y = 0;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_SW:
        *x = 0;
        *y = rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_NW:
        *x = 0;
        *y = 0;
        break;

      default:
        return FALSE;
    }

  *x += rect.x;
  *y += rect.y;

  /* Avoid weird bouncing at the screen edge; see bug 154706 */
  *x = CLAMP (*x, 0, window->screen->rect.width-1);
  *y = CLAMP (*y, 0, window->screen->rect.height-1);
  
  meta_error_trap_push_with_return (window->display);

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Warping pointer to %d,%d with window at %d,%d\n",
              *x, *y, rect.x, rect.y);

  /* Need to update the grab positions so that the MotionNotify and other
   * events generated by the XWarpPointer() call below don't cause complete
   * funkiness.  See bug 124582 and bug 122670.
   */
  window->display->grab_anchor_root_x = *x;
  window->display->grab_anchor_root_y = *y;
  window->display->grab_latest_motion_x = *x;
  window->display->grab_latest_motion_y = *y;
  if (window->display->grab_wireframe_active)
    window->display->grab_anchor_window_pos = 
      window->display->grab_wireframe_rect;
  else
    {
      window->display->grab_anchor_window_pos = window->rect;
      meta_window_get_position (window,
                                &window->display->grab_anchor_window_pos.x,
                                &window->display->grab_anchor_window_pos.y);
    }
  
  XWarpPointer (window->display->xdisplay,
                None,
                window->screen->xroot,
                0, 0, 0, 0, 
                *x, *y);

  if (meta_error_trap_pop_with_return (window->display, FALSE) != Success)
    {
      meta_verbose ("Failed to warp pointer for window %s\n",
                    window->desc);
      return FALSE;
    }
  
  return TRUE;
}

void
meta_window_begin_grab_op (MetaWindow *window,
                           MetaGrabOp  op,
                           guint32     timestamp)
{
  int x, y;
  gulong grab_start_serial;

  grab_start_serial = XNextRequest (window->display->xdisplay);
  
  warp_grab_pointer (window,
                     op, &x, &y);

  meta_display_begin_grab_op (window->display,
                              window->screen,
                              window,
                              op,
                              FALSE,
                              grab_start_serial /* event_serial */,
                              0 /* button */,
                              0,
                              timestamp,
                              x, y);

  /* We override the one set in display_begin_grab_op since we
   * did additional stuff as part of the grabbing process
   */
  window->display->grab_start_serial = grab_start_serial;
}

void
meta_window_update_keyboard_resize (MetaWindow *window,
                                    gboolean    update_cursor)
{
  int x, y;
  
  warp_grab_pointer (window,
                     window->display->grab_op,
                     &x, &y);

  if (update_cursor)
    {
      guint32 timestamp;
      /* FIXME: Using CurrentTime is really bad mojo */
      timestamp = CurrentTime;
      meta_display_set_grab_op_cursor (window->display,
                                       NULL,
                                       window->display->grab_op,
                                       TRUE,
                                       window->display->grab_xwindow,
                                       timestamp);
    }
}

void
meta_window_update_keyboard_move (MetaWindow *window)
{
  int x, y;
  
  warp_grab_pointer (window,
                     window->display->grab_op,
                     &x, &y);
}

void
meta_window_update_layer (MetaWindow *window)
{
  MetaGroup *group;
  
  meta_stack_freeze (window->screen->stack);
  group = meta_window_get_group (window);
  if (group)
    meta_group_update_layers (group);
  else
    meta_stack_update_layer (window->screen->stack, window);
  meta_stack_thaw (window->screen->stack);
}

/* ensure_mru_position_after ensures that window appears after
 * below_this_one in the active_workspace's mru_list (i.e. it treats
 * window as having been less recently used than below_this_one)
 */
static void
ensure_mru_position_after (MetaWindow *window,
                           MetaWindow *after_this_one)
{
  /* This is sort of slow since it runs through the entire list more
   * than once (especially considering the fact that we expect the
   * windows of interest to be the first two elements in the list),
   * but it doesn't matter while we're only using it on new window
   * map.
   */

  GList* active_mru_list;
  GList* window_position;
  GList* after_this_one_position;

  active_mru_list         = window->screen->active_workspace->mru_list;
  window_position         = g_list_find (active_mru_list, window);
  after_this_one_position = g_list_find (active_mru_list, after_this_one);

  /* after_this_one_position is NULL when we switch workspaces, but in
   * that case we don't need to do any MRU shuffling so we can simply
   * return.
   */
  if (after_this_one_position == NULL)
    return;

  if (g_list_length (window_position) > g_list_length (after_this_one_position))
    {
      window->screen->active_workspace->mru_list =
        g_list_delete_link (window->screen->active_workspace->mru_list,
                            window_position);

      window->screen->active_workspace->mru_list =
        g_list_insert_before (window->screen->active_workspace->mru_list,
                              after_this_one_position->next,
                              window);
    }
}

void
meta_window_stack_just_below (MetaWindow *window,
                              MetaWindow *below_this_one)
{
  g_return_if_fail (window         != NULL);
  g_return_if_fail (below_this_one != NULL);

  if (window->stack_position > below_this_one->stack_position)
    {
      meta_topic (META_DEBUG_STACK,
                  "Setting stack position of window %s to %d (making it below window %s).\n",
                  window->desc, 
                  below_this_one->stack_position, 
                  below_this_one->desc);
      meta_window_set_stack_position (window, below_this_one->stack_position);
    }
  else
    {
      meta_topic (META_DEBUG_STACK,
                  "Window %s  was already below window %s.\n",
                  window->desc, below_this_one->desc);
    }
}

void
meta_window_set_user_time (MetaWindow *window,
                           guint32     timestamp)
{
  /* FIXME: If Soeren's suggestion in bug 151984 is implemented, it will allow
   * us to sanity check the timestamp here and ensure it doesn't correspond to
   * a future time.
   */

  /* Only update the time if this timestamp is newer... */
  if (window->net_wm_user_time_set &&
      XSERVER_TIME_IS_BEFORE (timestamp, window->net_wm_user_time))
    {
      meta_topic (META_DEBUG_STARTUP,
                  "Window %s _NET_WM_USER_TIME not updated to %u, because it "
                  "is less than %u\n",
                  window->desc, timestamp, window->net_wm_user_time);
    }
  else
    {
      meta_topic (META_DEBUG_STARTUP,
                  "Window %s has _NET_WM_USER_TIME of %u\n",
                  window->desc, timestamp);
      window->net_wm_user_time_set = TRUE;
      window->net_wm_user_time = timestamp;
      if (XSERVER_TIME_IS_BEFORE (window->display->last_user_time, timestamp))
        window->display->last_user_time = timestamp;

      /* If this is a terminal, user interaction with it means the user likely
       * doesn't want to have focus transferred for now due to new windows.
       */
      if (meta_prefs_get_focus_new_windows () ==
               META_FOCUS_NEW_WINDOWS_STRICT &&
          __window_is_terminal (window))
        window->display->allow_terminal_deactivation = FALSE;
    }
}

/* Sets the demands_attention hint on a window, but only
 * if it's at least partially obscured (see #305882).
 */
void
meta_window_set_demands_attention (MetaWindow *window)
{
  MetaRectangle candidate_rect, other_rect;
  GList *stack = window->screen->stack->sorted;
  MetaWindow *other_window;
  gboolean obscured = FALSE;
  
  /* Does the window have any other window on this workspace
   * overlapping it?
   */

  meta_window_get_outer_rect (window, &candidate_rect);

  /* The stack is sorted with the top windows first. */
  
  while (stack != NULL && stack->data != window)
    {
      other_window = stack->data;
      stack = stack->next;
     
      if (other_window->on_all_workspaces ||
          window->on_all_workspaces ||
          other_window->workspace == window->workspace)
        {
          meta_window_get_outer_rect (other_window, &other_rect);

          if (meta_rectangle_overlap (&candidate_rect, &other_rect))
            {
              obscured = TRUE;
              break;
            }
        }
    }

  /* If the window's in full view, there's no point setting the flag. */
  
  if (!obscured)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
          "Not marking %s as needing attention because it's in full view\n",
          window->desc);
      return;
    }
    
  /* Otherwise, go ahead and set the flag. */
  
  meta_topic (META_DEBUG_WINDOW_OPS,
      "Marking %s as needing attention\n", window->desc);

  window->wm_state_demands_attention = TRUE;
  set_net_wm_state (window);
}

void
meta_window_unset_demands_attention (MetaWindow *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
      "Marking %s as not needing attention\n", window->desc);
  
  window->wm_state_demands_attention = FALSE;
  set_net_wm_state (window);
}
