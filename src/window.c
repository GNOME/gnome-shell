/* Metacity X managed windows */

/* 
 * Copyright (C) 2001 Havoc Pennington, Anders Carlsson
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
#include "stack.h"
#include "keybindings.h"
#include "ui.h"
#include "place.h"
#include "session.h"
#include "effects.h"

#include <X11/Xatom.h>

static void constrain_size     (MetaWindow        *window,
                                MetaFrameGeometry *fgeom,
                                int                width,
                                int                height,
                                int               *new_width,
                                int               *new_height);
static void constrain_position (MetaWindow        *window,
                                MetaFrameGeometry *fgeom,
                                int                x,
                                int                y,
                                int               *new_x,
                                int               *new_y);

static int      update_size_hints         (MetaWindow     *window);
static int      update_title              (MetaWindow     *window);
static int      update_protocols          (MetaWindow     *window);
static int      update_wm_hints           (MetaWindow     *window);
static int      update_net_wm_state       (MetaWindow     *window);
static int      update_mwm_hints          (MetaWindow     *window);
static int      update_wm_class           (MetaWindow     *window);
static int      update_transient_for      (MetaWindow     *window);
static void     update_sm_hints           (MetaWindow     *window);
static int      update_role               (MetaWindow     *window);
static int      update_net_wm_type        (MetaWindow     *window);
static int      update_initial_workspace  (MetaWindow     *window);
static int      update_icon_name          (MetaWindow     *window);
static int      update_icon               (MetaWindow     *window);
static void     recalc_window_type        (MetaWindow     *window);
static void     recalc_window_features    (MetaWindow     *window);
static int      set_wm_state              (MetaWindow     *window,
                                           int             state);
static int      set_net_wm_state          (MetaWindow     *window);
static void     send_configure_notify     (MetaWindow     *window);
static gboolean process_property_notify   (MetaWindow     *window,
                                           XPropertyEvent *event);
static void     meta_window_show          (MetaWindow     *window);
static void     meta_window_hide          (MetaWindow     *window);

static gboolean meta_window_get_icon_geometry (MetaWindow    *window,
                                               MetaRectangle *rect);

static void adjust_for_gravity               (MetaWindow        *window,
                                              MetaFrameGeometry *fgeom,
                                              gboolean           coords_assume_border,
                                              int                x,
                                              int                y,
                                              int               *xp,
                                              int               *yp);
static void meta_window_move_resize_internal (MetaWindow        *window,
                                              gboolean           is_configure_request,
                                              gboolean           do_gravity_adjust,
                                              int                resize_gravity,
                                              int                root_x_nw,
                                              int                root_y_nw,
                                              int                w,
                                              int                h);


void meta_window_move_resize_now (MetaWindow  *window);

static gboolean get_cardinal (MetaDisplay *display,
                              Window       xwindow,
                              Atom         atom,
                              gulong      *val);

void meta_window_unqueue_calc_showing (MetaWindow *window);

static void meta_window_apply_session_info (MetaWindow                  *window,
                                            const MetaWindowSessionInfo *info);


MetaWindow*
meta_window_new (MetaDisplay *display, Window xwindow,
                 gboolean must_be_viewable)
{
  MetaWindow *window;
  XWindowAttributes attrs;
  GSList *tmp;
  MetaWorkspace *space;
  gulong existing_wm_state;
  
  meta_verbose ("Attempting to manage 0x%lx\n", xwindow);

  /* Grab server */
  meta_display_grab (display);
  
  meta_error_trap_push (display);
  
  XGetWindowAttributes (display->xdisplay,
                        xwindow, &attrs);

  if (meta_error_trap_pop (display))
    {
      meta_verbose ("Failed to get attributes for window 0x%lx\n",
                    xwindow);
      meta_display_ungrab (display);
      return NULL;
    }
  
  if (attrs.override_redirect)
    {
      meta_verbose ("Deciding not to manage override_redirect window 0x%lx\n", xwindow);
      meta_display_ungrab (display);
      return NULL;
    }

  existing_wm_state = WithdrawnState;
  if (must_be_viewable && attrs.map_state != IsViewable)
    {
      /* Only manage if WM_STATE is IconicState or NormalState */
      gulong state;

      if (!(get_cardinal (display, xwindow,
                          display->atom_wm_state,
                          &state) &&
            (state == IconicState || state == NormalState)))
        {
          meta_verbose ("Deciding not to manage unmapped or unviewable window 0x%lx\n", xwindow);
          meta_display_ungrab (display);
          return NULL;
        }

      existing_wm_state = state;
    }
  
  meta_error_trap_push (display);
  
  XAddToSaveSet (display->xdisplay, xwindow);

  XSelectInput (display->xdisplay, xwindow,
                PropertyChangeMask |
                EnterWindowMask | LeaveWindowMask |
                FocusChangeMask);  

  /* Get rid of any borders */
  if (attrs.border_width != 0)
    XSetWindowBorderWidth (display->xdisplay, xwindow, 0);

  /* Get rid of weird gravities */
  if (attrs.win_gravity != NorthWestGravity)
    {
      XSetWindowAttributes set_attrs;
      
      set_attrs.win_gravity = NorthWestGravity;
      
      XChangeWindowAttributes (display->xdisplay,
                               xwindow,
                               CWWinGravity,
                               &set_attrs);
    }
  
  if (meta_error_trap_pop (display) != Success)
    {
      meta_verbose ("Window 0x%lx disappeared just as we tried to manage it\n",
                    xwindow);
      meta_display_ungrab (display);
      return NULL;
    }

  g_assert (!attrs.override_redirect);
  
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

  /* avoid tons of stack updates */
  meta_stack_freeze (window->screen->stack);
  
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
  window->icon_name = NULL;
  window->icon = NULL;
  
  window->desc = g_strdup_printf ("0x%lx", window->xwindow);

  window->frame = NULL;
  window->has_focus = FALSE;

  window->user_has_resized = FALSE;
  window->user_has_moved = FALSE;
  
  window->maximized = FALSE;
  window->on_all_workspaces = FALSE;
  window->shaded = FALSE;
  window->initially_iconic = FALSE;
  window->minimized = FALSE;
  window->iconic = FALSE;
  window->mapped = attrs.map_state != IsUnmapped;
  /* if already mapped we don't want to do the placement thing */
  window->placed = window->mapped;
  if (window->placed)
    meta_verbose ("Not placing window 0x%lx since it's already mapped\n",
                  xwindow);
  window->unmanaging = FALSE;
  window->calc_showing_queued = FALSE;
  window->keys_grabbed = FALSE;
  window->grab_on_frame = FALSE;
  window->all_keys_grabbed = FALSE;
  window->withdrawn = FALSE;
  window->initial_workspace_set = FALSE;
  window->calc_placement = FALSE;
  
  window->unmaps_pending = 0;

  window->mwm_decorated = TRUE;
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
  
  window->wm_state_modal = FALSE;
  window->wm_state_skip_taskbar = FALSE;
  window->wm_state_skip_pager = FALSE;
  
  window->res_class = NULL;
  window->res_name = NULL;
  window->role = NULL;
  window->sm_client_id = NULL;
  
  window->xtransient_for = None;
  window->xgroup_leader = None;
  window->xclient_leader = None;

  window->icon_pixmap = None;
  window->icon_mask = None;
  
  window->type = META_WINDOW_NORMAL;
  window->type_atom = None;

  window->layer = META_LAYER_NORMAL;
  window->stack_op = NULL;
  window->initial_workspace = 0; /* not used */
  meta_display_register_x_window (display, &window->xwindow, window);

  update_size_hints (window);
  update_title (window);
  update_protocols (window);  
  update_wm_hints (window);
  update_net_wm_state (window);
  update_mwm_hints (window);
  update_wm_class (window);
  update_transient_for (window);
  update_sm_hints (window); /* must come after transient_for */
  update_role (window);
  update_net_wm_type (window);
  update_initial_workspace (window);
  update_icon_name (window);
  update_icon (window);

  if (!window->mapped &&
      (window->size_hints.flags & PPosition) == 0 &&
      (window->size_hints.flags & USPosition) == 0)
    {
      /* ignore current window position */
      window->size_hints.x = 0;
      window->size_hints.y = 0;
    }
  
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
  
  /* FIXME we have a tendency to set this then immediately
   * change it again.
   */
  set_wm_state (window, window->iconic ? IconicState : NormalState);
  set_net_wm_state (window);

  if (window->decorated)
    meta_window_ensure_frame (window);

  meta_window_grab_keys (window);
  meta_display_grab_window_buttons (window->display, window->xwindow);
  
  /* For the workspace, first honor hints,
   * if that fails put transients with parents,
   * otherwise put window on active space
   */
  
  if (window->initial_workspace_set)
    {
      space =
        meta_display_get_workspace_by_screen_index (window->display,
                                                    window->screen,
                                                    window->initial_workspace);

      if (space)
        meta_workspace_add_window (space, window);
    }
  
  if (window->workspaces == NULL && 
      window->xtransient_for != None)
    {
      /* Try putting dialog on parent's workspace */
      MetaWindow *parent;

      parent = meta_display_lookup_x_window (window->display,
                                             window->xtransient_for);

      if (parent)
        {
          GList *tmp;
          
          if (parent->on_all_workspaces)
            window->on_all_workspaces = TRUE;
          
          tmp = parent->workspaces;
          while (tmp != NULL)
            {
              meta_workspace_add_window (tmp->data, window);
              
              tmp = tmp->next;
            }
        }
    }
  
  if (window->workspaces == NULL)
    {
      space = window->screen->active_workspace;

      meta_workspace_add_window (space, window);
    }

  /* Only accept USPosition on normal windows because the app is full
   * of shit claiming the user set -geometry for a dialog or dock
   */
  if (window->type == META_WINDOW_NORMAL &&
      (window->size_hints.flags & USPosition))
    {
      /* don't constrain with placement algorithm */
      window->placed = TRUE;
      meta_verbose ("Honoring USPosition for %s instead of using placement algorithm\n", window->desc);
    }

  /* Assume the app knows best how to place these. */
  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK ||
      window->type == META_WINDOW_TOOLBAR ||
      window->type == META_WINDOW_MENU)
    {
      if (window->size_hints.flags & PPosition)
        {
          window->placed = TRUE;
          meta_verbose ("Not placing non-normal non-dialog window with PPosition set\n");
        }
    }

  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK)
    {
      /* Change the default, but don't enforce this if
       * the user focuses the dock/desktop and unsticks it
       * using key shortcuts
       */
      window->on_all_workspaces = TRUE;
    }
  
  /* Put our state back where it should be,
   * passing TRUE for is_configure_request, ICCCM says
   * initial map is handled same as configure request
   */
  meta_window_move_resize_internal (window, TRUE, FALSE,
                                    NorthWestGravity,
                                    window->size_hints.x,
                                    window->size_hints.y,
                                    window->size_hints.width,
                                    window->size_hints.height);

  meta_stack_add (window->screen->stack, 
                  window);

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
  
  /* Sync stack changes */
  meta_stack_thaw (window->screen->stack);
  
  meta_window_queue_calc_showing (window);

  meta_display_ungrab (display);
  
  return window;
}

/* This function should only be called from the end of meta_window_new () */
static void
meta_window_apply_session_info (MetaWindow *window,
                                const MetaWindowSessionInfo *info)
{
  if (info->on_all_workspaces_set)
    {
      window->on_all_workspaces = info->on_all_workspaces;
      meta_verbose ("Restoring sticky state %d for window %s\n",
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
            meta_display_get_workspace_by_screen_index (window->display,
                                                        window->screen,
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
          while (window->workspaces)
            meta_workspace_remove_window (window->workspaces->data, window);

          tmp = spaces;
          while (tmp != NULL)
            {
              MetaWorkspace *space;

              space = tmp->data;
              
              meta_workspace_add_window (space, window);              

              meta_verbose ("Restoring saved window %s to workspace %d\n",
                            window->desc,
                            meta_workspace_screen_index (space));
              
              tmp = tmp->next;
            }

          g_slist_free (spaces);
        }
    }

  if (info->geometry_set)
    {
      int x, y, w, h;
      
      window->placed = TRUE; /* don't do placement algorithms later */

      x = info->rect.x;
      y = info->rect.y;

      w = window->size_hints.base_width +
        info->rect.width * window->size_hints.width_inc;
      h = window->size_hints.base_height +
        info->rect.height * window->size_hints.height_inc;

      /* Force old gravity, ignoring anything now set */
      window->size_hints.win_gravity = info->gravity;
      
      meta_verbose ("Restoring pos %d,%d size %d x %d for %s\n",
                    x, y, w, h, window->desc);
      
      meta_window_move_resize_internal (window,
                                        FALSE, TRUE,
                                        NorthWestGravity,
                                        x, y, w, h);
    }
}

void
meta_window_free (MetaWindow  *window)
{
  GList *tmp;
  
  meta_verbose ("Unmanaging 0x%lx\n", window->xwindow);

  window->unmanaging = TRUE;

  if (window->display->grab_window == window)
    meta_display_end_grab_op (window->display, CurrentTime);
  
  if (window->display->focus_window == window)
    window->display->focus_window = NULL;

  if (window->display->prev_focus_window == window)
    window->display->prev_focus_window = NULL;
  
  meta_window_unqueue_calc_showing (window);
  
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

  meta_stack_remove (window->screen->stack, window);
  
  /* FIXME restore original size if window has maximized */

  if (window->withdrawn)
    set_wm_state (window, WithdrawnState);
  
  if (window->frame)
    meta_window_destroy_frame (window);

  meta_window_ungrab_keys (window);
  meta_display_ungrab_window_buttons (window->display, window->xwindow);
  
  meta_display_unregister_x_window (window->display, window->xwindow);
  
  /* Put back anything we messed up */
  meta_error_trap_push (window->display);
  if (window->border_width != 0)
    XSetWindowBorderWidth (window->display->xdisplay,
                           window->xwindow,
                           window->border_width);
  
  meta_error_trap_pop (window->display);

  g_free (window->sm_client_id);
  g_free (window->role);
  g_free (window->res_class);
  g_free (window->res_name);
  g_free (window->title);
  g_free (window->icon_name);
  g_free (window->desc);
  g_free (window);
}

static int
set_wm_state (MetaWindow *window,
              int         state)
{
  unsigned long data[2];

  /* twm sets the icon window as data[1], I couldn't find that in
   * ICCCM.
   */
  data[0] = state;
  data[1] = None;

  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_wm_state,
                   window->display->atom_wm_state,
                   32, PropModeReplace, (guchar*) data, 2);
  return meta_error_trap_pop (window->display);
}

static int
set_net_wm_state (MetaWindow *window)
{
  int i;
  unsigned long data[10];
  gboolean skip_pager;
  gboolean skip_taskbar;

  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK ||
      window->type == META_WINDOW_TOOLBAR ||
      window->type == META_WINDOW_MENU)
    skip_pager = TRUE;
  else
    skip_pager = FALSE;

  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK ||
      window->type == META_WINDOW_MENU)
    skip_taskbar = TRUE;
  else
    skip_taskbar = FALSE;
  
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
  if (window->wm_state_skip_pager || skip_pager)
    {
      data[i] = window->display->atom_net_wm_state_skip_pager;
      ++i;
    }
  if (window->wm_state_skip_taskbar || skip_pager)
    {
      data[i] = window->display->atom_net_wm_state_skip_taskbar;
      ++i;
    }
  if (window->maximized)
    {
      data[i] = window->display->atom_net_wm_state_maximized_horz;
      ++i;
      data[i] = window->display->atom_net_wm_state_maximized_vert;
      ++i;
    }

  meta_verbose ("Setting _NET_WM_STATE with %d atoms\n", i);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_state,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) data, i);
  return meta_error_trap_pop (window->display);
}

void
meta_window_calc_showing (MetaWindow  *window)
{
  gboolean on_workspace;

  meta_verbose ("Calc showing for window %s\n", window->desc);
  
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

  if (window->on_all_workspaces)
    {
      on_workspace = TRUE;
      meta_verbose ("Window %s is on all workspaces\n", window->desc);
    }
  
  if (window->minimized || !on_workspace)
    {
      /* Really this effects code should probably
       * be in meta_window_hide so the window->mapped
       * test isn't duplicated here. Anyhow, we animate
       * if we are mapped now, we are supposed to
       * be minimized, and we are on the current workspace.
       */
      if (on_workspace && window->minimized && window->mapped)
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
              icon_rect.x = window->screen->width;
              icon_rect.y = window->screen->height;
              icon_rect.width = 1;
              icon_rect.height = 1;
            }

          meta_window_get_outer_rect (window, &window_rect);
          
          /* Draw a nice cool animation */
          meta_effects_draw_box_animation (window->screen,
                                           &window_rect,
                                           &icon_rect,
                                           META_MINIMIZE_ANIMATION_LENGTH);
	}

      meta_window_hide (window);
    }
  else
    {
      meta_window_show (window);
    }
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

  meta_verbose ("Clearing the calc_showing queue\n");
  
  /* sort them from bottom to top, so we map the
   * bottom windows first, so that placement (e.g. cascading)
   * works properly
   */
  calc_showing_pending = g_slist_sort (calc_showing_pending,
                                       stackcmp);
  
  tmp = calc_showing_pending;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;
      
      meta_window_calc_showing (window);
      window->calc_showing_queued = FALSE;
      
      tmp = tmp->next;
    }

  g_slist_free (calc_showing_pending);
  calc_showing_pending = NULL;
  
  calc_showing_idle = 0;
  return FALSE;
}

void
meta_window_unqueue_calc_showing (MetaWindow *window)
{
  if (!window->calc_showing_queued)
    return;

  meta_verbose ("Removing %s from the calc_showing queue\n",
                window->desc);
  
  calc_showing_pending = g_slist_remove (calc_showing_pending, window);
  window->calc_showing_queued = FALSE;
  
  if (calc_showing_pending == NULL &&
      calc_showing_idle != 0)
    {
      g_source_remove (calc_showing_idle);
      calc_showing_idle = 0;
    }
}

void
meta_window_queue_calc_showing (MetaWindow  *window)
{
  if (window->unmanaging)
    return;

  if (window->calc_showing_queued)
    return;

  meta_verbose ("Putting %s in the calc_showing queue\n",
                window->desc);
  
  window->calc_showing_queued = TRUE;
  
  if (calc_showing_idle == 0)
    calc_showing_idle = g_idle_add (idle_calc_showing, NULL);

  calc_showing_pending = g_slist_prepend (calc_showing_pending, window);
}

void
meta_window_show (MetaWindow *window)
{
  meta_verbose ("Showing window %s, shaded: %d iconic: %d placed: %d\n",
                window->desc, window->shaded, window->iconic, window->placed);

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
    }
  
  /* Shaded means the frame is mapped but the window is not */
  
  if (window->frame && !window->frame->mapped)
    {
      meta_verbose ("Frame actually needs map\n");
      window->frame->mapped = TRUE;
      meta_ui_map_frame (window->screen->ui, window->frame->xwindow);
    }

  if (window->shaded)
    {
      if (window->mapped)
        {
          meta_verbose ("%s actually needs unmap\n", window->desc);
          window->mapped = FALSE;
          window->unmaps_pending += 1;
          meta_error_trap_push (window->display);
          XUnmapWindow (window->display->xdisplay, window->xwindow);
          meta_error_trap_pop (window->display);
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
          meta_verbose ("%s actually needs map\n", window->desc);
          window->mapped = TRUE;
          meta_error_trap_push (window->display);
          XMapWindow (window->display->xdisplay, window->xwindow);
          meta_error_trap_pop (window->display);
        }
      
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
  
  if (window->frame && window->frame->mapped)
    {
      meta_verbose ("Frame actually needs unmap\n");
      window->frame->mapped = FALSE;
      meta_ui_unmap_frame (window->screen->ui, window->frame->xwindow);
    }

  if (window->mapped)
    {
      meta_verbose ("%s actually needs unmap\n", window->desc);
      window->mapped = FALSE;
      window->unmaps_pending += 1;
      meta_error_trap_push (window->display);
      XUnmapWindow (window->display->xdisplay, window->xwindow);
      meta_error_trap_pop (window->display);
    }

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
      window->maximized = TRUE;
      
      /* save size/pos as appropriate args for move_resize */
      window->saved_rect = window->rect;
      if (window->frame)
        {
          window->saved_rect.x += window->frame->rect.x;
          window->saved_rect.y += window->frame->rect.y;
        }
      
      /* move_resize with new maximization constraints
       */
      meta_window_queue_move_resize (window);

      set_net_wm_state (window);
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

      set_net_wm_state (window);
    }
}

void
meta_window_shade (MetaWindow  *window)
{
  meta_verbose ("Shading %s\n", window->desc);
  if (!window->shaded)
    {
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
                                           META_SHADE_ANIMATION_LENGTH);
        }

      window->shaded = TRUE;
      
      meta_window_focus (window, CurrentTime);

      meta_window_queue_move_resize (window);
      meta_window_queue_calc_showing (window);

      set_net_wm_state (window);
    }
}

void
meta_window_unshade (MetaWindow  *window)
{
  meta_verbose ("Unshading %s\n", window->desc);
  if (window->shaded)
    {
      window->shaded = FALSE;
      meta_window_queue_move_resize (window);
      meta_window_queue_calc_showing (window);
      /* focus the window */
      /* FIXME CurrentTime is bogus */
      meta_window_focus (window, CurrentTime);

      set_net_wm_state (window);
    }
}


/* returns values suitable for meta_window_move */
static void
adjust_for_gravity (MetaWindow        *window,
                    MetaFrameGeometry *fgeom,
                    gboolean           coords_assume_border,
                    int                x,
                    int                y,
                    int               *xp,
                    int               *yp)
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
      frame_width = child_x + window->rect.width + fgeom->right_width;
      frame_height = child_y + window->rect.height + fgeom->bottom_height;
    }
  else
    {
      child_x = 0;
      child_y = 0;
      frame_width = window->rect.width;
      frame_height = window->rect.height;
    }
  
  /* We're computing position to pass to window_move, which is
   * the position of the client window (StaticGravity basically)
   *
   * (see WM spec description of gravity computation, but note that
   * their formulas assume we're honoring the border width, rather
   * than compensating for having turned it off)
   */
  switch (window->size_hints.win_gravity)
    {
    case NorthWestGravity:
      ref_x = x;
      ref_y = y;
      break;
    case NorthGravity:
      ref_x = x + window->rect.width / 2 + bw;
      ref_y = y;
      break;
    case NorthEastGravity:
      ref_x = x + window->rect.width + bw * 2;
      ref_y = y;
      break;
    case WestGravity:
      ref_x = x;
      ref_y = y + window->rect.height / 2 + bw;
      break;
    case CenterGravity:
      ref_x = x + window->rect.width / 2 + bw;
      ref_y = y + window->rect.height / 2 + bw;
      break;
    case EastGravity:
      ref_x = x + window->rect.width + bw * 2;
      ref_y = y + window->rect.height / 2 + bw;
      break;
    case SouthWestGravity:
      ref_x = x;
      ref_y = y + window->rect.height + bw * 2;
      break;
    case SouthGravity:
      ref_x = x + window->rect.width / 2 + bw;
      ref_y = y + window->rect.height + bw * 2;
      break;
    case SouthEastGravity:
      ref_x = x + window->rect.width + bw * 2;
      ref_y = y + window->rect.height + bw * 2;
      break;
    case StaticGravity:
    default:
      ref_x = x;
      ref_y = y;
      break;
    }

  switch (window->size_hints.win_gravity)
    {
    case NorthWestGravity:
      *xp = ref_x + child_x;
      *yp = ref_y + child_y;
      break;
    case NorthGravity:
      *xp = ref_x - frame_width / 2 + child_x;
      *yp = ref_y + child_y;
      break;
    case NorthEastGravity:
      *xp = ref_x - frame_width + child_x;
      *yp = ref_y + child_y;
      break;
    case WestGravity:
      *xp = ref_x + child_x;
      *yp = ref_y - frame_height / 2 + child_y;
      break;
    case CenterGravity:
      *xp = ref_x - frame_width / 2 + child_x;
      *yp = ref_y - frame_height / 2 + child_y;
      break;
    case EastGravity:
      *xp = ref_x - frame_width + child_x;
      *yp = ref_y - frame_height / 2 + child_y;
      break;
    case SouthWestGravity:
      *xp = ref_x + child_x;
      *yp = ref_y - frame_height + child_y;
      break;
    case SouthGravity:
      *xp = ref_x - frame_width / 2 + child_x;
      *yp = ref_y - frame_height + child_y;
      break;
    case SouthEastGravity:
      *xp = ref_x - frame_width + child_x;
      *yp = ref_y - frame_height + child_y;
      break;
    case StaticGravity:
    default:
      *xp = ref_x;
      *yp = ref_y;
      break;
    }
}

static void
meta_window_move_resize_internal (MetaWindow  *window,
                                  gboolean     is_configure_request,
                                  /* only relevant if !is_configure_request */
                                  gboolean     do_gravity_adjust,
                                  int          resize_gravity,
                                  int          root_x_nw,
                                  int          root_y_nw,
                                  int          w,
                                  int          h)
{
  XWindowChanges values;
  unsigned int mask;
  gboolean need_configure_notify;
  MetaFrameGeometry fgeom;
  gboolean need_move_client = FALSE;
  gboolean need_move_frame = FALSE;
  gboolean need_resize_client = FALSE;
  gboolean need_resize_frame = FALSE;
  int size_dx;
  int size_dy;
  int pos_dx;
  int pos_dy;
  int frame_size_dx;
  int frame_size_dy;
  
  {
    int oldx, oldy;
    meta_window_get_position (window, &oldx, &oldy);
    meta_verbose ("Move/resize %s to %d,%d %dx%d%s from %d,%d %dx%d\n",
                  window->desc, root_x_nw, root_y_nw, w, h,
                  is_configure_request ? " (configure request)" : "",
                  oldx, oldy, window->rect.width, window->rect.height);
  }  

  if (window->frame)
    meta_frame_calc_geometry (window->frame,
                              &fgeom);
  
  constrain_size (window, &fgeom, w, h, &w, &h);
  meta_verbose ("Constrained resize of %s to %d x %d\n", window->desc, w, h);

  if (w != window->rect.width ||
      h != window->rect.height)
    need_resize_client = TRUE;  

  size_dx = w - window->rect.width;
  size_dy = h - window->rect.height;
  
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

      if (new_w != window->frame->rect.width ||
          new_h != window->frame->rect.height)
        need_resize_frame = TRUE;

      frame_size_dx = new_w - window->frame->rect.width;
      frame_size_dy = new_h - window->frame->rect.height;
      
      window->frame->rect.width = new_w;
      window->frame->rect.height = new_h;

      meta_verbose ("Calculated frame size %dx%d\n",
                    window->frame->rect.width,
                    window->frame->rect.height);
    }
  else
    {
      frame_size_dx = 0;
      frame_size_dy = 0;
    }

  if (is_configure_request || do_gravity_adjust)
    {
      adjust_for_gravity (window,
                          window->frame ? &fgeom : NULL,
                          /* configure request coords assume
                           * the border width existed
                           */
                          is_configure_request,
                          root_x_nw,
                          root_y_nw,
                          &root_x_nw,
                          &root_y_nw);
      
      meta_verbose ("Compensated position for gravity, new pos %d,%d\n",
                    root_x_nw, root_y_nw);
    }

  /* There can be somewhat bogus interactions between gravity
   * and the position constraints (with position contraints
   * basically breaking gravity). Not sure how to fix this.
   */
  
  /* If client is staying fixed on the east during resize, then we
   * have to move the west edge.
   */
  switch (resize_gravity)
    {
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      root_x_nw -= size_dx;
      break;
    default:
      break;
    }

  /* If client is staying fixed on the south during resize,
   * we have to move the north edge
   */
  switch (resize_gravity)
    {
    case SouthGravity:
    case SouthEastGravity:
    case SouthWestGravity:
      root_y_nw -= size_dy;
      break;
    default:
      break;
    }
  
  constrain_position (window,
                      window->frame ? &fgeom : NULL,
                      root_x_nw, root_y_nw,
                      &root_x_nw, &root_y_nw);

  meta_verbose ("Constrained position to %d,%d\n",
                root_x_nw, root_y_nw);
      
  if (window->frame)
    {
      int new_x, new_y;
      
      new_x = root_x_nw - fgeom.left_width;
      new_y = root_y_nw - fgeom.top_height;

      if (new_x != window->frame->rect.x ||
          new_y != window->frame->rect.y)
        need_move_frame = TRUE;

      if (window->rect.x != fgeom.left_width ||
          window->rect.y != fgeom.top_height)
        need_move_client = TRUE;
      
      window->frame->rect.x = new_x;
      window->frame->rect.y = new_y;
      
      /* window->rect.x, window->rect.y are relative to frame,
       * remember they are the server coords
       */
      pos_dx = fgeom.left_width - window->rect.x;
      pos_dy = fgeom.top_height - window->rect.y;
      
      window->rect.x = fgeom.left_width;
      window->rect.y = fgeom.top_height;
    }
  else
    {
      if (root_x_nw != window->rect.x ||
          root_y_nw != window->rect.y)
        need_move_client = TRUE;

      pos_dx = root_x_nw - window->rect.x;
      pos_dy = root_y_nw - window->rect.y;
      
      window->rect.x = root_x_nw;
      window->rect.y = root_y_nw;
    }

  /* Fill in other frame member variables */
  if (window->frame)
    {
      window->frame->child_x = fgeom.left_width;
      window->frame->child_y = fgeom.top_height;
      window->frame->right_width = fgeom.right_width;
      window->frame->bottom_height = fgeom.bottom_height;
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
  
  values.border_width = 0;
  values.x = window->rect.x;
  values.y = window->rect.y;
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
        meta_verbose ("Syncing new client geometry %d,%d %dx%d, border: %s pos: %s size: %s\n",
                      newx, newy,
                      window->rect.width, window->rect.height,
                      mask & CWBorderWidth ? "true" : "false",
                      need_move_client ? "true" : "false",
                      need_resize_client ? "true" : "false");
      }
      
      meta_error_trap_push (window->display);
      XConfigureWindow (window->display->xdisplay,
                        window->xwindow,
                        mask,
                        &values);
      meta_error_trap_pop (window->display);
    }

  /* Now do the frame */
  if (window->frame)
    {
      meta_frame_sync_to_window (window->frame, need_move_frame, need_resize_frame);
    }
  
  if (need_configure_notify)
    send_configure_notify (window);

  /* Invariants leaving this function are:
   *   a) window->rect and frame->rect reflect the actual
   *      server-side size/pos of window->xwindow and frame->xwindow
   *   b) all constraints are obeyed by window->rect and frame->rect
   */
}

void
meta_window_resize (MetaWindow  *window,
                    int          w,
                    int          h)
{
  int x, y;

  meta_window_get_position (window, &x, &y);
  
  meta_window_move_resize_internal (window, FALSE, FALSE,
                                    NorthWestGravity,
                                    x, y, w, h);
}

void
meta_window_move (MetaWindow  *window,
                  int          root_x_nw,
                  int          root_y_nw)
{
  meta_window_move_resize_internal (window, FALSE, FALSE,
                                    NorthWestGravity,
                                    root_x_nw, root_y_nw,
                                    window->rect.width,
                                    window->rect.height);
}

void
meta_window_move_resize (MetaWindow  *window,
                         int          root_x_nw,
                         int          root_y_nw,
                         int          w,
                         int          h)
{
  meta_window_move_resize_internal (window, FALSE, FALSE,
                                    NorthWestGravity,
                                    root_x_nw, root_y_nw,
                                    w, h);
}

void
meta_window_resize_with_gravity (MetaWindow *window,
                                 int          w,
                                 int          h,
                                 int          gravity)
{
  int x, y;

  meta_window_get_position (window, &x, &y);
  
  meta_window_move_resize_internal (window, FALSE, FALSE,
                                    gravity,
                                    x, y, w, h);
}

void
meta_window_move_resize_now (MetaWindow  *window)
{
  int x, y;
  
  meta_window_get_position (window, &x, &y);
  
  meta_window_move_resize (window, x, y,
                           window->rect.width, window->rect.height);
}

void
meta_window_queue_move_resize (MetaWindow  *window)
{
  /* FIXME actually queue, don't do it immediately */
  meta_window_move_resize_now (window);
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
meta_window_get_outer_rect (MetaWindow    *window,
                            MetaRectangle *rect)
{
  if (window->frame)
    *rect = window->frame->rect;
  else
    *rect = window->rect;
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

  if (window->display->grab_window &&
      window->display->grab_window->all_keys_grabbed)
    {
      meta_verbose ("Current focus window %s has global keygrab, not focusing window %s after all\n",
                    window->display->grab_window->desc, window->desc);
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
          meta_verbose ("Focusing frame of %s\n", window->desc);
          XSetInputFocus (window->display->xdisplay,
                          window->frame->xwindow,
                          RevertToPointerRoot,
                          CurrentTime);
        }
    }
  else
    {
      meta_error_trap_push (window->display);
      
      if (window->input)
        {
          XSetInputFocus (window->display->xdisplay,
                          window->xwindow,
                          RevertToPointerRoot,
                          timestamp);
        }
      
      if (window->take_focus)
        {
          meta_window_send_icccm_message (window,
                                          window->display->atom_wm_take_focus,
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
      meta_verbose ("%s already on this workspace\n", window->desc);
      return;
    }

  /* Add first, to maintain invariant that we're always
   * on some workspace.
   */
  meta_workspace_add_window (workspace, window);

  /* unstick if stuck */
  if (window->on_all_workspaces)
    window->on_all_workspaces = FALSE;
  
  /* Lamely rely on prepend */
  g_assert (window->workspaces->data == workspace);  
  
  /* Remove from all other spaces */
  while (window->workspaces->next) /* while list size > 1 */
    meta_workspace_remove_window (window->workspaces->next->data, window);
}

void
meta_window_stick (MetaWindow  *window)
{
  if (window->on_all_workspaces)
    return;

  /* We don't change window->workspaces, because we revert
   * to that original workspace list if on_all_workspaces is
   * toggled back off.
   */
  window->on_all_workspaces = TRUE;

  meta_window_set_current_workspace_hint (window);
  
  meta_window_queue_calc_showing (window);
}

void
meta_window_unstick (MetaWindow  *window)
{
  if (!window->on_all_workspaces)
    return;

  /* Revert to window->workspaces */

  window->on_all_workspaces = FALSE;

  /* We change ourselves to the active workspace, since otherwise you'd get
   * a weird window-vaporization effect. Once we have UI for being
   * on more than one workspace this should probably be add_workspace
   * not change_workspace.
   */
  if (!meta_workspace_contains_window (window->screen->active_workspace,
                                       window))
    meta_window_change_workspace (window, window->screen->active_workspace);
  
  meta_window_set_current_workspace_hint (window);
  
  meta_window_queue_calc_showing (window);
}

unsigned long
meta_window_get_net_wm_desktop (MetaWindow *window)
{
  if (window->on_all_workspaces ||
      g_list_length (window->workspaces) > 1)
    return 0xFFFFFFFF;
  else
    return meta_workspace_screen_index (window->workspaces->data);
}

int
meta_window_set_current_workspace_hint (MetaWindow *window)
{
  /* FIXME if on more than one workspace, we claim to be "sticky",
   * the WM spec doesn't say what to do here.
   */
  unsigned long data[1];

  if (window->workspaces == NULL)
    return Success; /* this happens when destroying windows */
  
  data[0] = meta_window_get_net_wm_desktop (window);

  meta_verbose ("Setting _NET_WM_DESKTOP of %s to %ld\n",
                window->desc, data[0]);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_desktop,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  return meta_error_trap_pop (window->display);
}

void
meta_window_raise (MetaWindow  *window)
{
  meta_verbose ("Raising window %s\n", window->desc);

  meta_stack_raise (window->screen->stack, window);
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
  int x, y, width, height;

  /* it's essential to use only the explicitly-set fields,
   * and otherwise use our current up-to-date position.
   *
   * Otherwise you get spurious position changes when the app changes
   * size, for example, if window->rect is not in sync with the
   * server-side position in effect when the configure request was
   * generated.
   */

  meta_window_get_gravity_position (window, &x, &y);

  if (((window->type == META_WINDOW_DESKTOP ||
        window->type == META_WINDOW_DOCK ||
        window->type == META_WINDOW_TOOLBAR ||
        window->type == META_WINDOW_MENU) &&
       (window->size_hints.flags & PPosition)) ||
      /* This is here exactly until some crap app annoys me
       * by misusing it. ;-) Then I remove it and only honor
       * USPosition at map time.
       */
      (window->size_hints.flags & USPosition))
    {
      if (event->xconfigurerequest.value_mask & CWX)
        x = event->xconfigurerequest.x;
      
      if (event->xconfigurerequest.value_mask & CWY)
        y = event->xconfigurerequest.y;
    }

  width = window->rect.width;
  height = window->rect.height;
  
  if (event->xconfigurerequest.value_mask & CWWidth)
    width = event->xconfigurerequest.width;

  if (event->xconfigurerequest.value_mask & CWHeight)
    height = event->xconfigurerequest.height;

  /* ICCCM 4.1.5 */
  
  /* Note that x, y is the corner of the window border,
   * and width, height is the size of the window inside
   * its border, but that we always deny border requests
   * and give windows a border of 0. But we save the
   * requested border here.
   */
  window->border_width = event->xconfigurerequest.border_width;

  /* We're ignoring the value_mask here, since sizes
   * not in the mask will be the current window geometry.
   */
  
  window->size_hints.x = x;
  window->size_hints.y = y;
  window->size_hints.width = width;
  window->size_hints.height = height;

  meta_window_move_resize_internal (window, TRUE, FALSE,
                                    NorthWestGravity,
                                    window->size_hints.x,
                                    window->size_hints.y,
                                    window->size_hints.width,
                                    window->size_hints.height);

  return TRUE;
}

gboolean
meta_window_property_notify (MetaWindow *window,
                             XEvent     *event)
{
  return process_property_notify (window, &event->xproperty);  
}

gboolean
meta_window_client_message (MetaWindow *window,
                            XEvent     *event)
{
  MetaDisplay *display;

  display = window->display;
  
  if (event->xclient.message_type ==
      display->atom_net_close_window)
    {
      /* I think the wm spec should maybe put a time
       * in this message, CurrentTime here is sort of
       * bogus. But it rarely matters most likely.
       */
      meta_window_delete (window, CurrentTime);

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_wm_desktop)
    {
      int space;
      MetaWorkspace *workspace;
              
      space = event->xclient.data.l[0];
              
      meta_verbose ("Request to move %s to screen workspace %d\n",
                    window->desc, space);

      workspace =
        meta_display_get_workspace_by_screen_index (display,
                                                    window->screen,
                                                    space);

      if (workspace)
        {
          if (window->on_all_workspaces)
            meta_window_unstick (window);
          meta_window_change_workspace (window, workspace);
        }
      else if (space == 0xFFFFFFFF)
        {
          meta_window_stick (window);
        }
      else
        {
          meta_verbose ("No such workspace %d for screen\n", space);
        }

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

          meta_error_trap_push (display);
          str1 = XGetAtomName (display->xdisplay, first);
          if (meta_error_trap_pop (display))
            str1 = NULL;

          meta_error_trap_push (display);
          str2 = XGetAtomName (display->xdisplay, second); 
          if (meta_error_trap_pop (display))
            str2 = NULL;
          
          meta_verbose ("Request to change _NET_WM_STATE action %ld atom1: %s atom2: %s\n",
                        action,
                        str1 ? str1 : "(unknown)",
                        str2 ? str2 : "(unknown)");

          if (str1)
            XFree (str1);
          if (str2)
            XFree (str2);
        }

      if (first == display->atom_net_wm_state_shaded ||
          second == display->atom_net_wm_state_shaded)
        {
          gboolean shade;

          shade = (action == _NET_WM_STATE_ADD ||
                   (action == _NET_WM_STATE_TOGGLE && !window->shaded));
          if (shade)
            meta_window_shade (window);
          else
            meta_window_unshade (window);
        }

      if (first == display->atom_net_wm_state_maximized_horz ||
          second == display->atom_net_wm_state_maximized_horz ||
          first == display->atom_net_wm_state_maximized_vert ||
          second == display->atom_net_wm_state_maximized_vert)
        {
          gboolean max;

          max = (action == _NET_WM_STATE_ADD ||
                 (action == _NET_WM_STATE_TOGGLE && !window->maximized));
          if (max)
            meta_window_maximize (window);
          else
            meta_window_unmaximize (window);
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
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_skip_pager);

          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_skip_taskbar ||
          second == display->atom_net_wm_state_skip_taskbar)
        {
          window->wm_state_skip_taskbar =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_skip_taskbar);

          set_net_wm_state (window);
        }
      
      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_wm_change_state)
    {
      meta_verbose ("WM_CHANGE_STATE client message, state: %ld\n",
                    event->xclient.data.l[0]);
      if (event->xclient.data.l[0] == IconicState)
        meta_window_minimize (window);

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

  /* We don't ever want to set prev_focus_window to NULL,
   * though it may be NULL due to e.g. only one window ever
   * getting focus, or a window disappearing.
   */

  /* We ignore grabs, though this is questionable.
   * It may be better to increase the intelligence of
   * the focus window tracking.
   *
   * The problem is that keybindings for windows are done
   * with XGrabKey, which means focus_window disappears
   * and prev_focus_window gets confused from what the
   * user expects once a keybinding is used.
   */
  if ((event->type == FocusIn ||
       event->type == FocusOut) &&
      (event->xfocus.mode == NotifyGrab ||
       event->xfocus.mode == NotifyUngrab))
    {
      meta_verbose ("Ignoring focus event generated by a grab\n");
      return TRUE;
    }
    
  if (event->type == FocusIn)
    {
      if (window != window->display->focus_window)
        {
          if (window == window->display->prev_focus_window &&
              window->display->focus_window != NULL)
            {
              meta_verbose ("%s is now the previous focus window due to another window focused in\n",
                            window->display->focus_window->desc);
              window->display->prev_focus_window = window->display->focus_window;
            }
          meta_verbose ("New focus window %s\n", window->desc);
          window->display->focus_window = window;
        }
      window->has_focus = TRUE;
      if (window->frame)
        meta_frame_queue_draw (window->frame);
    }
  else if (event->type == FocusOut ||
           event->type == UnmapNotify)
    {
      if (window == window->display->focus_window)
        {
          meta_verbose ("%s is now the previous focus window due to being focused out or unmapped\n",
                        window->desc);
          window->display->prev_focus_window = window;

          window->display->focus_window = NULL;
        }
      window->has_focus = FALSE;
      if (window->frame)
        meta_frame_queue_draw (window->frame);
    }

  return FALSE;
}

static gboolean
process_property_notify (MetaWindow     *window,
                         XPropertyEvent *event)
{
  if (event->atom == XA_WM_NAME ||
      event->atom == window->display->atom_net_wm_name)
    {
      meta_verbose ("Property notify on %s for WM_NAME or NET_WM_NAME\n", window->desc);
      update_title (window);
    }
  else if (event->atom == XA_WM_NORMAL_HINTS)
    {
      meta_verbose ("Property notify on %s for WM_NORMAL_HINTS\n", window->desc);
      
      update_size_hints (window);
      
      /* See if we need to constrain current size */
      meta_window_queue_move_resize (window);
    }
  else if (event->atom == window->display->atom_wm_protocols)
    {
      meta_verbose ("Property notify on %s for WM_PROTOCOLS\n", window->desc);
      
      update_protocols (window);

      meta_window_queue_move_resize (window);
    }
  else if (event->atom == XA_WM_HINTS)
    {
      meta_verbose ("Property notify on %s for WM_HINTS\n", window->desc);
      
      update_wm_hints (window);
      update_icon (window);
      
      meta_window_queue_move_resize (window);
    }
  else if (event->atom == window->display->atom_motif_wm_hints)
    {
      meta_verbose ("Property notify on %s for MOTIF_WM_HINTS\n", window->desc);
      
      update_mwm_hints (window);
      
      if (window->decorated)
        meta_window_ensure_frame (window);
      else
        meta_window_destroy_frame (window);

      meta_window_queue_move_resize (window);
    }
  else if (event->atom == XA_WM_CLASS)
    {
      meta_verbose ("Property notify on %s for WM_CLASS\n", window->desc);
      
      update_wm_class (window);
    }
  else if (event->atom == XA_WM_TRANSIENT_FOR)
    {
      meta_verbose ("Property notify on %s for WM_TRANSIENT_FOR\n", window->desc);
      
      update_transient_for (window);

      meta_window_queue_move_resize (window);
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
           window->display->atom_net_wm_window_type ||
           /* update_net_wm_type falls back to this */
           event->atom == window->display->atom_win_layer)
    {
      meta_verbose ("Property notify on %s for NET_WM_WINDOW_TYPE or WIN_LAYER\n", window->desc);
      update_net_wm_type (window);
    }
  else if (event->atom ==
           window->display->atom_net_wm_icon_name ||
           event->atom == XA_WM_ICON_NAME)
    {
      meta_verbose ("Property notify on %s for NET_WM_ICON_NAME or WM_ICON_NAME\n", window->desc);
      
      update_icon_name (window);
    }
  else if (event->atom == window->display->atom_net_wm_icon)
    {
      meta_verbose ("Property notify on %s for NET_WM_ICON\n", window->desc);
      update_icon (window);
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
  XSendEvent (window->display->xdisplay,
              window->xwindow,
              False, StructureNotifyMask, &event);
  meta_error_trap_pop (window->display);
}

static int
update_size_hints (MetaWindow *window)
{
  int x, y, w, h;
  gulong supplied;

  meta_verbose ("Updating WM_NORMAL_HINTS\n");
  
  /* Save the last ConfigureRequest, which we put here.
   * Values here set in the hints are supposed to
   * be ignored.
   */
  x = window->size_hints.x;
  y = window->size_hints.y;
  w = window->size_hints.width;
  h = window->size_hints.height;
  
  window->size_hints.flags = 0;
  supplied = 0;
  
  meta_error_trap_push (window->display);
  XGetWMNormalHints (window->display->xdisplay,
                     window->xwindow,
                     &window->size_hints,
                     &supplied);

  /* as far as I can tell, "supplied" is just
   * to check whether we had old-style normal hints
   * without gravity, base size as returned by
   * XGetNormalHints()
   */
  
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

  if (window->size_hints.max_width < window->size_hints.min_width)
    {
      /* someone is on crack */
      meta_warning (_("Window %s sets max width %d less than min width %d, disabling resize\n"),
                    window->desc,
                    window->size_hints.max_width,
                    window->size_hints.min_width);
      window->size_hints.max_width = window->size_hints.min_width;
    }

  if (window->size_hints.max_height < window->size_hints.min_height)
    {
      /* another cracksmoker */
      meta_warning (_("Window %s sets max height %d less than min height %d, disabling resize\n"),
                    window->desc,
                    window->size_hints.max_height,
                    window->size_hints.min_height);
      window->size_hints.max_height = window->size_hints.min_height;
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
      meta_verbose ("Window %s doesn't set gravity, using NW\n",
                    window->desc);
      window->size_hints.win_gravity = NorthWestGravity;
      window->size_hints.flags |= PWinGravity;
    }

  recalc_window_features (window);
  
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
              meta_warning ("WM_NAME property for %s contained stuff window manager is too dumb to figure out: %s\n", window->desc, err->message);
              g_error_free (err);
            }

          if (str)
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

  if (window->frame)
    meta_ui_set_frame_title (window->screen->ui,
                             window->frame->xwindow,
                             window->title);
  
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
  window->xgroup_leader = None;
  window->icon_pixmap = None;
  window->icon_mask = None;
  
  meta_error_trap_push (window->display);
  
  hints = XGetWMHints (window->display->xdisplay,
                       window->xwindow);
  if (hints)
    {
      window->input = (hints->flags & InputHint) != 0;

      if (hints->flags & StateHint)
        window->initially_iconic = (hints->initial_state == IconicState);

      if (hints->flags & WindowGroupHint)
        window->xgroup_leader = hints->window_group;

      if (hints->flags & IconPixmapHint)
        window->icon_pixmap = hints->icon_pixmap;

      if (hints->flags & IconMaskHint)
        window->icon_mask = hints->icon_mask;
      
      meta_verbose ("Read WM_HINTS input: %d iconic: %d group leader: 0x%ld\n",
                    window->input, window->initially_iconic,
                    window->xgroup_leader);
      
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
  int format;
  gulong n_atoms;
  gulong bytes_after;
  Atom *atoms;
  int result;
  int i;

  window->shaded = FALSE;
  window->maximized = FALSE;
  window->wm_state_modal = FALSE;
  
  meta_error_trap_push (window->display);
  XGetWindowProperty (window->display->xdisplay, window->xwindow,
		      window->display->atom_net_wm_state,
                      0, G_MAXLONG,
		      False, XA_ATOM, &type, &format, &n_atoms,
		      &bytes_after, (guchar **)&atoms);  

  result = meta_error_trap_pop (window->display);
  if (result != Success)
    {
      recalc_window_type (window);
      return result;
    }
    
  if (type != XA_ATOM)
    {
      recalc_window_type (window);
      return -1; /* whatever */
    }

  i = 0;
  while (i < n_atoms)
    {
      if (atoms[i] == window->display->atom_net_wm_state_shaded)
        window->shaded = TRUE;
      else if (atoms[i] == window->display->atom_net_wm_state_maximized_horz)
        window->maximized = TRUE;
      else if (atoms[i] == window->display->atom_net_wm_state_maximized_vert)
        window->maximized = TRUE;
      else if (atoms[i] == window->display->atom_net_wm_state_modal)
        window->wm_state_modal = TRUE;
      
      ++i;
    }
  
  XFree (atoms);

  recalc_window_type (window);
  
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
  int format;
  gulong nitems;
  gulong bytes_after;
  int result;

  window->mwm_decorated = TRUE;
  window->mwm_has_close_func = TRUE;
  window->mwm_has_minimize_func = TRUE;
  window->mwm_has_maximize_func = TRUE;
  window->mwm_has_move_func = TRUE;
  window->mwm_has_resize_func = TRUE;
  
  meta_error_trap_push (window->display);
  XGetWindowProperty (window->display->xdisplay, window->xwindow,
		      window->display->atom_motif_wm_hints,
                      0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, (guchar **)&hints);

  result = meta_error_trap_pop (window->display);

  if (result != Success ||
      type == None)
    {
      meta_verbose ("Window %s has no MWM hints\n", window->desc);
      /* may be Success, unused anyhow */
      return result;
    }
  
  /* We support MWM hints deemed non-stupid */

  meta_verbose ("Window %s has MWM hints\n",
                window->desc);
  
  if (hints->flags & MWM_HINTS_DECORATIONS)
    {
      meta_verbose ("Window %s sets MWM_HINTS_DECORATIONS 0x%lx\n",
                    window->desc, hints->decorations);

      if (hints->decorations == 0)
        window->mwm_decorated = FALSE;
    }
  else
    meta_verbose ("Decorations flag unset\n");
  
  if (hints->flags & MWM_HINTS_FUNCTIONS)
    {
      gboolean toggle_value;
      
      meta_verbose ("Window %s sets MWM_HINTS_FUNCTIONS 0x%lx\n",
                    window->desc, hints->functions);

      /* If _ALL is specified, then other flags indicate what to turn off;
       * if ALL is not specified, flags are what to turn on.
       * at least, I think so
       */
      
      if ((hints->flags & MWM_FUNC_ALL) == 0)
        {
          toggle_value = TRUE;

          meta_verbose ("Window %s disables all funcs then reenables some\n",
                        window->desc);
          window->mwm_has_close_func = FALSE;
          window->mwm_has_minimize_func = FALSE;
          window->mwm_has_maximize_func = FALSE;
          window->mwm_has_move_func = FALSE;
          window->mwm_has_resize_func = FALSE;
        }
      else
        {
          meta_verbose ("Window %s enables all funcs then disables some\n",
                        window->desc);
          toggle_value = FALSE;
        }
      
      if ((hints->functions & MWM_FUNC_CLOSE) == 0)
        {
          meta_verbose ("Window %s toggles close via MWM hints\n",
                        window->desc);
          window->mwm_has_close_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MINIMIZE) == 0)
        {
          meta_verbose ("Window %s toggles minimize via MWM hints\n",
                        window->desc);
          window->mwm_has_minimize_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MAXIMIZE) == 0)
        {
          meta_verbose ("Window %s toggles maximize via MWM hints\n",
                        window->desc);
          window->mwm_has_maximize_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MOVE) == 0)
        {
          meta_verbose ("Window %s toggles move via MWM hints\n",
                        window->desc);
          window->mwm_has_move_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_RESIZE) == 0)
        {
          meta_verbose ("Window %s toggles resize via MWM hints\n",
                        window->desc);
          window->mwm_has_resize_func = toggle_value;
        }
    }
  else
    meta_verbose ("Functions flag unset\n");

  XFree (hints);

  recalc_window_features (window);
  
  return Success;
}

static gboolean
meta_window_get_icon_geometry (MetaWindow    *window,
                               MetaRectangle *rect)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *geometry;
  int result;
  
  meta_error_trap_push (window->display);
  type = None;
  XGetWindowProperty (window->display->xdisplay,
		      window->xwindow,
		      window->display->atom_net_wm_icon_geometry,
		      0, G_MAXLONG,
		      False, XA_CARDINAL, &type, &format, &nitems,
		      &bytes_after, ((guchar **)&geometry));
  
  result = meta_error_trap_pop (window->display);
  
  if (result != Success || type != XA_CARDINAL || nitems != 4)
    return FALSE;  
  
  if (rect)
    {
      rect->x = geometry[0];
      rect->y = geometry[1];
      rect->width = geometry[2];
      rect->height = geometry[3];
    }

  XFree (geometry);

  return TRUE;
}

static int
update_wm_class (MetaWindow *window)
{
  XClassHint ch;
  
  if (window->res_class)
    g_free (window->res_class);
  if (window->res_name)
    g_free (window->res_name);

  window->res_class = NULL;
  window->res_name = NULL;

  meta_error_trap_push (window->display);

  ch.res_name = NULL;
  ch.res_class = NULL;

  XGetClassHint (window->display->xdisplay,
                 window->xwindow,
                 &ch);

  if (ch.res_name)
    {
      window->res_name = g_strdup (ch.res_name);
      XFree (ch.res_name);
    }

  if (ch.res_class)
    {
      window->res_class = g_strdup (ch.res_class);
      XFree (ch.res_class);
    }

  meta_verbose ("Window %s class: '%s' name: '%s'\n",
                window->desc,
                window->res_class ? window->res_class : "(null)",
                window->res_name ? window->res_name : "(null)");
  
  return meta_error_trap_pop (window->display);
}

static int
read_string_prop (MetaDisplay *display,
                  Window       xwindow,
                  Atom         atom,
                  char       **strp)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *str;
  int result;  
  
  meta_error_trap_push (display);
  str = NULL;
  XGetWindowProperty (display->xdisplay,
                      xwindow, atom,
                      0, G_MAXLONG,
		      False, XA_STRING, &type, &format, &nitems,
		      &bytes_after, (guchar **)&str);  

  result = meta_error_trap_pop (display);
  if (result != Success)
    return result;
  
  if (type != XA_STRING)
    return -1; /* whatever */

  *strp = g_strdup (str);
  
  XFree (str);
  
  return Success;
}

static Window
read_client_leader (MetaDisplay *display,
                    Window       xwindow)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *leader;
  int result;
  Window retval;
  
  meta_error_trap_push (display);
  leader = NULL;
  XGetWindowProperty (display->xdisplay, xwindow,
		      display->atom_wm_client_leader,
                      0, G_MAXLONG,
		      False, XA_WINDOW, &type, &format, &nitems,
		      &bytes_after, (guchar **)&leader);  

  result = meta_error_trap_pop (display);
  if (result != Success)
    return None;
  
  if (type != XA_WINDOW)
    return None;

  retval = *leader;
  
  XFree (leader);

  return retval;
}

static void
update_sm_hints (MetaWindow *window)
{
  MetaWindow *w;
  Window leader;
  
  window->xclient_leader = None;
  window->sm_client_id = NULL;

  /* If not on the current window, we can get the client
   * leader from transient parents. If we find a client
   * leader, we read the SM_CLIENT_ID from it.
   */
  leader = None;
  w = window;
  while (w != NULL)
    {
      leader = read_client_leader (window->display, w->xwindow);

      if (leader != None)
        break;
      
      if (w->xtransient_for == None)
        break;

      w = meta_display_lookup_x_window (w->display, w->xtransient_for);

      if (w == window)
        break; /* Cute, someone thought they'd make a transient_for cycle */
    }
      
  if (leader)
    {
      window->xclient_leader = leader;
      read_string_prop (window->display, leader,
                        window->display->atom_sm_client_id,
                        &window->sm_client_id);

      meta_verbose ("Window %s client leader: 0x%lx SM_CLIENT_ID: '%s'\n",
                    window->desc, window->xclient_leader, window->sm_client_id);
    }
  else
    meta_verbose ("Didn't find a client leader for %s\n", window->desc);
}

static int
update_role (MetaWindow *window)
{
  int result;
  
  if (window->role)
    g_free (window->role);
  window->role = NULL;

  result = read_string_prop (window->display, window->xwindow,
                             window->display->atom_wm_window_role,
                             &window->role);

  meta_verbose ("Updated role of %s to '%s'\n",
                window->desc, window->role ? window->role : "(null)");
  
  return Success;
}

static int
update_transient_for (MetaWindow *window)
{
  Window w;

  meta_error_trap_push (window->display);
  w = None;
  XGetTransientForHint (window->display->xdisplay,
                        window->xwindow,
                        &w);
  window->xtransient_for = w;

  if (window->xtransient_for != None)
    meta_verbose ("Window %s transient for 0x%lx\n", window->desc,
                  window->xtransient_for);
  else
    meta_verbose ("Window %s is not transient\n", window->desc);

  /* may now be a dialog */
  recalc_window_type (window);

  /* update stacking constraints */
  meta_stack_update_transient (window->screen->stack, window);
  
  return meta_error_trap_pop (window->display);
}


static gboolean
get_cardinal (MetaDisplay *display,
              Window      xwindow,
              Atom        atom,
              gulong     *val)
{  
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int err;
  
  meta_error_trap_push (display);
  type = None;
  XGetWindowProperty (display->xdisplay,
                      xwindow,
                      atom,
                      0, G_MAXLONG,
		      False, XA_CARDINAL, &type, &format, &nitems,
		      &bytes_after, (guchar **)&num);  
  err = meta_error_trap_pop (display);
  if (err != Success)
    return FALSE;
  
  if (type != XA_CARDINAL)
    return FALSE;

  *val = *num;
  
  XFree (num);

  return TRUE;
}

/* some legacy cruft */
typedef enum
{
  WIN_LAYER_DESKTOP     = 0,
  WIN_LAYER_BELOW       = 2,
  WIN_LAYER_NORMAL      = 4,
  WIN_LAYER_ONTOP       = 6,
  WIN_LAYER_DOCK        = 8,
  WIN_LAYER_ABOVE_DOCK  = 10
} GnomeWinLayer;

static int
update_net_wm_type (MetaWindow *window)
{
  Atom type;
  int format;
  gulong n_atoms;
  gulong bytes_after;
  Atom *atoms;
  int result;
  int i;

  window->type_atom = None;
  
  meta_error_trap_push (window->display);
  XGetWindowProperty (window->display->xdisplay, window->xwindow,
		      window->display->atom_net_wm_window_type,
                      0, G_MAXLONG,
		      False, XA_ATOM, &type, &format, &n_atoms,
		      &bytes_after, (guchar **)&atoms);  

  result = meta_error_trap_pop (window->display);
  if (result != Success ||
      type != XA_ATOM)
    {
      /* Fall back to WIN_LAYER */
      gulong layer = WIN_LAYER_NORMAL;

      if (get_cardinal (window->display,
                        window->xwindow,
                        window->display->atom_win_layer,
                        &layer))
        {
          meta_verbose ("%s falling back to _WIN_LAYER hint, layer %ld\n",
                        window->desc, layer);
          switch (layer)
            {
            case WIN_LAYER_DESKTOP:
              window->type_atom =
                window->display->atom_net_wm_window_type_desktop;
              break;
            case WIN_LAYER_NORMAL:
              window->type_atom =
                window->display->atom_net_wm_window_type_normal;
              break;
            case WIN_LAYER_DOCK:
              window->type_atom =
                window->display->atom_net_wm_window_type_dock;
              break;
            default:
              break;
            }
        }
      
      recalc_window_type (window);
      return result;
    }

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
          atoms[i] == window->display->atom_net_wm_window_type_normal)
        {
          window->type_atom = atoms[i];
          break;
        }
      
      ++i;
    }
  
  XFree (atoms);

  if (meta_is_verbose ())
    {
      char *str;

      meta_error_trap_push (window->display);
      str = XGetAtomName (window->display->xdisplay, window->type_atom);
      if (meta_error_trap_pop (window->display))
        str = NULL;

      meta_verbose ("Window %s type atom %s\n", window->desc,
                    str ? str : "(none)");

      if (str)
        XFree (str);
    }
  
  recalc_window_type (window);
  return Success;
}

static int
update_initial_workspace (MetaWindow *window)
{
  gulong val = 0;

  window->initial_workspace_set = FALSE;
  
  /* Fall back to old WM spec hint if net_wm_desktop is missing, this
   * is just to be nice when restarting from old Sawfish basically,
   * should nuke it eventually
   */  
  if (get_cardinal (window->display,
                    window->xwindow,
                    window->display->atom_net_wm_desktop,
                    &val))
    {
      window->initial_workspace_set = TRUE;
      window->initial_workspace = val;
    }
  else if (get_cardinal (window->display,
                         window->xwindow,
                         window->display->atom_win_workspace,
                         &val))
    {
      window->initial_workspace_set = TRUE;
      window->initial_workspace = val;
    }

  return Success;
}

static int
update_icon_name (MetaWindow *window)
{
  XTextProperty text;

  meta_error_trap_push (window->display);
  
  if (window->icon_name)
    {
      g_free (window->icon_name);
      window->icon_name = NULL;
    }  

  XGetTextProperty (window->display->xdisplay,
                    window->xwindow,
                    &text,
                    window->display->atom_net_wm_icon_name);

  if (text.nitems > 0 &&
      text.format == 8 && 
      g_utf8_validate (text.value, text.nitems, NULL))
    {
      meta_verbose ("Using _NET_WM_ICON_NAME for new title of %s: '%s'\n",
                    window->desc, text.value);

      window->icon_name = g_strdup (text.value);
    }

  if (text.nitems > 0)
    XFree (text.value);
  
  if (window->icon_name == NULL &&
      text.nitems > 0)
    meta_warning ("_NET_WM_ICON_NAME property for %s contained invalid UTF-8\n",
                  window->desc);

  if (window->icon_name == NULL)
    {
      XGetTextProperty (window->display->xdisplay,
                        window->xwindow,
                        &text,
                        XA_WM_ICON_NAME);

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
              meta_warning ("WM_ICON_NAME property for %s contained stuff we are too dumb to figure out: %s\n", window->desc, err->message);
              g_error_free (err);
            }

          if (str)
            meta_verbose ("Using WM_ICON_NAME for new title of %s: '%s'\n",
                          window->desc, text.value);

          window->icon_name = str;

          XFree (text.value);
        }
    }
  
  if (window->icon_name == NULL)
    window->icon_name = g_strdup ("");
  
  return meta_error_trap_pop (window->display);
}

static int
update_icon (MetaWindow *window)
{
  meta_error_trap_push (window->display);
  
  if (window->icon)
    {
      g_object_unref (G_OBJECT (window->icon));
      window->icon = NULL;
    }  
  
  /* FIXME */
  
  return meta_error_trap_pop (window->display);
}

static void
recalc_window_type (MetaWindow *window)
{
  int old_type;

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

  meta_verbose ("Calculated type %d for %s, old type %d\n",
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
      meta_stack_update_layer (window->screen->stack, window);
    }
}

static void
recalc_window_features (MetaWindow *window)
{
  /* Use MWM hints initially */
  window->decorated = window->mwm_decorated;
  window->has_close_func = window->mwm_has_close_func;
  window->has_minimize_func = window->mwm_has_minimize_func;
  window->has_maximize_func = window->mwm_has_maximize_func;
  window->has_move_func = window->mwm_has_move_func;
  window->has_resize_func = window->mwm_has_resize_func;

  window->has_shade_func = TRUE;

  /* Semantic category overrides the MWM hints */
  
  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK)
    {
      window->decorated = FALSE;
      window->has_close_func = FALSE;
      window->has_shade_func = FALSE;
      window->has_move_func = FALSE;
      window->has_resize_func = FALSE;
    }

  if (window->type != META_WINDOW_NORMAL)
    {
      window->has_minimize_func = FALSE;
      window->has_maximize_func = FALSE;
    }

  /* If min_size == max_size, then don't allow resize */
  if (window->size_hints.min_width == window->size_hints.max_width &&
      window->size_hints.min_height == window->size_hints.max_height)
    window->has_resize_func = FALSE;
  
  /* don't allow maximize if we can't resize */
  if (!window->has_resize_func)
    window->has_maximize_func = FALSE;

  /* no shading if not decorated */
  if (!window->decorated)
    window->has_shade_func = FALSE;
  
  /* FIXME perhaps should ensure if we don't have a shade func,
   * we aren't shaded, etc.
   */
}

static void
constrain_size (MetaWindow *window,
                MetaFrameGeometry *fgeom,
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

  /* frame member variables should NEVER be used in here */
  
#define FLOOR(value, base)	( ((int) ((value) / (base))) * (base) )
  
  /* Get the allowed size ranges, considering maximized, etc. */
  fullw = window->screen->active_workspace->workarea.width;
  fullh = window->screen->active_workspace->workarea.height;
  if (window->frame)
    {
      fullw -= fgeom->left_width + fgeom->right_width;
      fullh -= fgeom->top_height + fgeom->bottom_height;
    }
  
  maxw = window->size_hints.max_width;
  maxh = window->size_hints.max_height;

  /* If user hasn't resized or moved, then try to shrink the window to
   * fit onscreen, while not violating the min size, just as
   * we do for maximize
   */
  if (window->maximized ||
      !(window->user_has_resized || window->user_has_moved))
    {
      maxw = MIN (maxw, fullw);
      maxh = MIN (maxh, fullh);
    }

  minw = window->size_hints.min_width;
  minh = window->size_hints.min_height;
  
  /* Check that fullscreen doesn't go under app-specified min size, if
   * so snap back to min size
   */
  if (maxw < minw)
    maxw = minw;
  if (maxh < minh)
    maxh = minh;
  
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

static void
constrain_position (MetaWindow *window,
                    MetaFrameGeometry *fgeom,
                    int         x,
                    int         y,
                    int        *new_x,
                    int        *new_y)
{
  /* frame member variables should NEVER be used in here, only
   * MetaFrameGeometry
   */

  if (!window->placed && window->calc_placement)
    meta_window_place (window, fgeom, x, y, &x, &y);
  
  if (window->type != META_WINDOW_DESKTOP &&
      window->type != META_WINDOW_DOCK)
    {
      int nw_x, nw_y;
      int se_x, se_y;
      int offscreen_w, offscreen_h;
      
      /* find furthest northwest point the window can occupy,
       * to disallow moving titlebar off the top or left
       */
      nw_x = window->screen->active_workspace->workarea.x;
      nw_y = window->screen->active_workspace->workarea.y;
      if (window->frame)
        {
          nw_x += fgeom->left_width;
          nw_y += fgeom->top_height;
        }
      
      /* find bottom-right corner of workarea */
      se_x = window->screen->active_workspace->workarea.x +
        window->screen->active_workspace->workarea.width;
      se_y = window->screen->active_workspace->workarea.y +
        window->screen->active_workspace->workarea.height;

      /* if the window's size exceeds the screen size,
       * we allow it to go off the top/left far enough
       * to get the right/bottom edges onscreen.
       */
      offscreen_w = nw_x + window->rect.width;
      offscreen_h = nw_y + window->rect.height;
      if (window->frame)
        {
          offscreen_w += fgeom->right_width;
          offscreen_h += fgeom->bottom_height;
        }

      offscreen_w = offscreen_w - se_x;
      offscreen_h = offscreen_h - se_y;

      /* Now change NW limit to reflect amount offscreen in SE direction */
      if (offscreen_w > 0)
        nw_x -= offscreen_w;
      if (offscreen_h > 0)
        nw_y -= offscreen_h;
      
      /* Convert se_x, se_y to the most bottom-right position
       * the window can occupy
       */
      if (window->frame)
        {
#define TITLEBAR_LENGTH_ONSCREEN 10
          se_x -= (fgeom->left_width + TITLEBAR_LENGTH_ONSCREEN);
          se_y -= fgeom->top_height;
        }

      /* If we have a micro-screen or huge frames maybe nw/se got
       * swapped
       */
      if (nw_x > se_x)
        {
          int tmp = nw_x;
          nw_x = se_x;
          se_x = tmp;
        }

      if (nw_y > se_y)
        {
          int tmp = nw_y;
          nw_y = se_y;
          se_y = tmp;
        }
      
      /* Clamp window to the given positions */
      if (x < nw_x)
        x = nw_x;
      if (y < nw_y)
        y = nw_y;
      
      if (x > se_x)
        x = se_x;
      if (y > se_y)
        y = se_y;

      /* If maximized, force the exact position */
      if (window->maximized)
        {
          if (x != nw_x)
            x = nw_x;
          if (y != nw_y)
            y = nw_y;
        }
    }

  *new_x = x;
  *new_y = y;
}

static void
menu_callback (MetaWindowMenu *menu,
               Display        *xdisplay,
               Window          client_xwindow,
               MetaMenuOp      op,
               int             workspace_index,
               gpointer        data)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, client_xwindow);
  
  if (window != NULL) /* window can be NULL */
    {
      meta_verbose ("Menu op %d on %s\n", op, window->desc);
      
      /* op can be 0 for none */
      switch (op)
        {
        case META_MENU_OP_DELETE:
          meta_window_delete (window, CurrentTime);
          break;

        case META_MENU_OP_MINIMIZE:
          meta_window_minimize (window);
          break;

        case META_MENU_OP_UNMAXIMIZE:
          meta_window_unmaximize (window);
          break;
      
        case META_MENU_OP_MAXIMIZE:
          meta_window_maximize (window);
          break;

        case META_MENU_OP_UNSHADE:
          meta_window_unshade (window);
          break;
      
        case META_MENU_OP_SHADE:
          meta_window_shade (window);
          break;
      
        case META_MENU_OP_WORKSPACES:
          {
            MetaWorkspace *workspace;

            workspace =
              meta_display_get_workspace_by_screen_index (window->display,
                                                          window->screen,
                                                          workspace_index);

            if (workspace)
              meta_window_change_workspace (window,
                                            workspace);
            else
              meta_warning ("Workspace %d doesn't exist\n", workspace_index);
          }
          break;

        case META_MENU_OP_STICK:
          meta_window_stick (window);
          break;

        case META_MENU_OP_UNSTICK:
          meta_window_unstick (window);
          break;

        case META_MENU_OP_MOVE:
          meta_window_raise (window);
          meta_display_begin_grab_op (window->display,
                                      window,
                                      META_GRAB_OP_KEYBOARD_MOVING,
                                      FALSE, 0, 0,
                                      CurrentTime,
                                      0, 0);
          break;

        case META_MENU_OP_RESIZE:
          break;
          
        case 0:
          /* nothing */
          break;
          
        default:
          meta_warning (G_STRLOC": Unknown window op\n");
          break;
        }
    }
  else
    {
      meta_verbose ("Menu callback on nonexistent window\n");
    }
  
  meta_ui_window_menu_free (menu);
}

void
meta_window_show_menu (MetaWindow *window,
                       int         root_x,
                       int         root_y,
                       int         button,
                       Time        timestamp)
{
  MetaMenuOp ops;
  MetaMenuOp insensitive;
  MetaWindowMenu *menu;
  
  ops = 0;
  insensitive = 0;

  ops |= (META_MENU_OP_DELETE | META_MENU_OP_WORKSPACES | META_MENU_OP_MINIMIZE | META_MENU_OP_MOVE | META_MENU_OP_RESIZE);
  
  if (window->maximized)
    ops |= META_MENU_OP_UNMAXIMIZE;
  else
    ops |= META_MENU_OP_MAXIMIZE;
  
  if (window->shaded)
    ops |= META_MENU_OP_UNSHADE;
  else
    ops |= META_MENU_OP_SHADE;

  if (window->on_all_workspaces)
    ops |= META_MENU_OP_UNSTICK;
  else
    ops |= META_MENU_OP_STICK;
  
  if (!window->has_maximize_func)
    insensitive |= META_MENU_OP_UNMAXIMIZE | META_MENU_OP_MAXIMIZE;
  
  if (!window->has_minimize_func)
    insensitive |= META_MENU_OP_MINIMIZE;
  
  if (!window->has_close_func)
    insensitive |= META_MENU_OP_DELETE;

  if (!window->has_shade_func)
    insensitive |= META_MENU_OP_SHADE | META_MENU_OP_UNSHADE;

  if (!window->has_move_func)
    insensitive |= META_MENU_OP_MOVE;

  if (!window->has_resize_func)
    insensitive |= META_MENU_OP_RESIZE;
  
  menu =
    meta_ui_window_menu_new (window->screen->ui,
                             window->xwindow,
                             ops,
                             insensitive,
                             meta_window_get_net_wm_desktop (window),
                             meta_screen_get_n_workspaces (window->screen),
                             menu_callback,
                             NULL); 

  meta_verbose ("Popping up window menu for %s\n", window->desc);
  meta_ui_window_menu_popup (menu, root_x, root_y, button, timestamp);
}

static void
window_query_root_pointer (MetaWindow *window,
                          int *x, int *y)
{
  Window root_return, child_return;
  int root_x_return, root_y_return;
  int win_x_return, win_y_return;
  unsigned int mask_return;

  XQueryPointer (window->display->xdisplay,
                 window->xwindow,
                 &root_return,
                 &child_return,
                 &root_x_return,
                 &root_y_return,
                 &win_x_return,
                 &win_y_return,
                 &mask_return);

  if (x)
    *x = root_x_return;
  if (y)
    *y = root_y_return;
}

static void
update_move (MetaWindow  *window,
             unsigned int mask,
             int          x,
             int          y)
{
  int dx, dy;
  int new_x, new_y;
  
  dx = x - window->display->grab_root_x;
  dy = y - window->display->grab_root_y;

  window->user_has_moved = TRUE;
  new_x = window->display->grab_initial_window_pos.x + dx;
  new_y = window->display->grab_initial_window_pos.y + dy;

  if (mask & ShiftMask)
    {
      /* snap to edges */
      new_x = meta_window_find_nearest_vertical_edge (window, new_x);
      new_y = meta_window_find_nearest_horizontal_edge (window, new_y);
    }
  
  meta_window_move (window, new_x, new_y);
}

static void
update_resize (MetaWindow *window,
               int x, int y)
{
  int dx, dy;
  int new_w, new_h;
  int gravity;
  
  dx = x - window->display->grab_root_x;
  dy = y - window->display->grab_root_y;

  new_w = window->display->grab_initial_window_pos.width;
  new_h = window->display->grab_initial_window_pos.height;

  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_E:
      new_w += dx;
      break;

    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_RESIZING_W:
      new_w -= dx;
      break;
      
    default:
      break;
    }
  
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_RESIZING_SW:
      new_h += dy;
      break;
      
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
      new_h -= dy;
      break;
    default:
      break;
    }

  /* compute gravity of client during operation */
  gravity = -1;
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
      gravity = NorthWestGravity;
      break;
    case META_GRAB_OP_RESIZING_S:
      gravity = NorthGravity;
      break;
    case META_GRAB_OP_RESIZING_SW:
      gravity = NorthEastGravity;
      break;      
    case META_GRAB_OP_RESIZING_N:
      gravity = SouthGravity;
      break;
    case META_GRAB_OP_RESIZING_NE:
      gravity = SouthWestGravity;
      break;
    case META_GRAB_OP_RESIZING_NW:
      gravity = SouthEastGravity;
      break;
    case META_GRAB_OP_RESIZING_E:
      gravity = WestGravity;
      break;
    case META_GRAB_OP_RESIZING_W:
      gravity = EastGravity;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  window->user_has_resized = TRUE;
  meta_window_resize_with_gravity (window, new_w, new_h, gravity);
}

void
meta_window_handle_mouse_grab_op_event (MetaWindow *window,
                                        XEvent     *event)
{
  switch (event->type)
    {
    case ButtonRelease:
      meta_display_end_grab_op (window->display, event->xbutton.time);
      
      switch (window->display->grab_op)
        {
        case META_GRAB_OP_MOVING:
          update_move (window, event->xbutton.state,
                       event->xbutton.x_root, event->xbutton.y_root);
          break;
          
        case META_GRAB_OP_RESIZING_E:
        case META_GRAB_OP_RESIZING_W:
        case META_GRAB_OP_RESIZING_S:
        case META_GRAB_OP_RESIZING_N:
        case META_GRAB_OP_RESIZING_SE:
        case META_GRAB_OP_RESIZING_SW:
        case META_GRAB_OP_RESIZING_NE:
        case META_GRAB_OP_RESIZING_NW:
          update_resize (window, event->xbutton.x_root, event->xbutton.y_root);
          break;

        default:
          break;
        }      
      break;
      
    case MotionNotify:
      switch (window->display->grab_op)
        {
        case META_GRAB_OP_MOVING:
          {
            int x, y;
            window_query_root_pointer (window, &x, &y);
            update_move (window,
                         event->xbutton.state,
                         x, y);
          }
          break;
          
        case META_GRAB_OP_RESIZING_E:
        case META_GRAB_OP_RESIZING_W:
        case META_GRAB_OP_RESIZING_S:
        case META_GRAB_OP_RESIZING_N:
        case META_GRAB_OP_RESIZING_SE:
        case META_GRAB_OP_RESIZING_SW:
        case META_GRAB_OP_RESIZING_NE:
        case META_GRAB_OP_RESIZING_NW:
          {
            int x, y;
            window_query_root_pointer (window, &x, &y);
            update_resize (window, x, y);
          }
          break;

        default:
          break;
        }
      break;

    default:
      break;
    }
}

gboolean
meta_window_shares_some_workspace (MetaWindow *window,
                                   MetaWindow *with)
{
  GList *tmp;
  
  if (window->on_all_workspaces ||
      with->on_all_workspaces)
    return TRUE;
  
  tmp = window->workspaces;
  while (tmp != NULL)
    {
      if (g_list_find (with->workspaces, tmp->data) != NULL)
        return TRUE;

      tmp = tmp->next;
    }

  return FALSE;
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
  
  meta_error_trap_pop (window->display);
}
