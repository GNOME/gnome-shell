/* Metacity X managed windows */

/* 
 * Copyright (C) 2001 Havoc Pennington, Anders Carlsson
 * Copyright (C) 2002 Red Hat, Inc.
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

#include <X11/Xatom.h>
#include <string.h>

#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif

typedef enum
{
  META_IS_CONFIGURE_REQUEST = 1 << 0,
  META_DO_GRAVITY_ADJUST    = 1 << 1,
  META_USER_MOVE_RESIZE     = 1 << 2
} MetaMoveResizeFlags;

typedef enum
{
  WIN_HINTS_SKIP_FOCUS      = (1<<0), /* "alt-tab" skips this win */
  WIN_HINTS_SKIP_WINLIST    = (1<<1), /* not in win list */
  WIN_HINTS_SKIP_TASKBAR    = (1<<2), /* not on taskbar */
  WIN_HINTS_GROUP_TRANSIENT = (1<<3), /* ??????? */
  WIN_HINTS_FOCUS_ON_CLICK  = (1<<4), /* app only accepts focus when clicked */
  WIN_HINTS_DO_NOT_COVER    = (1<<5)  /* attempt to not cover this window */
} GnomeWinHints;

static int destroying_windows_disallowed = 0;

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

static void     update_size_hints         (MetaWindow     *window);
static void     update_protocols          (MetaWindow     *window);
static void     update_wm_hints           (MetaWindow     *window);
static void     update_net_wm_state       (MetaWindow     *window);
static void     update_mwm_hints          (MetaWindow     *window);
static void     update_wm_class           (MetaWindow     *window);
static void     update_transient_for      (MetaWindow     *window);
static void     update_sm_hints           (MetaWindow     *window);
static void     update_role               (MetaWindow     *window);
static void     update_net_wm_type        (MetaWindow     *window);
static void     update_icon               (MetaWindow     *window);
static void     redraw_icon               (MetaWindow     *window); 
static void     update_struts             (MetaWindow     *window);
static void     recalc_window_type        (MetaWindow     *window);
static void     recalc_window_features    (MetaWindow     *window);
static void     recalc_do_not_cover_struts(MetaWindow     *window);
static void     invalidate_work_areas     (MetaWindow *window);
static void     set_wm_state              (MetaWindow     *window,
                                           int             state);
static void     set_net_wm_state          (MetaWindow     *window);
static void     send_configure_notify     (MetaWindow     *window);
static gboolean process_property_notify   (MetaWindow     *window,
                                           XPropertyEvent *event);
static void     meta_window_show          (MetaWindow     *window);
static void     meta_window_hide          (MetaWindow     *window);

static GList*   meta_window_get_workspaces (MetaWindow    *window);

static void     meta_window_save_rect         (MetaWindow    *window);

static void adjust_for_gravity               (MetaWindow        *window,
                                              MetaFrameGeometry *fgeom,
                                              gboolean           coords_assume_border,
                                              int                x,
                                              int                y,
                                              int               *xp,
                                              int               *yp);

static void meta_window_move_resize_internal (MetaWindow         *window,
                                              MetaMoveResizeFlags flags,
                                              int                 resize_gravity,
                                              int                 root_x_nw,
                                              int                 root_y_nw,
                                              int                 w,
                                              int                 h);


void meta_window_move_resize_now (MetaWindow  *window);

void meta_window_unqueue_calc_showing (MetaWindow *window);
void meta_window_flush_calc_showing   (MetaWindow *window);

void meta_window_unqueue_move_resize  (MetaWindow *window);
void meta_window_flush_move_resize    (MetaWindow *window);

static void meta_window_apply_session_info (MetaWindow                  *window,
                                            const MetaWindowSessionInfo *info);

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

MetaWindow*
meta_window_new (MetaDisplay *display,
                 Window       xwindow,
                 gboolean     must_be_viewable)
{
  MetaWindow *window;
  XWindowAttributes attrs;
  GSList *tmp;
  MetaWorkspace *space;
  gulong existing_wm_state;
  gulong event_mask;
#define N_INITIAL_PROPS 10
  Atom initial_props[N_INITIAL_PROPS];
  int i;
  gboolean has_shape;
  
  g_assert (N_INITIAL_PROPS == (int) G_N_ELEMENTS (initial_props));
  
  meta_verbose ("Attempting to manage 0x%lx\n", xwindow);

  if (xwindow == display->no_focus_window)
    {
      meta_verbose ("Not managing no_focus_window 0x%lx\n",
                    xwindow);
      return NULL;
    }
  
  /* Grab server */
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
  
  if (attrs.override_redirect)
    {
      meta_verbose ("Deciding not to manage override_redirect window 0x%lx\n", xwindow);
      meta_error_trap_pop (display, TRUE);
      meta_display_ungrab (display);
      return NULL;
    }

  meta_verbose ("must_be_viewable = %d attrs.map_state = %d (%s)\n",
                must_be_viewable,
                attrs.map_state,
                (attrs.map_state == IsUnmapped) ?
                "IsUnmapped" :
                (attrs.map_state == IsViewable) ?
                "IsViewable" :
                (attrs.map_state == IsUnviewable) ?
                "IsUnviewable" :
                "(unknown)");
  
  existing_wm_state = WithdrawnState;
  if (must_be_viewable && attrs.map_state != IsViewable)
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
                  "Window has_shape = %d extents %d,%d %d x %d\n",
                  has_shape, x_bounding, y_bounding,
                  w_bounding, h_bounding);
    }
#endif

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
  
  if (meta_error_trap_pop_with_return (display, FALSE) != Success)
    {
      meta_verbose ("Window 0x%lx disappeared just as we tried to manage it\n",
                    xwindow);
      meta_error_trap_pop (display, FALSE);
      meta_display_ungrab (display);
      return NULL;
    }

  g_assert (!attrs.override_redirect);
  
  window = g_new (MetaWindow, 1);

  window->dialog_pid = -1;
  window->dialog_pipe = -1;
  
  window->xwindow = xwindow;
  
  /* this is in window->screen->display, but that's too annoying to
   * type
   */
  window->display = display;
  window->workspaces = NULL;

#ifdef HAVE_XSYNC
  window->update_counter = None;
#endif
  
  window->screen = NULL;
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *scr = tmp->data;

      if (scr->xroot == attrs.root)
        {
          window->screen = tmp->data;
          break;
        }
      
      tmp = tmp->next;
    }
  
  g_assert (window->screen);

  /* avoid tons of stack updates */
  meta_stack_freeze (window->screen->stack);

  window->has_shape = has_shape;
  
  /* Remember this rect is the actual window size */
  window->rect.x = attrs.x;
  window->rect.y = attrs.y;
  window->rect.width = attrs.width;
  window->rect.height = attrs.height;

  window->size_hints.flags = 0;
  
  /* And border width, size_hints are the "request" */
  window->border_width = attrs.border_width;
  window->size_hints.x = attrs.x;
  window->size_hints.y = attrs.y;
  window->size_hints.width = attrs.width;
  window->size_hints.height = attrs.height;

  /* And this is our unmaximized size */
  window->saved_rect = window->rect;
  window->user_rect = window->rect;
  
  window->depth = attrs.depth;
  window->xvisual = attrs.visual;
  window->colormap = attrs.colormap;
  
  window->title = NULL;
  window->icon_name = NULL;
  window->icon = NULL;
  window->mini_icon = NULL;
  meta_icon_cache_init (&window->icon_cache);
  window->wm_hints_pixmap = None;
  window->wm_hints_mask = None;
  
  window->desc = g_strdup_printf ("0x%lx", window->xwindow);

  window->frame = NULL;
  window->has_focus = FALSE;

  window->user_has_move_resized = FALSE;
  
  window->maximized = FALSE;
  window->fullscreen = FALSE;
  window->on_all_workspaces = FALSE;
  window->shaded = FALSE;
  window->initially_iconic = FALSE;
  window->minimized = FALSE;
  window->iconic = FALSE;
  window->mapped = attrs.map_state != IsUnmapped;
  /* if already mapped we don't want to do the placement thing */
  window->placed = window->mapped;
  if (window->placed)
    meta_topic (META_DEBUG_PLACEMENT,
                "Not placing window 0x%lx since it's already mapped\n",
                xwindow);
  window->unmanaging = FALSE;
  window->calc_showing_queued = FALSE;
  window->move_resize_queued = FALSE;
  window->keys_grabbed = FALSE;
  window->grab_on_frame = FALSE;
  window->all_keys_grabbed = FALSE;
  window->withdrawn = FALSE;
  window->initial_workspace_set = FALSE;
  window->calc_placement = FALSE;
  
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
  
  window->wm_state_modal = FALSE;
  window->skip_taskbar = FALSE;
  window->skip_pager = FALSE;
  window->wm_state_skip_taskbar = FALSE;
  window->wm_state_skip_pager = FALSE;
  window->wm_state_above = FALSE;
  window->wm_state_below = FALSE;
  
  window->res_class = NULL;
  window->res_name = NULL;
  window->role = NULL;
  window->sm_client_id = NULL;
  window->wm_client_machine = NULL;
  window->startup_id = NULL;
  
  window->net_wm_pid = -1;
  
  window->xtransient_for = None;
  window->xgroup_leader = None;
  window->xclient_leader = None;
  window->transient_parent_is_root_window = FALSE;
  
  window->type = META_WINDOW_NORMAL;
  window->type_atom = None;

  window->has_struts = FALSE;
  window->do_not_cover = FALSE;
  window->left_strut = 0;
  window->right_strut = 0;
  window->top_strut = 0;
  window->bottom_strut = 0;

  window->cached_group = NULL;

  window->using_net_wm_name = FALSE;
  window->using_net_wm_icon_name = FALSE;
  
  window->layer = META_LAYER_LAST; /* invalid value */
  window->stack_position = -1;
  window->initial_workspace = 0; /* not used */
  meta_display_register_x_window (display, &window->xwindow, window);
  
  /* Fill these in the order we want them to be gotten.
   * we want to get window name and class first
   * so we can use them in error messages and such.
   */
  i = 0;
  initial_props[i++] = display->atom_net_wm_name;
  initial_props[i++] = display->atom_wm_client_machine;
  initial_props[i++] = display->atom_net_wm_pid;
  initial_props[i++] = XA_WM_NAME;
  initial_props[i++] = display->atom_net_wm_icon_name;
  initial_props[i++] = XA_WM_ICON_NAME;
  initial_props[i++] = display->atom_net_wm_desktop;
  initial_props[i++] = display->atom_win_workspace;
  initial_props[i++] = display->atom_net_startup_id;
  initial_props[i++] = display->atom_metacity_update_counter;
  g_assert (N_INITIAL_PROPS == i);
  
  meta_window_reload_properties (window, initial_props, N_INITIAL_PROPS);
  
  update_size_hints (window);
  update_protocols (window);
  update_wm_hints (window);
  update_struts (window);

  update_net_wm_state (window);
  
  update_mwm_hints (window);
  update_wm_class (window);
  update_transient_for (window);
  update_sm_hints (window); /* must come after transient_for */
  update_role (window);
  update_net_wm_type (window);
  update_icon (window);
  
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
  
  /* FIXME we have a tendency to set this then immediately
   * change it again.
   */
  set_wm_state (window, window->iconic ? IconicState : NormalState);
  set_net_wm_state (window);

  if (window->decorated)
    meta_window_ensure_frame (window);

  meta_window_grab_keys (window);
  meta_display_grab_window_buttons (window->display, window->xwindow);
  meta_display_grab_focus_window_button (window->display, window->xwindow);
  
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
          
          meta_workspace_add_window (window->screen->active_workspace, window);
          window->on_all_workspaces = TRUE;
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
  
  if (window->workspaces == NULL && 
      window->xtransient_for != None)
    {
      /* Try putting dialog on parent's workspace */
      MetaWindow *parent;

      parent = meta_display_lookup_x_window (window->display,
                                             window->xtransient_for);

      if (parent)
        {
          GList *tmp_list;

          meta_topic (META_DEBUG_PLACEMENT,
                      "Putting window %s on some workspaces as parent %s\n",
                      window->desc, parent->desc);
          
          if (parent->on_all_workspaces)
            window->on_all_workspaces = TRUE;
          
          tmp_list = parent->workspaces;
          while (tmp_list != NULL)
            {
              meta_workspace_add_window (tmp_list->data, window);
              
              tmp_list = tmp_list->next;
            }
        }
    }
  
  if (window->workspaces == NULL)
    {
      meta_topic (META_DEBUG_PLACEMENT,
                  "Putting window %s on active workspace\n",
                  window->desc);
      
      space = window->screen->active_workspace;

      meta_workspace_add_window (space, window);
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

  /* for the various on_all_workspaces = TRUE possible above */
  meta_window_set_current_workspace_hint (window);

  /* Put our state back where it should be,
   * passing TRUE for is_configure_request, ICCCM says
   * initial map is handled same as configure request
   */
  meta_window_move_resize_internal (window,
                                    META_IS_CONFIGURE_REQUEST,
                                    NorthWestGravity,
                                    window->size_hints.x,
                                    window->size_hints.y,
                                    window->size_hints.width,
                                    window->size_hints.height);

  /* add to MRU list */
  window->display->mru_list =
    g_list_append (window->display->mru_list, window);
  
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
  
  /* Maximize windows if they are too big for their work
   * area (bit of a hack here). Assume undecorated windows
   * probably don't intend to be maximized.
   */
  if (window->has_maximize_func && window->decorated &&
      !window->fullscreen)
    {
      MetaRectangle workarea;
      MetaRectangle outer;
      
      meta_window_get_work_area (window, TRUE, &workarea);      

      meta_window_get_outer_rect (window, &outer);
      
      if (outer.width >= workarea.width &&
          outer.height >= workarea.height)
        meta_window_maximize (window);
    }
  
  /* Sync stack changes */
  meta_stack_thaw (window->screen->stack);

  /* disable show desktop mode unless we're a desktop component */
  if (window->screen->showing_desktop &&
      window->type != META_WINDOW_DESKTOP &&
      window->type != META_WINDOW_DOCK)
    meta_screen_unshow_desktop (window->screen);
  
  meta_window_queue_calc_showing (window);

  meta_error_trap_pop (display, FALSE); /* pop the XSync()-reducing trap */
  meta_display_ungrab (display);
  
  return window;
}

/* This function should only be called from the end of meta_window_new () */
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
	  meta_window_maximize (window);

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
          while (window->workspaces)
            meta_workspace_remove_window (window->workspaces->data, window);

          tmp = spaces;
          while (tmp != NULL)
            {
              MetaWorkspace *space;

              space = tmp->data;
              
              meta_workspace_add_window (space, window);              

              meta_topic (META_DEBUG_SM,
                          "Restoring saved window %s to workspace %d\n",
                          window->desc,
                          meta_workspace_index (space));
              
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
      
      meta_topic (META_DEBUG_SM,
                  "Restoring pos %d,%d size %d x %d for %s\n",
                  x, y, w, h, window->desc);
      
      meta_window_move_resize_internal (window,
                                        META_DO_GRAVITY_ADJUST,
                                        NorthWestGravity,
                                        x, y, w, h);
    }
}

void
meta_window_free (MetaWindow  *window)
{
  GList *tmp;
  
  meta_verbose ("Unmanaging 0x%lx\n", window->xwindow);

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
                  "Focusing top window since we're unmanaging %s\n",
                  window->desc);
      meta_screen_focus_top_window (window->screen, window);
    }
  else if (window->display->expected_focus_window == window)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing top window since expected focus window freed %s\n",
                  window->desc);
      window->display->expected_focus_window = NULL;
      meta_screen_focus_top_window (window->screen, window);
    }
  else
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Unmanaging window %s which doesn't currently have focus\n",
                  window->desc);
    }

  if (window->has_struts)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Unmanaging window %s which has struts, so invalidating work areas\n",
                  window->desc);
      invalidate_work_areas (window);
    }
  
  if (window->display->grab_window == window)
    meta_display_end_grab_op (window->display,
                              meta_display_get_current_time (window->display));

  g_assert (window->display->grab_window != window);
  
  if (window->display->focus_window == window)
    window->display->focus_window = NULL;

  window->display->mru_list =
    g_list_remove (window->display->mru_list, window);
  
  meta_window_unqueue_calc_showing (window);
  meta_window_unqueue_move_resize (window);
  meta_window_free_delete_dialog (window);
  
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
  meta_display_ungrab_focus_window_button (window->display, window->xwindow);
  
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
  meta_error_trap_pop (window->display, FALSE);
}

static void
set_net_wm_state (MetaWindow *window)
{
  int i;
  unsigned long data[10];
  
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
  if (window->maximized)
    {
      data[i] = window->display->atom_net_wm_state_maximized_horz;
      ++i;
      data[i] = window->display->atom_net_wm_state_maximized_vert;
      ++i;
    }
  if (window->fullscreen)
    {
      data[i] = window->display->atom_net_wm_state_fullscreen;
      ++i;
    }
  if (window->shaded || window->minimized)
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
  
  meta_verbose ("Setting _NET_WM_STATE with %d atoms\n", i);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_state,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) data, i);
  meta_error_trap_pop (window->display, FALSE);
}

/* FIXME rename this, it makes it sound like map state is relevant */
gboolean
meta_window_visible_on_workspace (MetaWindow    *window,
                                  MetaWorkspace *workspace)
{
  return (window->on_all_workspaces && window->screen == workspace->screen) ||
    meta_workspace_contains_window (workspace, window);
}

static gboolean
window_should_be_showing (MetaWindow  *window)
{
  gboolean showing, on_workspace;

  meta_verbose ("Should be showing for window %s\n", window->desc);

  /* 1. See if we're on the workspace */
  
  on_workspace = meta_window_visible_on_workspace (window, 
                                                   window->screen->active_workspace);

  showing = on_workspace;
  
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

  /* 2. See if we're minimized */
  if (window->minimized)
    showing = FALSE;
  
  /* 3. See if we're in "show desktop" mode */
  
  if (showing &&
      window->screen->showing_desktop &&
      window->type != META_WINDOW_DESKTOP &&
      window->type != META_WINDOW_DOCK)
    {
      meta_verbose ("Window %s is on current workspace, but we're showing the desktop\n",
                    window->desc);
      showing = FALSE;
    }

  /* 4. See if an ancestor is minimized (note that
   *    ancestor's "mapped" field may not be up to date
   *    since it's being computed in this same idle queue)
   */
  
  if (showing)
    {
      MetaWindow *w;

      w = window;
      while (w != NULL)
        {
          if (w->minimized)
            {
              showing = FALSE;
              break;
            }
          
          if (w->xtransient_for == None ||
              w->transient_parent_is_root_window)
            break;
          
          w = meta_display_lookup_x_window (w->display, w->xtransient_for);
          
          if (w == window)
            break; /* Cute, someone thought they'd make a transient_for cycle */

          /* w may be null... */
        }
    }

  return showing;
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

      on_workspace = meta_window_visible_on_workspace (window, 
                                                       window->screen->active_workspace);
  
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
                                           META_MINIMIZE_ANIMATION_LENGTH,
                                           META_BOX_ANIM_SCALE);
	}

      meta_window_hide (window);
    }
  else
    {
      meta_window_show (window);
    }
}

void
meta_window_calc_showing (MetaWindow  *window)
{
  implement_showing (window, window_should_be_showing (window));
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
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      if (!window->placed)
        unplaced = g_slist_prepend (unplaced, window);
      else if (window_should_be_showing (window))
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
  
  g_slist_free (copy);

  g_slist_free (unplaced);
  g_slist_free (should_show);
  g_slist_free (should_hide);
  
  destroying_windows_disallowed -= 1;
  
  return FALSE;
}

void
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

void
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
window_takes_focus_on_map (MetaWindow *window)
{
  /* don't initially focus windows that are intended to not accept
   * focus
   */
  if (!(window->input || window->take_focus))
    return FALSE;

  switch (window->type)
    {
    case META_WINDOW_DOCK:
    case META_WINDOW_DESKTOP:
    case META_WINDOW_UTILITY:
    case META_WINDOW_SPLASHSCREEN:
    case META_WINDOW_TOOLBAR:
    case META_WINDOW_MENU:
      /* don't focus these */
      break;
    case META_WINDOW_NORMAL:

      /* Always focus new windows */
      return TRUE;

      /* Old Windows-XP style rule for reference */
      /* Focus only if the current focus is on a desktop element
       * or nonexistent.
       *
       * (using display->focus_window is a bit of a race condition,
       *  but I have no idea how to avoid it)
       */
      if (window->display->focus_window == NULL ||
          (window->display->focus_window &&
           (window->display->focus_window->type == META_WINDOW_DOCK ||
            window->display->focus_window->type == META_WINDOW_DESKTOP)))
        return TRUE;
      break;
    case META_WINDOW_DIALOG:
    case META_WINDOW_MODAL_DIALOG:

      /* Always focus */
      return TRUE;

      /* Old Windows-XP style rule for reference */
      /* Focus only if the transient parent has focus */
      /* (using display->focus_window is a bit of a race condition,
       *  but I have no idea how to avoid it)
       */
      if (window->display->focus_window == NULL ||
          (window->display->focus_window &&
           meta_window_is_ancestor_of_transient (window->display->focus_window,
                                                 window)) ||
          (window->display->focus_window->type == META_WINDOW_DOCK ||
           window->display->focus_window->type == META_WINDOW_DESKTOP))
        return TRUE;
      break;
    }

  return FALSE;
}

void
meta_window_show (MetaWindow *window)
{
  gboolean did_placement;
  gboolean did_show;
  
  meta_topic (META_DEBUG_WINDOW_STATE,
              "Showing window %s, shaded: %d iconic: %d placed: %d\n",
              window->desc, window->shaded, window->iconic, window->placed);

  did_show = FALSE;
  did_placement = FALSE;
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
      did_placement = TRUE;
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
        }      
      
      if (window->iconic)
        {
          window->iconic = FALSE;
          set_wm_state (window, NormalState);
        }
    }

  if (did_placement)
    {
      if (window->xtransient_for != None)
        {
          MetaWindow *parent;

          parent =
            meta_display_lookup_x_window (window->display,
                                          window->xtransient_for);
          
          if (parent && parent->has_focus &&
              (window->input || window->take_focus))
            {
              meta_topic (META_DEBUG_FOCUS,
                          "Focusing transient window '%s' since parent had focus\n",
                          window->desc);
              meta_window_focus (window,
                                 meta_display_get_current_time (window->display));
            }
        }

      if (window_takes_focus_on_map (window))
        {                
          meta_window_focus (window,
                             meta_display_get_current_time (window->display));
        }
    }

  if (did_show)
    {
      set_net_wm_state (window);
      
      if (window->has_struts)
        {
          meta_topic (META_DEBUG_WORKAREA,
                      "Mapped window %s with struts, so invalidating work areas\n",
                      window->desc);
          invalidate_work_areas (window);
        }
    }
}

void
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
  
  if (did_hide)
    {
      set_net_wm_state (window);
      
      if (window->has_struts)
        {
          meta_topic (META_DEBUG_WORKAREA,
                      "Unmapped window %s with struts, so invalidating work areas\n",
                      window->desc);
          invalidate_work_areas (window);
        }
    }
}

static void
queue_calc_showing_func (MetaWindow *window,
                         void       *data)
{
  meta_window_queue_calc_showing (window);
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
                      "Focusing top window due to minimization of focus window %s\n",
                      window->desc);
          meta_screen_focus_top_window (window->screen, window);
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
  if (window->screen->showing_desktop)
    meta_screen_unshow_desktop (window->screen);
  
  if (window->minimized)
    {
      window->minimized = FALSE;
      meta_window_queue_calc_showing (window);

      meta_window_foreach_transient (window,
                                     queue_calc_showing_func,
                                     NULL);
    }
}

static void
meta_window_save_rect (MetaWindow *window)
{
  if (!(window->maximized || window->fullscreen))
    {
      /* save size/pos as appropriate args for move_resize */
      window->saved_rect = window->rect;
      if (window->frame)
        {
          window->saved_rect.x += window->frame->rect.x;
          window->saved_rect.y += window->frame->rect.y;
        }
    }
}

void
meta_window_maximize (MetaWindow  *window)
{
  if (!window->maximized)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Maximizing %s\n", window->desc);

      if (window->shaded)
        meta_window_unshade (window);

      meta_window_save_rect (window);
      
      window->maximized = TRUE;

      /* FIXME why did I put this here? */
      meta_window_raise (window);      
      
      /* move_resize with new maximization constraints
       */
      meta_window_queue_move_resize (window);

      recalc_window_features (window);
      set_net_wm_state (window);
    }
}

void
meta_window_unmaximize (MetaWindow  *window)
{
  if (window->maximized)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Unmaximizing %s\n", window->desc);
      
      window->maximized = FALSE;

      meta_window_move_resize (window,
                               TRUE,
                               window->saved_rect.x,
                               window->saved_rect.y,
                               window->saved_rect.width,
                               window->saved_rect.height);

      recalc_window_features (window);
      set_net_wm_state (window);
    }
}


void
meta_window_make_fullscreen (MetaWindow  *window)
{
  if (!window->fullscreen)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Fullscreening %s\n", window->desc);

      if (window->shaded)
        meta_window_unshade (window);

      meta_window_save_rect (window);
      
      window->fullscreen = TRUE;

      meta_stack_freeze (window->screen->stack);
      meta_window_update_layer (window);
      
      meta_window_raise (window);
      meta_stack_thaw (window->screen->stack);
      
      /* move_resize with new constraints
       */
      meta_window_queue_move_resize (window);

      recalc_window_features (window);
      set_net_wm_state (window);
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

      meta_window_update_layer (window);
      
      meta_window_move_resize (window,
                               TRUE,
                               window->saved_rect.x,
                               window->saved_rect.y,
                               window->saved_rect.width,
                               window->saved_rect.height);

      recalc_window_features (window);
      set_net_wm_state (window);
    }
}

void
meta_window_shade (MetaWindow  *window)
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
      meta_window_focus (window,
                         meta_display_get_current_time (window->display));
      
      set_net_wm_state (window);
    }
}

void
meta_window_unshade (MetaWindow  *window)
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
      meta_window_focus (window, meta_display_get_current_time (window->display));

      set_net_wm_state (window);
    }
}

static void
unminimize_window_and_all_transient_parents (MetaWindow *window)
{
  MetaWindow *w;
  
  w = window;
  while (w != NULL)
    {
      meta_window_unminimize (w);
      
      if (w->xtransient_for == None ||
          w->transient_parent_is_root_window)
        break;
      
      w = meta_display_lookup_x_window (w->display, w->xtransient_for);
      
      if (w == window)
        break; /* Cute, someone thought they'd make a transient_for cycle */
      
      /* w may be null... */
    }
}

void
meta_window_activate (MetaWindow *window,
                      guint32     timestamp)
{
  /* disable show desktop mode unless we're a desktop component */
  if (window->screen->showing_desktop &&
      window->type != META_WINDOW_DESKTOP &&
      window->type != META_WINDOW_DOCK)
    meta_screen_unshow_desktop (window->screen);
  
  /* Get window on current workspace */
  if (!meta_window_visible_on_workspace (window,
                                         window->screen->active_workspace))
    meta_window_change_workspace (window,
                                  window->screen->active_workspace);
  
  if (window->shaded)
    meta_window_unshade (window);

  unminimize_window_and_all_transient_parents (window);
  
  meta_window_raise (window);
  meta_topic (META_DEBUG_FOCUS,
              "Focusing window %s due to activation\n",
              window->desc);
  meta_window_focus (window, timestamp);
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

static gboolean
static_gravity_works (MetaDisplay *display)
{
  return display->static_gravity_works;
}

static void
meta_window_move_resize_internal (MetaWindow  *window,
                                  MetaMoveResizeFlags flags,
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
  int frame_size_dx;
  int frame_size_dy;
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
  
  is_configure_request = (flags & META_IS_CONFIGURE_REQUEST) != 0;
  do_gravity_adjust = (flags & META_DO_GRAVITY_ADJUST) != 0;
  is_user_action = (flags & META_USER_MOVE_RESIZE) != 0;
  
  /* We don't need it in the idle queue anymore. */
  meta_window_unqueue_move_resize (window);  
  
  {
    int oldx, oldy;
    meta_window_get_position (window, &oldx, &oldy);
    meta_topic (META_DEBUG_GEOMETRY,
                "Move/resize %s to %d,%d %dx%d%s%s from %d,%d %dx%d\n",
                window->desc, root_x_nw, root_y_nw, w, h,
                is_configure_request ? " (configure request)" : "",
                is_user_action ? " (user move/resize)" : "",
                oldx, oldy, window->rect.width, window->rect.height);
  }
  
  if (window->frame)
    meta_frame_calc_geometry (window->frame,
                              &fgeom);
  
  constrain_size (window, &fgeom, w, h, &w, &h);
  meta_topic (META_DEBUG_GEOMETRY,
              "Constrained resize of %s to %d x %d\n", window->desc, w, h);

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
      
      meta_topic (META_DEBUG_GEOMETRY,
                  "Compensated position for gravity, new pos %d,%d\n",
                  root_x_nw, root_y_nw);
    }

  /* There can be somewhat bogus interactions between gravity
   * and the position constraints (with position contraints
   * basically breaking gravity). Not sure how to fix this.
   */  

  switch (resize_gravity)
    {
      /* If client is staying fixed on the east during resize, then we
       * have to move the west edge.
       */
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      root_x_nw -= size_dx;
      break;

      /* centered horizontally */
    case NorthGravity:
    case SouthGravity:
    case CenterGravity:
      root_x_nw -= size_dx / 2;
      break;
      
    default:
      break;
    }

  switch (resize_gravity)
    {
      /* If client is staying fixed on the south during resize,
       * we have to move the north edge
       */
    case SouthGravity:
    case SouthEastGravity:
    case SouthWestGravity:
      root_y_nw -= size_dy;
      break;

      /* centered vertically */
    case EastGravity:
    case WestGravity:
    case CenterGravity:
      root_y_nw -= size_dy / 2;
      break;
      
    default:
      break;
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
  
  constrain_position (window,
                      window->frame ? &fgeom : NULL,
                      root_x_nw, root_y_nw,
                      &root_x_nw, &root_y_nw);

  meta_topic (META_DEBUG_GEOMETRY,
              "Constrained position to %d,%d\n",
              root_x_nw, root_y_nw);
      
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
  if (use_static_gravity)
    {
      if ((size_dx + size_dy) >= 0)
        configure_frame_first = FALSE;
      else
        configure_frame_first = TRUE;
    }
  else
    {
      configure_frame_first = FALSE;
    }


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
  
  /* Update struts for new window size */
  if (window->do_not_cover && (need_resize_client || need_move_client))
    {
      recalc_do_not_cover_struts (window);

      /* Does a resize on all windows on entire current workspace,
       * would be an infinite loop except for need_resize_client
       * above. We rely on reaching an equilibrium state, which
       * is somewhat fragile, though.
       */

      meta_topic (META_DEBUG_WORKAREA, "Window %s resized so invalidating its work areas\n",
                  window->desc);
      invalidate_work_areas (window);
    }

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

  meta_window_get_position (window, &x, &y);
  
  meta_window_move_resize_internal (window,
                                    user_op ? META_USER_MOVE_RESIZE : 0,
                                    NorthWestGravity,
                                    x, y, w, h);
}

void
meta_window_move (MetaWindow  *window,
                  gboolean     user_op,
                  int          root_x_nw,
                  int          root_y_nw)
{
  meta_window_move_resize_internal (window,
                                    user_op ? META_USER_MOVE_RESIZE : 0,
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
  meta_window_move_resize_internal (window,
                                    user_op ? META_USER_MOVE_RESIZE : 0,
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

  meta_window_get_position (window, &x, &y);
  
  meta_window_move_resize_internal (window,
                                    user_op ? META_USER_MOVE_RESIZE : 0,
                                    gravity,
                                    x, y, w, h);
}

void
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

static void
check_maximize_to_work_area (MetaWindow          *window,
                             const MetaRectangle *work_area)
{
  /* If we now fill the screen, maximize.
   * the point here is that fill horz + fill vert = maximized
   */
  MetaRectangle rect;

  if (!window->has_maximize_func)
    return;
  
  meta_window_get_outer_rect (window, &rect);

  if ( rect.x >= work_area->x &&
       rect.y >= work_area->y &&
       (((work_area->width - work_area->x) - rect.width) <
        window->size_hints.width_inc) &&
       (((work_area->height - work_area->y) - rect.height) <
        window->size_hints.height_inc) )
    meta_window_maximize (window);
}

void
meta_window_fill_horizontal (MetaWindow  *window)
{
  MetaRectangle work_area;
  int x, y, w, h;
  
  meta_window_get_user_position (window, &x, &y);

  w = window->rect.width;
  h = window->rect.height;
  
  meta_window_get_work_area (window, TRUE, &work_area);
  
  x = work_area.x;
  w = work_area.width;
  
  if (window->frame != NULL)
    {
      x += window->frame->child_x;
      w -= (window->frame->child_x + window->frame->right_width);
    }
  
  meta_window_move_resize (window, TRUE,
                           x, y, w, h);

  check_maximize_to_work_area (window, &work_area);
}

void
meta_window_fill_vertical (MetaWindow  *window)
{
  MetaRectangle work_area;
  int x, y, w, h;
  
  meta_window_get_user_position (window, &x, &y);

  w = window->rect.width;
  h = window->rect.height;

  meta_window_get_work_area (window, TRUE, &work_area);

  y = work_area.y;
  h = work_area.height;
  
  if (window->frame != NULL)
    {
      y += window->frame->child_y;
      h -= (window->frame->child_y + window->frame->bottom_height);
    }
  
  meta_window_move_resize (window, TRUE,
                           x, y, w, h);

  check_maximize_to_work_area (window, &work_area);
}

static guint move_resize_idle = 0;
static GSList *move_resize_pending = NULL;

/* We want to put windows whose size/pos affects other
 * windows earlier in the queue, for efficiency.
 */
static int
move_resize_cmp (gconstpointer a, gconstpointer b)
{
  MetaWindow *aw = (gpointer) a;
  MetaWindow *bw = (gpointer) b;

  if (aw->do_not_cover && !bw->do_not_cover)
    return -1; /* aw before bw */
  else if (!aw->do_not_cover && bw->do_not_cover)
    return 1;
  else
    return 0;
}

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

  copy = g_slist_sort (copy, move_resize_cmp);
  
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

void
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

void
meta_window_flush_move_resize (MetaWindow *window)
{
  if (window->move_resize_queued)
    {
      meta_window_unqueue_move_resize (window);
      meta_window_move_resize_now (window);
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
    move_resize_idle = g_idle_add (idle_move_resize, NULL);
  
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
meta_window_get_outer_rect (MetaWindow    *window,
                            MetaRectangle *rect)
{
  if (window->frame)
    *rect = window->frame->rect;
  else
    *rect = window->rect;
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

void
meta_window_focus (MetaWindow  *window,
                   Time         timestamp)
{  
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
          XSetInputFocus (window->display->xdisplay,
                          window->frame->xwindow,
                          RevertToPointerRoot,
                          CurrentTime);
          window->display->expected_focus_window = window;
        }
    }
  else
    {
      meta_error_trap_push (window->display);
      
      if (window->input)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Calling XSetInputFocus() on client window %s since input = true\n",
                      window->desc);
          XSetInputFocus (window->display->xdisplay,
                          window->xwindow,
                          RevertToPointerRoot,
                          timestamp);
          window->display->expected_focus_window = window;
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
}

static void
meta_window_change_workspace_without_transients (MetaWindow    *window,
                                                 MetaWorkspace *workspace)
{
  GList *next;
  
  meta_verbose ("Changing window %s to workspace %d\n",
                window->desc, meta_workspace_index (workspace));
  
  /* unstick if stuck. meta_window_unstick would call 
   * meta_window_change_workspace recursively if the window
   * is not in the active workspace.
   */
  if (window->on_all_workspaces)
    meta_window_unstick (window);

  /* See if we're already on this space. If not, make sure we are */
  if (g_list_find (window->workspaces, workspace) == NULL)
    meta_workspace_add_window (workspace, window);
  
  /* Remove from all other spaces */
  next = window->workspaces;
  while (next != NULL)
    {
      MetaWorkspace *remove;
      remove = next->data;
      next = next->next;

      if (remove != workspace)
        meta_workspace_remove_window (remove, window);
    }

  /* list size == 1 */
  g_assert (window->workspaces != NULL);
  g_assert (window->workspaces->next == NULL);
}

static void
change_workspace_foreach (MetaWindow *window,
                          void       *data)
{
  meta_window_change_workspace_without_transients (window, data);
}

void
meta_window_change_workspace (MetaWindow    *window,
                              MetaWorkspace *workspace)
{
  meta_window_change_workspace_without_transients (window, workspace);

  meta_window_foreach_transient (window, change_workspace_foreach,
                                 workspace);
}

void
meta_window_stick (MetaWindow  *window)
{
  meta_verbose ("Sticking window %s current on_all_workspaces = %d\n",
                window->desc, window->on_all_workspaces);
  
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
    return meta_workspace_index (window->workspaces->data);
}

void
meta_window_set_current_workspace_hint (MetaWindow *window)
{
  /* FIXME if on more than one workspace, we claim to be "sticky",
   * the WM spec doesn't say what to do here.
   */
  unsigned long data[1];

  if (window->workspaces == NULL)
    {
      /* this happens when unmanaging windows */      
      return;
    }
  
  data[0] = meta_window_get_net_wm_desktop (window);

  meta_verbose ("Setting _NET_WM_DESKTOP of %s to %ld\n",
                window->desc, data[0]);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_desktop,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (window->display, FALSE);
}

void
meta_window_raise (MetaWindow  *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Raising window %s\n", window->desc);

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
		  "Not allowing position change for window %s PPosition 0x%lx USPosition 0x%lx type %d\n", 
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
   */
  
  meta_window_move_resize_internal (window, META_IS_CONFIGURE_REQUEST,
                                    only_resize ?
                                    window->size_hints.win_gravity : NorthWestGravity,
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
      /* I think the wm spec should maybe put a time
       * in this message, CurrentTime here is sort of
       * bogus. But it rarely matters most likely.
       */
      meta_window_delete (window, meta_display_get_current_time (window->display));

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
          
          meta_verbose ("Request to change _NET_WM_STATE action %ld atom1: %s atom2: %s\n",
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

          shade = (action == _NET_WM_STATE_ADD ||
                   (action == _NET_WM_STATE_TOGGLE && !window->shaded));
          if (shade && window->has_shade_func)
            meta_window_shade (window);
          else
            meta_window_unshade (window);
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
          second == display->atom_net_wm_state_maximized_horz ||
          first == display->atom_net_wm_state_maximized_vert ||
          second == display->atom_net_wm_state_maximized_vert)
        {
          gboolean max;

          max = (action == _NET_WM_STATE_ADD ||
                 (action == _NET_WM_STATE_TOGGLE && !window->maximized));
          if (max && window->has_maximize_func)
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
      
      x_root = event->xclient.data.l[0];
      y_root = event->xclient.data.l[1];
      action = event->xclient.data.l[2];
      button = event->xclient.data.l[3];

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

          meta_window_begin_grab_op (window,
                                     op,
                                     meta_display_get_current_time (window->display));
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
                                          FALSE,
                                          button, 0,
                                          meta_display_get_current_time (window->display),
                                          x_root,
                                          y_root);
            }
        }

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_active_window)
    {
      meta_verbose ("_NET_ACTIVE_WINDOW request for window '%s', activating",
                    window->desc);

      meta_window_activate (window, meta_display_get_current_time (window->display));

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_win_hints)
    {
      /* gnome-winhints.c seems to indicate that the hints are
       * in l[1], though god knows why it's like that
       */
      gulong data[1];
      
      meta_verbose ("_WIN_HINTS client message, hints: %ld\n",
                    event->xclient.data.l[1]);

      if (event->xclient.data.l[1] & WIN_HINTS_DO_NOT_COVER)
        {
          meta_topic (META_DEBUG_WORKAREA,
                      "Setting WIN_HINTS_DO_NOT_COVER\n");
          
          data[0] = WIN_HINTS_DO_NOT_COVER;

          meta_error_trap_push (window->display);
          XChangeProperty (window->display->xdisplay,
                           window->xwindow, window->display->atom_win_hints,
                           XA_CARDINAL, 32, PropModeReplace,
                           (unsigned char *)data, 1);
          meta_error_trap_pop (window->display, FALSE);
        }
      else
        {
          meta_topic (META_DEBUG_WORKAREA,
                      "Unsetting WIN_HINTS_DO_NOT_COVER\n");
          
          data[0] = 0;

          meta_error_trap_push (window->display);
          XDeleteProperty (window->display->xdisplay,
                           window->xwindow, window->display->atom_win_hints);
          meta_error_trap_pop (window->display, FALSE);
        }
      
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
      if (window->display->expected_focus_window == window)
        window->display->expected_focus_window = NULL;

      if (window != window->display->focus_window)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "* Focus --> %s\n", window->desc);
          window->display->focus_window = window;
          window->has_focus = TRUE;
          /* Move to the front of the MRU list */
          window->display->mru_list =
            g_list_remove (window->display->mru_list, window);
          window->display->mru_list =
            g_list_prepend (window->display->mru_list, window);
          if (window->frame)
            meta_frame_queue_draw (window->frame);
          
          meta_error_trap_push (window->display);
          XInstallColormap (window->display->xdisplay,
                            window->colormap);
          meta_error_trap_pop (window->display, FALSE);

          /* move into FOCUSED_WINDOW layer */
          meta_window_update_layer (window);
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
  /* FIXME once we move entirely to the window-props.h framework, we
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

      meta_icon_cache_property_changed (&window->icon_cache,
                                        window->display,
                                        event->atom);
      
      update_wm_hints (window);
      update_icon (window);
      redraw_icon (window);
      
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
      /* because ensure/destroy frame may unmap */
      meta_window_queue_calc_showing (window);
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
  else if (event->atom == window->display->atom_net_wm_icon)
    {
      meta_verbose ("Property notify on %s for NET_WM_ICON\n", window->desc);
      meta_icon_cache_property_changed (&window->icon_cache,
                                        window->display,
                                        event->atom);
      update_icon (window);
      redraw_icon (window);
    }
  else if (event->atom == window->display->atom_kwm_win_icon)
    {
      meta_verbose ("Property notify on %s for KWM_WIN_ICON\n", window->desc);

      meta_icon_cache_property_changed (&window->icon_cache,
                                        window->display,
                                        event->atom);
      update_icon (window);
      redraw_icon (window);
    }
  else if (event->atom == window->display->atom_net_wm_strut)
    {
      meta_verbose ("Property notify on %s for _NET_WM_STRUT\n", window->desc);
      update_struts (window);
    }
  else if (event->atom == window->display->atom_win_hints)
    {
      meta_verbose ("Property notify on %s for _WIN_HINTS\n", window->desc);
      update_struts (window);
    }
  else if (event->atom == window->display->atom_net_startup_id)
    {
      meta_verbose ("Property notify on %s for _NET_STARTUP_ID\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_net_startup_id);
    }
  else if (event->atom == window->display->atom_metacity_update_counter)
    {
      meta_verbose ("Property notify on %s for _METACITY_UPDATE_COUNTER\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_metacity_update_counter);
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

#define FLAG_TOGGLED_ON(old,new,flag) \
 (((old)->flags & (flag)) == 0 &&     \
  ((new)->flags & (flag)) != 0)

#define FLAG_TOGGLED_OFF(old,new,flag) \
 (((old)->flags & (flag)) != 0 &&      \
  ((new)->flags & (flag)) == 0)

#define FLAG_CHANGED(old,new,flag) \
  (FLAG_TOGGLED_ON(old,new,flag) || FLAG_TOGGLED_OFF(old,new,flag))

static void
spew_size_hints_differences (const XSizeHints *old,
                             const XSizeHints *new)
{
  if (FLAG_CHANGED (old, new, USPosition))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: USPosition now %s\n",
                FLAG_TOGGLED_ON (old, new, USPosition) ? "set" : "unset");
  if (FLAG_CHANGED (old, new, USSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: USSize now %s\n",
                FLAG_TOGGLED_ON (old, new, USSize) ? "set" : "unset");
  if (FLAG_CHANGED (old, new, PPosition))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PPosition now %s\n",
                FLAG_TOGGLED_ON (old, new, PPosition) ? "set" : "unset");
  if (FLAG_CHANGED (old, new, PSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PSize now %s\n",
                FLAG_TOGGLED_ON (old, new, PSize) ? "set" : "unset");
  if (FLAG_CHANGED (old, new, PMinSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PMinSize now %s (%d x %d -> %d x %d)\n",
                FLAG_TOGGLED_ON (old, new, PMinSize) ? "set" : "unset",
                old->min_width, old->min_height,
                new->min_width, new->min_height);
  if (FLAG_CHANGED (old, new, PMaxSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PMaxSize now %s (%d x %d -> %d x %d)\n",
                FLAG_TOGGLED_ON (old, new, PMaxSize) ? "set" : "unset",
                old->max_width, old->max_height,
                new->max_width, new->max_height);
  if (FLAG_CHANGED (old, new, PResizeInc))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PResizeInc now %s (width_inc %d -> %d height_inc %d -> %d)\n",
                FLAG_TOGGLED_ON (old, new, PResizeInc) ? "set" : "unset",
                old->width_inc, new->width_inc,
                old->height_inc, new->height_inc);
  if (FLAG_CHANGED (old, new, PAspect))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PAspect now %s (min %d/%d -> %d/%d max %d/%d -> %d/%d)\n",
                FLAG_TOGGLED_ON (old, new, PAspect) ? "set" : "unset",
                old->min_aspect.x, old->min_aspect.y,
                new->min_aspect.x, new->min_aspect.y,
                old->max_aspect.x, old->max_aspect.y,
                new->max_aspect.x, new->max_aspect.y);
  if (FLAG_CHANGED (old, new, PBaseSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PBaseSize now %s (%d x %d -> %d x %d)\n",
                FLAG_TOGGLED_ON (old, new, PBaseSize) ? "set" : "unset",
                old->base_width, old->base_height,
                new->base_width, new->base_height);
  if (FLAG_CHANGED (old, new, PWinGravity))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PWinGravity now %s  (%d -> %d)\n",
                FLAG_TOGGLED_ON (old, new, PWinGravity) ? "set" : "unset",
                old->win_gravity, new->win_gravity);  
}

static void
update_size_hints (MetaWindow *window)
{
  int x, y, w, h;
  gulong supplied;
  XSizeHints old_hints;
  XSizeHints *new_hints;
  
  meta_topic (META_DEBUG_GEOMETRY, "Updating WM_NORMAL_HINTS for %s\n", window->desc);

  old_hints = window->size_hints;
  
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
  new_hints = NULL;
  
  meta_prop_get_size_hints (window->display,
                            window->xwindow,
                            XA_WM_NORMAL_HINTS,
                            &new_hints,
                            &supplied);
  
  /* as far as I can tell, "supplied" is just to check whether we had
   * old-style normal hints without gravity, base size as returned by
   * XGetNormalHints(), so we don't really use it as we fixup
   * window->size_hints to have those fields if they're missing.
   */
  
  if (new_hints != NULL)
    {
      window->size_hints = *new_hints;
      XFree (new_hints);
      new_hints = NULL;
    }
  
  /* Put back saved ConfigureRequest. */
  window->size_hints.x = x;
  window->size_hints.y = y;
  window->size_hints.width = w;
  window->size_hints.height = h;
  
  if (window->size_hints.flags & PBaseSize)
    {
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets base size %d x %d\n",
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
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets min size %d x %d\n",
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
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets max size %d x %d\n",
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
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets max width %d less than min width %d, disabling resize\n",
                  window->desc,
                  window->size_hints.max_width,
                  window->size_hints.min_width);
      window->size_hints.max_width = window->size_hints.min_width;
    }

  if (window->size_hints.max_height < window->size_hints.min_height)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets max height %d less than min height %d, disabling resize\n",
                  window->desc,
                  window->size_hints.max_height,
                  window->size_hints.min_height);
      window->size_hints.max_height = window->size_hints.min_height;
    }
  
  if (window->size_hints.flags & PResizeInc)
    {
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets resize width inc: %d height inc: %d\n",
                  window->desc,
                  window->size_hints.width_inc,
                  window->size_hints.height_inc);
      if (window->size_hints.width_inc == 0)
        {
          window->size_hints.width_inc = 1;
          meta_topic (META_DEBUG_GEOMETRY, "Corrected 0 width_inc to 1\n");
        }
      if (window->size_hints.height_inc == 0)
        {
          window->size_hints.height_inc = 1;
          meta_topic (META_DEBUG_GEOMETRY, "Corrected 0 height_inc to 1\n");
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
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets min_aspect: %d/%d max_aspect: %d/%d\n",
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
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets gravity %d\n",
                  window->desc,
                  window->size_hints.win_gravity);
    }
  else
    {
      meta_topic (META_DEBUG_GEOMETRY, "Window %s doesn't set gravity, using NW\n",
                  window->desc);
      window->size_hints.win_gravity = NorthWestGravity;
      window->size_hints.flags |= PWinGravity;
    }

  recalc_window_features (window);

  spew_size_hints_differences (&old_hints, &window->size_hints);
}

static void
update_protocols (MetaWindow *window)
{
  Atom *protocols = NULL;
  int n_protocols = 0;
  int i;

  window->take_focus = FALSE;
  window->delete_window = FALSE;
  window->net_wm_ping = FALSE;
  
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
          else if (protocols[i] == window->display->atom_net_wm_ping)
            window->net_wm_ping = TRUE;
          ++i;
        }

      meta_XFree (protocols);
    }

  meta_verbose ("Window %s has take_focus = %d delete_window = %d net_wm_ping = %d\n",
                window->desc, window->take_focus, window->delete_window,
                window->net_wm_ping);
  
  meta_error_trap_pop (window->display, TRUE);
}

static void
update_wm_hints (MetaWindow *window)
{
  XWMHints *hints;
  Window old_group_leader;

  old_group_leader = window->xgroup_leader;
  
  /* Fill in defaults */
  window->input = TRUE;
  window->initially_iconic = FALSE;
  window->xgroup_leader = None;
  window->wm_hints_pixmap = None;
  window->wm_hints_mask = None;

  hints = NULL;
  meta_prop_get_wm_hints (window->display,
                          window->xwindow,
                          XA_WM_HINTS,
                          &hints);
  
  if (hints)
    {
      if (hints->flags & InputHint)
        window->input = hints->input;

      if (hints->flags & StateHint)
        window->initially_iconic = (hints->initial_state == IconicState);

      if (hints->flags & WindowGroupHint)
        window->xgroup_leader = hints->window_group;

      if (hints->flags & IconPixmapHint)
        window->wm_hints_pixmap = hints->icon_pixmap;

      if (hints->flags & IconMaskHint)
        window->wm_hints_mask = hints->icon_mask;
      
      meta_verbose ("Read WM_HINTS input: %d iconic: %d group leader: 0x%lx pixmap: 0x%lx mask: 0x%lx\n",
                    window->input, window->initially_iconic,
                    window->xgroup_leader,
                    window->wm_hints_pixmap,
                    window->wm_hints_mask);
      
      meta_XFree (hints);
    }

  if (window->xgroup_leader != old_group_leader)
    {
      meta_verbose ("Window %s changed its group leader to 0x%lx\n",
                    window->desc, window->xgroup_leader);
      
      meta_window_group_leader_changed (window);
    }
}

static void
update_net_wm_state (MetaWindow *window)
{
  /* We know this is only on initial window creation,
   * clients don't change the property.
   */

  int n_atoms;
  Atom *atoms;

  window->shaded = FALSE;
  window->maximized = FALSE;
  window->wm_state_modal = FALSE;
  window->wm_state_skip_taskbar = FALSE;
  window->wm_state_skip_pager = FALSE;
  window->wm_state_above = FALSE;
  window->wm_state_below = FALSE;
  
  if (meta_prop_get_atom_list (window->display, window->xwindow,
                               window->display->atom_net_wm_state,
                               &atoms, &n_atoms))
    {
      int i;
      
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
          else if (atoms[i] == window->display->atom_net_wm_state_skip_taskbar)
            window->wm_state_skip_taskbar = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_skip_pager)
            window->wm_state_skip_pager = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_fullscreen)
            window->fullscreen = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_above)
            window->wm_state_above = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_below)
            window->wm_state_below = TRUE;
          
          ++i;
        }
  
      meta_XFree (atoms);
    }
  
  recalc_window_type (window);
}


static void
update_mwm_hints (MetaWindow *window)
{
  MotifWmHints *hints;

  window->mwm_decorated = TRUE;
  window->mwm_border_only = FALSE;
  window->mwm_has_close_func = TRUE;
  window->mwm_has_minimize_func = TRUE;
  window->mwm_has_maximize_func = TRUE;
  window->mwm_has_move_func = TRUE;
  window->mwm_has_resize_func = TRUE;

  if (!meta_prop_get_motif_hints (window->display, window->xwindow,
                                  window->display->atom_motif_wm_hints,
                                  &hints))
    {
      meta_verbose ("Window %s has no MWM hints\n", window->desc);
      return;
    }
  
  /* We support those MWM hints deemed non-stupid */

  meta_verbose ("Window %s has MWM hints\n",
                window->desc);
  
  if (hints->flags & MWM_HINTS_DECORATIONS)
    {
      meta_verbose ("Window %s sets MWM_HINTS_DECORATIONS 0x%lx\n",
                    window->desc, hints->decorations);

      if (hints->decorations == 0)
        window->mwm_decorated = FALSE;
      /* some input methods use this */
      else if (hints->decorations == MWM_DECOR_BORDER)
        window->mwm_border_only = TRUE;
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
      
      if ((hints->functions & MWM_FUNC_ALL) == 0)
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
      
      if ((hints->functions & MWM_FUNC_CLOSE) != 0)
        {
          meta_verbose ("Window %s toggles close via MWM hints\n",
                        window->desc);
          window->mwm_has_close_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MINIMIZE) != 0)
        {
          meta_verbose ("Window %s toggles minimize via MWM hints\n",
                        window->desc);
          window->mwm_has_minimize_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MAXIMIZE) != 0)
        {
          meta_verbose ("Window %s toggles maximize via MWM hints\n",
                        window->desc);
          window->mwm_has_maximize_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MOVE) != 0)
        {
          meta_verbose ("Window %s toggles move via MWM hints\n",
                        window->desc);
          window->mwm_has_move_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_RESIZE) != 0)
        {
          meta_verbose ("Window %s toggles resize via MWM hints\n",
                        window->desc);
          window->mwm_has_resize_func = toggle_value;
        }
    }
  else
    meta_verbose ("Functions flag unset\n");

  meta_XFree (hints);

  recalc_window_features (window);
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

static void
update_wm_class (MetaWindow *window)
{
  XClassHint ch;
  
  if (window->res_class)
    g_free (window->res_class);
  if (window->res_name)
    g_free (window->res_name);

  window->res_class = NULL;
  window->res_name = NULL;

  ch.res_name = NULL;
  ch.res_class = NULL;

  meta_prop_get_class_hint (window->display,
                            window->xwindow,
                            XA_WM_CLASS,
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
                window->res_class ? window->res_class : "none",
                window->res_name ? window->res_name : "none");
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
      
      if (w->xtransient_for == None ||
          w->transient_parent_is_root_window)
        break;

      w = meta_display_lookup_x_window (w->display, w->xtransient_for);

      if (w == window)
        break; /* Cute, someone thought they'd make a transient_for cycle */
    }
      
  if (leader)
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
update_transient_for (MetaWindow *window)
{
  Window w;

  meta_error_trap_push (window->display);
  w = None;
  XGetTransientForHint (window->display->xdisplay,
                        window->xwindow,
                        &w);
  meta_error_trap_pop (window->display, TRUE);
  window->xtransient_for = w;

  window->transient_parent_is_root_window =
    window->xtransient_for == window->screen->xroot;
  
  if (window->xtransient_for != None)
    meta_verbose ("Window %s transient for 0x%lx (root = %d)\n", window->desc,
                  window->xtransient_for, window->transient_parent_is_root_window);
  else
    meta_verbose ("Window %s is not transient\n", window->desc);
  
  /* may now be a dialog */
  recalc_window_type (window);

  /* update stacking constraints */
  meta_stack_update_transient (window->screen->stack, window);
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

static void
update_net_wm_type (MetaWindow *window)
{
  int n_atoms;
  Atom *atoms;
  int i;

  window->type_atom = None;
  n_atoms = 0;
  atoms = NULL;
  
  if (!meta_prop_get_atom_list (window->display, window->xwindow, 
                                window->display->atom_net_wm_window_type,
                                &atoms, &n_atoms))
    {
      /* Fall back to WIN_LAYER */
      gulong layer = WIN_LAYER_NORMAL;

      if (meta_prop_get_cardinal (window->display,
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
update_icon (MetaWindow *window)
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
    }
  
  g_assert (window->icon);
  g_assert (window->mini_icon);
}

static void
redraw_icon (MetaWindow *window)
{
  /* We could probably be smart and just redraw the icon here. */
  if (window->frame)
    meta_ui_queue_frame_draw (window->screen->ui, window->frame->xwindow);
}

static GList*
meta_window_get_workspaces (MetaWindow *window)
{
  if (window->on_all_workspaces)
    return window->screen->workspaces;
  else
    return window->workspaces;
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

static void
update_struts (MetaWindow *window)
{
  gulong *struts = NULL;
  int nitems;
  gboolean old_has_struts;
  gboolean old_do_not_cover;
  int old_left;
  int old_right;
  int old_top;
  int old_bottom;
  
  meta_verbose ("Updating struts for %s\n", window->desc);
  
  old_has_struts = window->has_struts;
  old_do_not_cover = window->do_not_cover;
  old_left = window->left_strut;
  old_right = window->right_strut;
  old_top = window->top_strut;
  old_bottom = window->bottom_strut;  
  
  window->has_struts = FALSE;
  window->do_not_cover = FALSE;
  window->left_strut = 0;
  window->right_strut = 0;
  window->top_strut = 0;
  window->bottom_strut = 0;
  
  if (meta_prop_get_cardinal_list (window->display,
                                   window->xwindow,
                                   window->display->atom_net_wm_strut,
                                   &struts, &nitems))
    {
      if (nitems != 4)
        {
          meta_verbose ("_NET_WM_STRUT on %s has %d values instead of 4\n",
                        window->desc, nitems);
          meta_XFree (struts);
        }
      
      window->has_struts = TRUE;
      window->left_strut = struts[0];
      window->right_strut = struts[1];
      window->top_strut = struts[2];
      window->bottom_strut = struts[3];

      meta_verbose ("Using _NET_WM_STRUT struts %d %d %d %d for window %s\n",
                    window->left_strut, window->right_strut,
                    window->top_strut, window->bottom_strut,
                    window->desc);
      
      meta_XFree (struts);
    }
  else
    {
      meta_verbose ("No _NET_WM_STRUT property for %s\n",
                    window->desc);
    }
  
  if (!window->has_struts)
    {
      /* Try _WIN_HINTS */
      gulong hints;

      if (meta_prop_get_cardinal (window->display,
                                  window->xwindow,
                                  window->display->atom_win_hints,
                                  &hints))
        {
          if (hints & WIN_HINTS_DO_NOT_COVER)
            {
              window->has_struts = TRUE;
              window->do_not_cover = TRUE;
              recalc_do_not_cover_struts (window);

              meta_verbose ("Using _WIN_HINTS struts %d %d %d %d for window %s\n",
                            window->left_strut, window->right_strut,
                            window->top_strut, window->bottom_strut,
                            window->desc);              
            }
          else
            {
              meta_verbose ("DO_NOT_COVER hint not set in _WIN_HINTS\n");
            }
        }
      else
        {
          meta_verbose ("No _WIN_HINTS property on %s\n",
                        window->desc);
        }
    }

  if (old_has_struts != window->has_struts ||
      old_do_not_cover != window->do_not_cover ||
      old_left != window->left_strut ||
      old_right != window->right_strut ||
      old_top != window->top_strut ||
      old_bottom != window->bottom_strut)
    {  
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

static void
recalc_do_not_cover_struts (MetaWindow *window)
{
  if (window->do_not_cover)
    {
      /* We only understand windows that are aligned to
       * a screen edge
       */
      gboolean horizontal;
      gboolean on_left_edge;
      gboolean on_right_edge;
      gboolean on_bottom_edge;
      gboolean on_top_edge;      

      window->left_strut = 0;
      window->right_strut = 0;
      window->top_strut = 0;
      window->bottom_strut = 0;
      
      on_left_edge = window->rect.x == 0;
      on_right_edge = (window->rect.x + window->rect.width) ==
        window->screen->width;
      on_top_edge = window->rect.y == 0;
      on_bottom_edge = (window->rect.y + window->rect.height) ==
        window->screen->height;
      
      /* cheesy heuristic to decide where the strut goes */
      if (on_left_edge && on_right_edge && on_bottom_edge)
        horizontal = TRUE;
      else if (on_left_edge && on_right_edge && on_top_edge)
        horizontal = TRUE;
      else if (on_top_edge && on_bottom_edge && on_left_edge)
        horizontal = FALSE;
      else if (on_top_edge && on_bottom_edge && on_right_edge)
        horizontal = FALSE;
      else
        horizontal = window->rect.width > window->rect.height;
      
      if (horizontal)
        {
          if (on_top_edge)
            window->top_strut = window->rect.height;
          else if (on_bottom_edge)
            window->bottom_strut = window->rect.height;
        }
      else
        {
          if (on_left_edge)
            window->left_strut = window->rect.width;
          else if (on_right_edge)
            window->right_strut = window->rect.width;
        }
    }
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
      meta_window_update_layer (window);
    }
}

static void
set_allowed_actions_hint (MetaWindow *window)
{
#define MAX_N_ACTIONS 8
  unsigned long data[MAX_N_ACTIONS];
  int i;

  i = 0;
  if (window->has_close_func)
    {
      data[i] = window->display->atom_net_wm_action_close;
      ++i;
    }
  if (window->has_minimize_func)
    {
      data[i] = window->display->atom_net_wm_action_maximize_horz;
      ++i;
      data[i] = window->display->atom_net_wm_action_maximize_vert;
      ++i;
    }
  if (window->has_move_func)
    {
      data[i] = window->display->atom_net_wm_action_move;
      ++i;
    }
  if (window->has_resize_func)
    {
      data[i] = window->display->atom_net_wm_action_resize;
      ++i;
    }
  if (window->has_shade_func)
    {
      data[i] = window->display->atom_net_wm_action_shade;
      ++i;
    }
  if (!window->always_sticky)
    {
      data[i] = window->display->atom_net_wm_action_stick;
      ++i;
    }

  /* We always allow this */
  data[i] = window->display->atom_net_wm_action_change_desktop;
  ++i;

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
      if (window->size_hints.min_width == window->screen->width &&
          window->size_hints.min_height == window->screen->height &&
          !window->decorated)
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
  if (window->fullscreen)
    {
      const MetaXineramaScreenInfo *xinerama;

      xinerama = meta_screen_get_xinerama_for_window (window->screen,
                                                      window);


      fullw = xinerama->width;
      fullh = xinerama->height;
    }
  else if (window->type == META_WINDOW_DESKTOP ||
           window->type == META_WINDOW_DOCK)
    {
      
      fullw = window->screen->width;
      fullh = window->screen->height;
    }
  else
    {
      MetaRectangle work_area;
      
      meta_window_get_work_area (window, TRUE, &work_area);
      
      fullw = work_area.width;
      fullh = work_area.height;
    }

  if (window->frame && !window->fullscreen)
    {
      fullw -= (fgeom->left_width + fgeom->right_width);
      fullh -= (fgeom->top_height + fgeom->bottom_height);
    }
  
  maxw = window->size_hints.max_width;
  maxh = window->size_hints.max_height;
  
  if (window->maximized || window->fullscreen)
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
  
  if (window->maximized || window->fullscreen)
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
      delta = FLOOR (height - width / min_aspect, window->size_hints.height_inc);
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
constrain_position (MetaWindow        *window,
                    MetaFrameGeometry *fgeom,
                    int                x,
                    int                y,
                    int               *new_x,
                    int               *new_y)
{  
  /* frame member variables should NEVER be used in here, only
   * MetaFrameGeometry
   */

  if (!window->placed && window->calc_placement)
    meta_window_place (window, fgeom, x, y, &x, &y);

  if (window->type == META_WINDOW_DESKTOP)
    {
      x = 0;
      y = 0;
    }
  else if (window->type == META_WINDOW_DOCK)
    {
      ; /* let it do whatever */
    }
  else if (window->fullscreen)
    {
      const MetaXineramaScreenInfo *xinerama;

      xinerama = meta_screen_get_xinerama_for_window (window->screen,
                                                      window);
      
      x = xinerama->x_origin;
      y = xinerama->y_origin;

      /* If the window's geometry gridding (e.g. for a terminal)
       * prevents fullscreen, center the window within
       * the screen area.
       */
      x += (xinerama->width - window->rect.width) / 2;

      /* If the window is somehow larger than the screen be paranoid
       * and fix the resulting negative coords
       */
      if (x < xinerama->x_origin)
        x = xinerama->x_origin;
    }
  else if (window->maximized)
    {
      MetaRectangle work_area;
      
      meta_window_get_work_area (window, TRUE, &work_area);
      
      x = work_area.x;
      y = work_area.y;
      if (window->frame)
        {
          x += fgeom->left_width;
          y += fgeom->top_height;
        }

      /* If the window's geometry gridding (e.g. for a terminal)
       * prevents full maximization, center the window within
       * the maximized area horizontally.
       */
      x += (work_area.width - window->rect.width -
            (window->frame ? (fgeom->left_width + fgeom->right_width) : 0)) / 2;
    }
  else
    {
      int nw_x, nw_y;
      int se_x, se_y;
      int offscreen_w, offscreen_h;
      MetaRectangle work_area;
      
      meta_window_get_work_area (window, FALSE, &work_area);
      
      /* (FIXME instead of TITLEBAR_LENGTH_ONSCREEN, get the actual
       * size of the menu control?).
       */
      
#define TITLEBAR_LENGTH_ONSCREEN 36
      
      /* find furthest northwest point the window can occupy */
      nw_x = work_area.x;
      nw_y = work_area.y;

      /* FIXME note this means framed windows can go off the left
       * but not unframed windows.
       */
      if (window->frame)
        {
          /* Must keep TITLEBAR_LENGTH_ONSCREEN onscreen when moving left */
          nw_x -= fgeom->left_width + window->rect.width + fgeom->right_width - TITLEBAR_LENGTH_ONSCREEN;
          /* Can't move off the top */
          nw_y += fgeom->top_height;
        }
      
      /* find bottom-right corner of workarea */
      se_x = work_area.x + work_area.width;
      se_y = work_area.y + work_area.height;

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
      
      /* Limit movement off the right/bottom.
       * Remember, we're constraining StaticGravity position.
       */
      if (window->frame)
        {
          se_x -= TITLEBAR_LENGTH_ONSCREEN;
          se_y -= 0;
        }
      else
        {
          /* for frameless windows, just require an arbitrary little
           * chunk to be onscreen
           */
          se_x -= TITLEBAR_LENGTH_ONSCREEN;
          se_y -= TITLEBAR_LENGTH_ONSCREEN;
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
      
      /* Clamp window to the given positions.
       * Do the SE clamp first, so that the NW clamp has precedence
       * and we don't tend to lose the titlebar for too-large
       * windows.
       */
      if (x > se_x)
        x = se_x;
      if (y > se_y)
        y = se_y;
      
      if (x < nw_x)
        x = nw_x;
      if (y < nw_y)
        y = nw_y;
      
#undef TITLEBAR_LENGTH_ONSCREEN
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
          meta_window_delete (window, meta_display_get_current_time (window->display));
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
              meta_screen_get_workspace_by_index (window->screen,
                                                  workspace_index);

            if (workspace)
              {
                meta_workspace_activate (workspace);
                meta_window_change_workspace (window,
                                              workspace);
                meta_window_raise (window);
              }
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
          meta_window_begin_grab_op (window,
                                     META_GRAB_OP_KEYBOARD_MOVING,
                                     meta_display_get_current_time (window->display));
          break;

        case META_MENU_OP_RESIZE:
          meta_window_begin_grab_op (window,
                                     META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN,
                                     meta_display_get_current_time (window->display));
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

  if (!META_WINDOW_ALLOWS_MOVE (window))
    insensitive |= META_MENU_OP_MOVE;

  if (!META_WINDOW_ALLOWS_RESIZE (window))
    insensitive |= META_MENU_OP_RESIZE;

   if (window->always_sticky)
     insensitive |= META_MENU_OP_UNSTICK | META_MENU_OP_WORKSPACES;
  
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

static void
clear_moveresize_time (MetaWindow *window)
{
  /* Forces the next update to actually do something */
  window->display->grab_last_moveresize_time.tv_sec = 0;
  window->display->grab_last_moveresize_time.tv_usec = 0;
}

static gboolean
check_moveresize_frequency (MetaWindow *window)
{
  GTimeVal current_time;
  double elapsed;
  double max_resizes_per_second;
  
  g_get_current_time (&current_time);

  /* use milliseconds, 1000 milliseconds/second */
  elapsed =
    ((((double)current_time.tv_sec - window->display->grab_last_moveresize_time.tv_sec) * G_USEC_PER_SEC +
      (current_time.tv_usec - window->display->grab_last_moveresize_time.tv_usec))) / 1000.0;

#ifdef HAVE_XSYNC
  if (window->display->grab_update_alarm != None)
    max_resizes_per_second = 1.0;   /* this is max resizes without
                                     * getting any alarms; we resize
                                     * immediately if we get one.
                                     * i.e. this is a timeout for the
                                     * client getting stuck.
                                     */
  else
#endif /* HAVE_XSYNC */
    max_resizes_per_second = 20.0;
  
#define EPSILON (1e-6)  
  if (elapsed >= 0.0 && elapsed < (1000.0 / max_resizes_per_second))
    {
      meta_topic (META_DEBUG_RESIZING,
                  "Delaying move/resize as only %g of %g seconds elapsed\n",
                  elapsed / 1000.0, 1.0 / max_resizes_per_second);
      return FALSE;
    }
  else if (elapsed < (0.0 - EPSILON)) /* handle clock getting set backward */
    clear_moveresize_time (window);
  
  /* store latest time */
  window->display->grab_last_moveresize_time = current_time;
  
  meta_topic (META_DEBUG_RESIZING,
              " Doing move/resize now (%g of %g seconds elapsed)\n",
              elapsed / 1000.0, 1.0 / max_resizes_per_second);
  
  return TRUE;
}

static void
update_move (MetaWindow  *window,
             unsigned int mask,
             int          x,
             int          y)
{
  int dx, dy;
  int new_x, new_y;
  int move_threshold;

  window->display->grab_latest_motion_x = x;
  window->display->grab_latest_motion_y = y;
  
  dx = x - window->display->grab_current_root_x;
  dy = y - window->display->grab_current_root_y;

  new_x = window->display->grab_current_window_pos.x + dx;
  new_y = window->display->grab_current_window_pos.y + dy;

  if (mask & ShiftMask)
    {
      /* snap to edges */
      new_x = meta_window_find_nearest_vertical_edge (window, new_x);
      new_y = meta_window_find_nearest_horizontal_edge (window, new_y);
    }

  /* Force a move regardless of time if a certain delta is exceeded,
   * so we don't get too out of sync with reality when dropping frames
   */
  /* FIXME this delta is all wrong, as it's absolute since
   * the grab started. We want some sort of delta since
   * we last sent a configure or something.
   */
  move_threshold = 30;
  
  if (!check_moveresize_frequency (window) &&
      ABS (dx) < move_threshold && ABS (dy) < move_threshold)
    return;
  
  meta_window_move (window, TRUE, new_x, new_y);
}

static void
update_resize (MetaWindow *window,
               int x, int y)
{
  int dx, dy;
  int new_w, new_h;
  int gravity;
  int resize_threshold;
  MetaRectangle old;
  
  window->display->grab_latest_motion_x = x;
  window->display->grab_latest_motion_y = y;
  
  dx = x - window->display->grab_current_root_x;
  dy = y - window->display->grab_current_root_y;

  new_w = window->display->grab_current_window_pos.width;
  new_h = window->display->grab_current_window_pos.height;

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
      break;
    default:
      break;
    }

  /* Force a move regardless of time if a certain delta
   * is exceeded
   * FIXME this delta is all wrong, as it's absolute since
   * the grab started. We want some sort of delta since
   * we last sent a configure or something.
   */
#ifdef HAVE_XSYNC
  if (window->display->grab_update_alarm != None)
    resize_threshold = 5000; /* disable */
  else
#endif /* HAVE_XSYNC */
    resize_threshold = 30;

  if (!check_moveresize_frequency (window) &&
      ABS (dx) < resize_threshold && ABS (dy) < resize_threshold)
    return;

  old = window->rect;
  
  /* compute gravity of client during operation */
  gravity = meta_resize_gravity_from_grab_op (window->display->grab_op);
  g_assert (gravity >= 0);
  
  meta_window_resize_with_gravity (window, TRUE, new_w, new_h, gravity);

  /* If we don't actually resize the window, we clear the timestamp,
   * so we'll quickly try again.  Otherwise you get "stuck" because
   * the window doesn't increment its _METACITY_UPDATE_COUNTER when
   * nothing happens.
   */
  if (window->rect.width == old.width &&
      window->rect.height == old.height)
    clear_moveresize_time (window);
}

typedef struct
{
  XEvent prev_event;
  gboolean done;
  int count;
} CompressEventData;

static Bool
compress_event_predicate (Display  *display,
                          XEvent   *xevent,
                          XPointer  arg)
{
  CompressEventData *ced = (void*) arg;
  
  if (ced->done)
    return False;
  else if (ced->prev_event.type == xevent->type &&
           ced->prev_event.xany.window == xevent->xany.window)
    {
      ced->count += 1;
      return True;
    }
  else if (xevent->type != Expose &&
           xevent->type != ConfigureNotify &&
           xevent->type != PropertyNotify)
    {
      /* Don't compress across most unrelated events, just to be safe */
      ced->done = TRUE;
      return False;
    }
  else
    return False;
}

static void
maybe_replace_with_newer_event (MetaWindow *window,
                                XEvent     *event)
{
  XEvent new_event;
  CompressEventData ced;
  
  /* Chew up all events of the same type on the same window */

  ced.count = 0;
  ced.done = FALSE;
  ced.prev_event = *event;
  while (XCheckIfEvent (window->display->xdisplay,
                        &new_event,
                        compress_event_predicate,
                        (void*) &ced.prev_event))
    ced.prev_event = new_event;

  if (ced.count > 0)
    {
      meta_topic (META_DEBUG_RESIZING,
                  "Compressed %d motion events\n", ced.count);
      *event = ced.prev_event;
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
          clear_moveresize_time (window); /* force update to do something */

          /* no pointer round trip here, to keep in sync */
          update_resize (window,
                         window->display->grab_latest_motion_x,
                         window->display->grab_latest_motion_y);
          break;
          
        default:
          break;
        }
    }
#endif /* HAVE_XSYNC */
  
  switch (event->type)
    {
    case ButtonRelease:      
      if (meta_grab_op_is_moving (window->display->grab_op))
        {
          clear_moveresize_time (window);
          if (event->xbutton.root == window->screen->xroot)
            update_move (window, event->xbutton.state,
                         event->xbutton.x_root, event->xbutton.y_root);
        }
      else if (meta_grab_op_is_resizing (window->display->grab_op))
        {
          clear_moveresize_time (window);
          if (event->xbutton.root == window->screen->xroot)
            update_resize (window, event->xbutton.x_root, event->xbutton.y_root);
        }

      meta_display_end_grab_op (window->display, event->xbutton.time);
      break;    

    case MotionNotify:
      if (meta_grab_op_is_moving (window->display->grab_op))
        {
          if (event->xmotion.root == window->screen->xroot)
            {
              maybe_replace_with_newer_event (window, event);
              update_move (window,
                           event->xmotion.state,
                           event->xmotion.x_root,
                           event->xmotion.y_root);
            }
        }
      else if (meta_grab_op_is_resizing (window->display->grab_op))
        {
          if (event->xmotion.root == window->screen->xroot)
            {
              maybe_replace_with_newer_event (window, event);
              update_resize (window,
                             event->xmotion.x_root,
                             event->xmotion.y_root);
            }
        }
      break;

    case EnterNotify:
    case LeaveNotify:
      if (meta_grab_op_is_moving (window->display->grab_op))
        {
          if (event->xcrossing.root == window->screen->xroot)
            update_move (window,
                         event->xcrossing.state,
                         event->xcrossing.x_root,
                         event->xcrossing.y_root);
        }
      else if (meta_grab_op_is_resizing (window->display->grab_op))
        {
          if (event->xcrossing.root == window->screen->xroot)
            update_resize (window,
                           event->xcrossing.x_root,
                           event->xcrossing.y_root);
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
  
  meta_error_trap_pop (window->display, FALSE);
}

void
meta_window_get_work_area (MetaWindow    *window,
                           gboolean       for_current_xinerama,
                           MetaRectangle *area)
{
  MetaRectangle space_area;
  GList *tmp;
  
  int left_strut;
  int right_strut;
  int top_strut;
  int bottom_strut;  

  if (for_current_xinerama)
    {
      const MetaXineramaScreenInfo *xinerama;
      
      xinerama = meta_screen_get_xinerama_for_window (window->screen,
                                                      window);
      
      left_strut = xinerama->x_origin;
      right_strut = window->screen->width - xinerama->width - xinerama->x_origin;
      top_strut = xinerama->y_origin;
      bottom_strut = window->screen->height - xinerama->height - xinerama->y_origin;
    }
  else
    {
      left_strut = 0;
      right_strut = 0;
      top_strut = 0;
      bottom_strut = 0;
    }
      
  tmp = meta_window_get_workspaces (window);
  
  while (tmp != NULL)
    {
      meta_workspace_get_work_area (tmp->data, &space_area);

      left_strut = MAX (left_strut, space_area.x);
      right_strut = MAX (right_strut,
                         (window->screen->width - space_area.x - space_area.width));
      top_strut = MAX (top_strut, space_area.y);
      bottom_strut = MAX (bottom_strut,
                          (window->screen->height - space_area.y - space_area.height));      
      
      tmp = tmp->next;
    }
  
  area->x = left_strut;
  area->y = top_strut;
  area->width = window->screen->width - left_strut - right_strut;
  area->height = window->screen->height - top_strut - bottom_strut;

  meta_topic (META_DEBUG_WORKAREA,
              "Window %s has work area %d,%d %d x %d\n",
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
      int gravity;
      int x, y;
      MetaFrameGeometry fgeom;

      if (window->frame)
        meta_frame_calc_geometry (window->frame, &fgeom);
      else
        {
          fgeom.left_width = 0;
          fgeom.right_width = 0;
          fgeom.top_height = 0;
          fgeom.bottom_height = 0;
        }
      
      gravity = meta_resize_gravity_from_grab_op (window->display->grab_op);
      g_assert (gravity >= 0);

      meta_window_get_position (window, &x, &y);
      
      meta_ui_resize_popup_set (window->display->grab_resize_popup,
                                gravity,
                                x, y,
                                window->rect.width,
                                window->rect.height,
                                window->size_hints.base_width,
                                window->size_hints.base_height,
                                window->size_hints.min_width,
                                window->size_hints.min_height,
                                window->size_hints.width_inc,
                                window->size_hints.height_inc,
                                fgeom.left_width,
                                fgeom.right_width,
                                fgeom.top_height,
                                fgeom.bottom_height);

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
        (* func) (transient, data);
      
      tmp = tmp->next;
    }

  g_slist_free (windows);
}

gboolean
meta_window_is_ancestor_of_transient (MetaWindow *window,
                                      MetaWindow *transient)
{
  MetaWindow *w;

  if (window == transient)
    return FALSE;
  
  w = transient;
  while (w != NULL)
    {          
      if (w->xtransient_for == None ||
          w->transient_parent_is_root_window)
        return FALSE;

      if (w->xtransient_for == window->xwindow)
        return TRUE;
      
      w = meta_display_lookup_x_window (w->display, w->xtransient_for);
      
      if (w == transient)
        return FALSE; /* Cycle */
      
      /* w may be null... */
    }

  return FALSE;
}

static gboolean
warp_pointer (MetaWindow *window,
              MetaGrabOp  grab_op,
              int        *x,
              int        *y)
{
  switch (grab_op)
    {
      case META_GRAB_OP_KEYBOARD_MOVING:
      case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
        *x = window->rect.width / 2;
        *y = window->rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_S:
        *x = window->rect.width / 2;
        *y = window->rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_N:
        *x = window->rect.width / 2;
        *y = 0;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_W:
        *x = 0;
        *y = window->rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_E:
        *x = window->rect.width;
        *y = window->rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_SE:
        *x = window->rect.width;
        *y = window->rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_NE:
        *x = window->rect.width;
        *y = 0;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_SW:
        *x = 0;
        *y = window->rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_NW:
        *x = 0;
        *y = 0;
        break;

      default:
        return FALSE;
    }

  meta_error_trap_push_with_return (window->display);
  
  XWarpPointer (window->display->xdisplay,
                None,
                window->xwindow,
                0, 0, 0, 0, 
                *x,
                *y);

  if (meta_error_trap_pop_with_return (window->display, FALSE) != Success)
    {
      meta_verbose ("Failed to warp pointer for window %s\n", window->desc);
      return FALSE;
    }

  return TRUE;
}

gboolean
meta_window_warp_pointer (MetaWindow *window,
                          MetaGrabOp  grab_op)
{
  int x, y;
 
  return warp_pointer (window, grab_op, &x, &y); 
}

void
meta_window_begin_grab_op (MetaWindow *window,
                           MetaGrabOp  op,
                           Time        timestamp)
{
  int x, y, x_offset, y_offset;

  meta_window_get_position (window, &x, &y);

  meta_window_raise (window);

  warp_pointer (window, op, &x_offset, &y_offset);

  meta_display_begin_grab_op (window->display,
                              window->screen,
                              window,
                              op,
                              FALSE, 0, 0,
                              timestamp,
                              x + x_offset, 
                              y + y_offset);
}

void
meta_window_update_resize_grab_op (MetaWindow *window,
                                   gboolean    update_cursor)
{
  int x, y, x_offset, y_offset;

  meta_window_get_position (window, &x, &y);

  warp_pointer (window, window->display->grab_op, &x_offset, &y_offset);

  window->display->grab_current_root_x = x + x_offset;
  window->display->grab_current_root_y = y + y_offset;

  if (window->display->grab_window)
    window->display->grab_current_window_pos = window->rect;

  if (update_cursor)
    {
      meta_display_set_grab_op_cursor (window->display,
                                       NULL,
                                       window->display->grab_op,
                                       TRUE,
                                       window->display->grab_xwindow,
                                       meta_display_get_current_time (window->display));
    }
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
