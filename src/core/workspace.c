/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004, 2005 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:workspace
 * @title:MetaWorkspace
 * @short_description:Workspaces
 *
 * A workspace is a set of windows which all live on the same
 * screen.  (You may also see the name "desktop" around the place,
 * which is the EWMH's name for the same thing.)  Only one workspace
 * of a screen may be active at once; all windows on all other workspaces
 * are unmapped.
 */

#include <config.h>
#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "screen-private.h"
#include <meta/workspace.h>
#include "workspace-private.h"
#include "boxes-private.h"
#include <meta/errors.h>
#include <meta/prefs.h>

#include <meta/compositor.h>

#include <X11/Xatom.h>
#include <string.h>
#ifdef HAVE_LIBCANBERRA
#include <canberra-gtk.h>
#endif

void meta_workspace_queue_calc_showing   (MetaWorkspace *workspace);
static void focus_ancestor_or_top_window (MetaWorkspace *workspace,
                                          MetaWindow    *not_this_one,
                                          guint32        timestamp);
static void free_this                    (gpointer candidate,
                                          gpointer dummy);

G_DEFINE_TYPE (MetaWorkspace, meta_workspace, G_TYPE_OBJECT);

enum {
  PROP_0,

  PROP_N_WINDOWS,
  PROP_WORKSPACE_INDEX,

  LAST_PROP,
};

static GParamSpec *obj_props[LAST_PROP];

enum
{
  WINDOW_ADDED,
  WINDOW_REMOVED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _MetaWorkspaceLogicalMonitorData
{
  GList *logical_monitor_region;
  MetaRectangle logical_monitor_work_area;
} MetaWorkspaceLogicalMonitorData;

static MetaWorkspaceLogicalMonitorData *
meta_workspace_get_logical_monitor_data (MetaWorkspace      *workspace,
                                         MetaLogicalMonitor *logical_monitor)
{
  if (!workspace->logical_monitor_data)
    return NULL;
  return g_hash_table_lookup (workspace->logical_monitor_data, logical_monitor);
}

static void
workspace_logical_monitor_data_free (MetaWorkspaceLogicalMonitorData *data)
{
  g_clear_pointer (&data->logical_monitor_region,
                   meta_rectangle_free_list_and_elements);
}

static MetaWorkspaceLogicalMonitorData *
meta_workspace_ensure_logical_monitor_data (MetaWorkspace      *workspace,
                                            MetaLogicalMonitor *logical_monitor)
{
  MetaWorkspaceLogicalMonitorData *data;

  data = meta_workspace_get_logical_monitor_data (workspace, logical_monitor);
  if (data)
    return data;

  if (!workspace->logical_monitor_data)
    {
      workspace->logical_monitor_data =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               (GDestroyNotify) workspace_logical_monitor_data_free);
    }

  data = g_new0 (MetaWorkspaceLogicalMonitorData, 1);
  g_hash_table_insert (workspace->logical_monitor_data, logical_monitor, data);

  return data;
}

static void
meta_workspace_clear_logical_monitor_data (MetaWorkspace *workspace)
{
  g_clear_pointer (&workspace->logical_monitor_data, g_hash_table_destroy);
}

static void
meta_workspace_finalize (GObject *object)
{
  /* Actual freeing done in meta_workspace_remove() for now */
  G_OBJECT_CLASS (meta_workspace_parent_class)->finalize (object);
}

static void
meta_workspace_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_workspace_get_property (GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  MetaWorkspace *ws = META_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_N_WINDOWS:
      /*
       * This is reliable, but not very efficient; should we store
       * the list lenth ?
       */
      g_value_set_uint (value, g_list_length (ws->windows));
      break;
    case PROP_WORKSPACE_INDEX:
      g_value_set_uint (value, meta_workspace_index (ws));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_workspace_class_init (MetaWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize     = meta_workspace_finalize;
  object_class->get_property = meta_workspace_get_property;
  object_class->set_property = meta_workspace_set_property;

  signals[WINDOW_ADDED] = g_signal_new ("window-added",
                                        G_TYPE_FROM_CLASS (klass),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL, NULL,
                                        G_TYPE_NONE, 1,
                                        META_TYPE_WINDOW);
  signals[WINDOW_REMOVED] = g_signal_new ("window-removed",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE, 1,
                                          META_TYPE_WINDOW);

  obj_props[PROP_N_WINDOWS] = g_param_spec_uint ("n-windows",
                                                 "N Windows",
                                                 "Number of windows",
                                                 0, G_MAXUINT, 0,
                                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  obj_props[PROP_WORKSPACE_INDEX] = g_param_spec_uint ("workspace-index",
                                                       "Workspace index",
                                                       "The workspace's index",
                                                       0, G_MAXUINT, 0,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_props);
}

static void
meta_workspace_init (MetaWorkspace *workspace)
{
}

MetaWorkspace*
meta_workspace_new (MetaScreen *screen)
{
  MetaWorkspace *workspace;
  GSList *windows, *l;

  workspace = g_object_new (META_TYPE_WORKSPACE, NULL);

  workspace->screen = screen;
  workspace->screen->workspaces =
    g_list_append (workspace->screen->workspaces, workspace);
  workspace->windows = NULL;
  workspace->mru_list = NULL;

  workspace->work_areas_invalid = TRUE;
  workspace->work_area_screen.x = 0;
  workspace->work_area_screen.y = 0;
  workspace->work_area_screen.width = 0;
  workspace->work_area_screen.height = 0;

  workspace->screen_region = NULL;
  workspace->screen_edges = NULL;
  workspace->monitor_edges = NULL;
  workspace->list_containing_self = g_list_prepend (NULL, workspace);

  workspace->builtin_struts = NULL;
  workspace->all_struts = NULL;

  workspace->showing_desktop = FALSE;

  /* make sure sticky windows are in our mru_list */
  windows = meta_display_list_windows (screen->display, META_LIST_SORTED);
  for (l = windows; l; l = l->next)
    if (meta_window_located_on_workspace (l->data, workspace))
      meta_workspace_add_window (workspace, l->data);
  g_slist_free (windows);

  return workspace;
}

/* Foreach function for workspace_free_struts() */
static void
free_this (gpointer candidate, gpointer dummy)
{
  g_free (candidate);
}

/**
 * workspace_free_all_struts:
 * @workspace: The workspace.
 *
 * Frees the combined struts list of a workspace.
 */
static void
workspace_free_all_struts (MetaWorkspace *workspace)
{
  if (workspace->all_struts == NULL)
    return;

  g_slist_foreach (workspace->all_struts, free_this, NULL);
  g_slist_free (workspace->all_struts);
  workspace->all_struts = NULL;
}

/**
 * workspace_free_builtin_struts:
 * @workspace: The workspace.
 *
 * Frees the struts list set with meta_workspace_set_builtin_struts
 */
static void
workspace_free_builtin_struts (MetaWorkspace *workspace)
{
  if (workspace->builtin_struts == NULL)
    return;

  g_slist_foreach (workspace->builtin_struts, free_this, NULL);
  g_slist_free (workspace->builtin_struts);
  workspace->builtin_struts = NULL;
}

/* Ensure that the workspace is empty by making sure that
 * all of our windows are on-all-workspaces. */
static void
assert_workspace_empty (MetaWorkspace *workspace)
{
  GList *l;
  for (l = workspace->windows; l != NULL; l = l->next)
    {
      MetaWindow *window = l->data;
      g_assert (window->on_all_workspaces);
    }
}

void
meta_workspace_remove (MetaWorkspace *workspace)
{
  g_return_if_fail (workspace != workspace->screen->active_workspace);

  assert_workspace_empty (workspace);

  workspace->screen->workspaces =
    g_list_remove (workspace->screen->workspaces, workspace);

  meta_workspace_clear_logical_monitor_data (workspace);

  g_list_free (workspace->mru_list);
  g_list_free (workspace->list_containing_self);

  workspace_free_builtin_struts (workspace);

  /* screen.c:update_num_workspaces(), which calls us, removes windows from
   * workspaces first, which can cause the workareas on the workspace to be
   * invalidated (and hence for struts/regions/edges to be freed).
   * So, no point trying to double free it; that causes a crash
   * anyway.  #361804.
   */

  if (!workspace->work_areas_invalid)
    {
      workspace_free_all_struts (workspace);
      meta_rectangle_free_list_and_elements (workspace->screen_region);
      meta_rectangle_free_list_and_elements (workspace->screen_edges);
      meta_rectangle_free_list_and_elements (workspace->monitor_edges);
    }

  g_object_unref (workspace);

  /* don't bother to reset names, pagers can just ignore
   * extra ones
   */
}

void
meta_workspace_add_window (MetaWorkspace *workspace,
                           MetaWindow    *window)
{
  g_assert (g_list_find (workspace->mru_list, window) == NULL);
  workspace->mru_list = g_list_prepend (workspace->mru_list, window);

  workspace->windows = g_list_prepend (workspace->windows, window);

  if (window->struts)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Invalidating work area of workspace %d since we're adding window %s to it\n",
                  meta_workspace_index (workspace), window->desc);
      meta_workspace_invalidate_work_area (workspace);
    }

  g_signal_emit (workspace, signals[WINDOW_ADDED], 0, window);
  g_object_notify_by_pspec (G_OBJECT (workspace), obj_props[PROP_N_WINDOWS]);
}

void
meta_workspace_remove_window (MetaWorkspace *workspace,
                              MetaWindow    *window)
{
  workspace->windows = g_list_remove (workspace->windows, window);

  workspace->mru_list = g_list_remove (workspace->mru_list, window);
  g_assert (g_list_find (workspace->mru_list, window) == NULL);

  if (window->struts)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Invalidating work area of workspace %d since we're removing window %s from it\n",
                  meta_workspace_index (workspace), window->desc);
      meta_workspace_invalidate_work_area (workspace);
    }

  g_signal_emit (workspace, signals[WINDOW_REMOVED], 0, window);
  g_object_notify (G_OBJECT (workspace), "n-windows");
}

void
meta_workspace_relocate_windows (MetaWorkspace *workspace,
                                 MetaWorkspace *new_home)
{
  GList *copy, *l;

  g_return_if_fail (workspace != new_home);

  /* can't modify list we're iterating over */
  copy = g_list_copy (workspace->windows);

  for (l = copy; l != NULL; l = l->next)
    {
      MetaWindow *window = l->data;

      if (!window->on_all_workspaces)
        meta_window_change_workspace (window, new_home);
    }

  g_list_free (copy);

  assert_workspace_empty (workspace);
}

void
meta_workspace_queue_calc_showing  (MetaWorkspace *workspace)
{
  GList *l;

  for (l = workspace->windows; l != NULL; l = l->next)
    meta_window_queue (l->data, META_QUEUE_CALC_SHOWING);
}

static void
workspace_switch_sound(MetaWorkspace *from,
                       MetaWorkspace *to)
{
#ifdef HAVE_LIBCANBERRA
  MetaWorkspaceLayout layout;
  int i, nw, x, y, fi, ti;
  const char *e;

  nw = meta_screen_get_n_workspaces(from->screen);
  fi = meta_workspace_index(from);
  ti = meta_workspace_index(to);

  meta_screen_calc_workspace_layout(from->screen,
                                    nw,
                                    fi,
                                    &layout);

  for (i = 0; i < nw; i++)
    if (layout.grid[i] == ti)
      break;

  if (i >= nw)
    {
      meta_bug("Failed to find destination workspace in layout\n");
      goto finish;
    }

  y = i / layout.cols;
  x = i % layout.cols;

  /* We priorize horizontal over vertical movements here. The
     rationale for this is that horizontal movements are probably more
     interesting for sound effects because speakers are usually
     positioned on a horizontal and not a vertical axis. i.e. your
     spatial "Woosh!" effects will easily be able to encode horizontal
     movement but not such much vertical movement. */

  if (x < layout.current_col)
    e = "desktop-switch-left";
  else if (x > layout.current_col)
    e = "desktop-switch-right";
  else if (y < layout.current_row)
    e = "desktop-switch-up";
  else if (y > layout.current_row)
    e = "desktop-switch-down";
  else
    {
      meta_bug("Uh, origin and destination workspace at same logic position!\n");
      goto finish;
    }

  ca_context_play(ca_gtk_context_get(), 1,
                  CA_PROP_EVENT_ID, e,
                  CA_PROP_EVENT_DESCRIPTION, "Desktop switched",
                  CA_PROP_CANBERRA_CACHE_CONTROL, "permanent",
                  NULL);

 finish:
  meta_screen_free_workspace_layout (&layout);
#endif /* HAVE_LIBCANBERRA */
}

/**
 * meta_workspace_activate_with_focus:
 * @workspace: a #MetaWorkspace
 * @focus_this: the #MetaWindow to be focused, or %NULL
 * @timestamp: timestamp for @focus_this
 *
 * Switches to @workspace and possibly activates the window @focus_this.
 *
 * The window @focus_this is activated by calling meta_window_activate()
 * which will unminimize it and transient parents, raise it and give it
 * the focus.
 *
 * If a window is currently being moved by the user, it will be
 * moved to @workspace.
 *
 * The advantage of calling this function instead of meta_workspace_activate()
 * followed by meta_window_activate() is that it happens as a unit, so
 * no other window gets focused first before @focus_this.
 */
void
meta_workspace_activate_with_focus (MetaWorkspace *workspace,
                                    MetaWindow    *focus_this,
                                    guint32        timestamp)
{
  MetaWorkspace  *old;
  MetaWindow     *move_window;
  MetaScreen     *screen;
  MetaDisplay    *display;
  MetaCompositor *comp;
  MetaWorkspaceLayout layout1, layout2;
  gint num_workspaces, current_space, new_space;
  MetaMotionDirection direction;

  meta_verbose ("Activating workspace %d\n",
                meta_workspace_index (workspace));

  if (workspace->screen->active_workspace == workspace)
    return;

  /* Free any cached pointers to the workspaces's edges from
   * a current resize or move operation */
  meta_display_cleanup_edges (workspace->screen->display);

  if (workspace->screen->active_workspace)
    workspace_switch_sound (workspace->screen->active_workspace, workspace);

  /* Note that old can be NULL; e.g. when starting up */
  old = workspace->screen->active_workspace;

  workspace->screen->active_workspace = workspace;

  meta_screen_set_active_workspace_hint (workspace->screen);

  /* If the "show desktop" mode is active for either the old workspace
   * or the new one *but not both*, then update the
   * _net_showing_desktop hint
   */
  if (old && (old->showing_desktop != workspace->showing_desktop))
    meta_screen_update_showing_desktop_hint (workspace->screen);

  if (old == NULL)
    return;

  move_window = NULL;
  if (meta_grab_op_is_moving (workspace->screen->display->grab_op))
    move_window = workspace->screen->display->grab_window;

  if (move_window != NULL)
    {
      /* We put the window on the new workspace, flip spaces,
       * then remove from old workspace, so the window
       * never gets unmapped and we maintain the button grab
       * on it.
       *
       * \bug  This comment appears to be the reverse of what happens
       */
      if (!meta_window_located_on_workspace (move_window, workspace))
        meta_window_change_workspace (move_window, workspace);
    }

  meta_workspace_queue_calc_showing (old);
  meta_workspace_queue_calc_showing (workspace);

   /*
    * Notify the compositor that the active workspace is changing.
    */
   screen = workspace->screen;
   display = meta_screen_get_display (screen);
   comp = meta_display_get_compositor (display);
   direction = 0;

   current_space = meta_workspace_index (old);
   new_space     = meta_workspace_index (workspace);
   num_workspaces = meta_screen_get_n_workspaces (workspace->screen);
   meta_screen_calc_workspace_layout (workspace->screen, num_workspaces,
                                      current_space, &layout1);

   meta_screen_calc_workspace_layout (workspace->screen, num_workspaces,
                                      new_space, &layout2);

   if (meta_get_locale_direction () == META_LOCALE_DIRECTION_RTL)
     {
       if (layout1.current_col > layout2.current_col)
         direction = META_MOTION_RIGHT;
       else if (layout1.current_col < layout2.current_col)
         direction = META_MOTION_LEFT;
     }
   else
    {
       if (layout1.current_col < layout2.current_col)
         direction = META_MOTION_RIGHT;
       else if (layout1.current_col > layout2.current_col)
         direction = META_MOTION_LEFT;
    }

   if (layout1.current_row < layout2.current_row)
     {
       if (!direction)
         direction = META_MOTION_DOWN;
       else if (direction == META_MOTION_RIGHT)
         direction = META_MOTION_DOWN_RIGHT;
       else
         direction = META_MOTION_DOWN_LEFT;
     }

   if (layout1.current_row > layout2.current_row)
     {
       if (!direction)
         direction = META_MOTION_UP;
       else if (direction == META_MOTION_RIGHT)
         direction = META_MOTION_UP_RIGHT;
       else
         direction = META_MOTION_UP_LEFT;
     }

   meta_screen_free_workspace_layout (&layout1);
   meta_screen_free_workspace_layout (&layout2);

   meta_compositor_switch_workspace (comp, old, workspace, direction);

  /* This needs to be done after telling the compositor we are switching
   * workspaces since focusing a window will cause it to be immediately
   * shown and that would confuse the compositor if it didn't know we
   * were in a workspace switch.
   */
  if (focus_this)
    {
      meta_window_activate (focus_this, timestamp);
    }
  else if (move_window)
    {
      meta_window_raise (move_window);
    }
  else
    {
      meta_topic (META_DEBUG_FOCUS, "Focusing default window on new workspace\n");
      meta_workspace_focus_default_window (workspace, NULL, timestamp);
    }

   /* Emit switched signal from screen.c */
   meta_screen_workspace_switched (screen, current_space, new_space, direction);
}

void
meta_workspace_activate (MetaWorkspace *workspace,
                         guint32        timestamp)
{
  meta_workspace_activate_with_focus (workspace, NULL, timestamp);
}

int
meta_workspace_index (MetaWorkspace *workspace)
{
  int ret;

  ret = g_list_index (workspace->screen->workspaces, workspace);

  if (ret < 0)
    meta_bug ("Workspace does not exist to index!\n");

  return ret;
}

void
meta_workspace_index_changed (MetaWorkspace *workspace)
{
  GList *l;
  for (l = workspace->windows; l != NULL; l = l->next)
    {
      MetaWindow *win = l->data;
      meta_window_current_workspace_changed (win);
    }

  g_object_notify_by_pspec (G_OBJECT (workspace), obj_props[PROP_WORKSPACE_INDEX]);
}

/**
 * meta_workspace_list_windows:
 * @workspace: a #MetaWorkspace
 *
 * Gets windows contained on the workspace, including workspace->windows
 * and also sticky windows. Override-redirect windows are not included.
 *
 * Return value: (transfer container) (element-type MetaWindow): the list of windows.
 */
GList*
meta_workspace_list_windows (MetaWorkspace *workspace)
{
  GSList *display_windows, *l;
  GList *workspace_windows;

  display_windows = meta_display_list_windows (workspace->screen->display,
                                               META_LIST_DEFAULT);

  workspace_windows = NULL;
  for (l = display_windows; l != NULL; l = l->next)
    {
      MetaWindow *window = l->data;

      if (meta_window_located_on_workspace (window, workspace))
        workspace_windows = g_list_prepend (workspace_windows,
                                            window);
    }

  g_slist_free (display_windows);

  return workspace_windows;
}

void
meta_workspace_invalidate_work_area (MetaWorkspace *workspace)
{
  GList *windows, *l;

  if (workspace->work_areas_invalid)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Work area for workspace %d is already invalid\n",
                  meta_workspace_index (workspace));
      return;
    }

  meta_topic (META_DEBUG_WORKAREA,
              "Invalidating work area for workspace %d\n",
              meta_workspace_index (workspace));

  /* If we are in the middle of a resize or move operation, we
   * might have cached pointers to the workspace's edges */
  if (workspace == workspace->screen->active_workspace)
    meta_display_cleanup_edges (workspace->screen->display);

  meta_workspace_clear_logical_monitor_data (workspace);

  workspace_free_all_struts (workspace);

  meta_rectangle_free_list_and_elements (workspace->screen_region);
  meta_rectangle_free_list_and_elements (workspace->screen_edges);
  meta_rectangle_free_list_and_elements (workspace->monitor_edges);
  workspace->screen_region = NULL;
  workspace->screen_edges = NULL;
  workspace->monitor_edges = NULL;

  workspace->work_areas_invalid = TRUE;

  /* redo the size/position constraints on all windows */
  windows = meta_workspace_list_windows (workspace);

  for (l = windows; l != NULL; l = l->next)
    {
      MetaWindow *w = l->data;
      meta_window_queue (w, META_QUEUE_MOVE_RESIZE);
    }

  g_list_free (windows);

  meta_screen_queue_workarea_recalc (workspace->screen);
}

static MetaStrut *
copy_strut(MetaStrut *original)
{
  return g_memdup(original, sizeof(MetaStrut));
}

static GSList *
copy_strut_list(GSList *original)
{
  GSList *result = NULL;

  for (; original != NULL; original = original->next)
    result = g_slist_prepend (result, copy_strut (original->data));

  return g_slist_reverse (result);
}

static void
ensure_work_areas_validated (MetaWorkspace *workspace)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  GList *windows;
  GList *tmp;
  GList *logical_monitors, *l;
  MetaRectangle work_area;

  if (!workspace->work_areas_invalid)
    return;

  g_assert (workspace->all_struts == NULL);
  g_assert (workspace->screen_region == NULL);
  g_assert (workspace->screen_edges == NULL);
  g_assert (workspace->monitor_edges == NULL);

  /* STEP 1: Get the list of struts */

  workspace->all_struts = copy_strut_list (workspace->builtin_struts);

  windows = meta_workspace_list_windows (workspace);
  for (tmp = windows; tmp != NULL; tmp = tmp->next)
    {
      MetaWindow *win = tmp->data;
      GSList *s_iter;

      for (s_iter = win->struts; s_iter != NULL; s_iter = s_iter->next) {
        workspace->all_struts = g_slist_prepend (workspace->all_struts,
                                                 copy_strut(s_iter->data));
      }
    }
  g_list_free (windows);

  /* STEP 2: Get the maximal/spanning rects for the onscreen and
   *         on-single-monitor regions
   */
  g_assert (workspace->screen_region   == NULL);

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      MetaWorkspaceLogicalMonitorData *data;

      g_assert (!meta_workspace_get_logical_monitor_data (workspace,
                                                          logical_monitor));

      data = meta_workspace_ensure_logical_monitor_data (workspace,
                                                         logical_monitor);
      data->logical_monitor_region =
        meta_rectangle_get_minimal_spanning_set_for_region (
          &logical_monitor->rect,
          workspace->all_struts);
    }

  workspace->screen_region =
    meta_rectangle_get_minimal_spanning_set_for_region (
      &workspace->screen->rect,
      workspace->all_struts);

  /* STEP 3: Get the work areas (region-to-maximize-to) for the screen and
   *         monitors.
   */
  work_area = workspace->screen->rect;  /* start with the screen */
  if (workspace->screen_region == NULL)
    work_area = meta_rect (0, 0, -1, -1);
  else
    meta_rectangle_clip_to_region (workspace->screen_region,
                                   FIXED_DIRECTION_NONE,
                                   &work_area);

  /* Lots of paranoia checks, forcing work_area_screen to be sane */
#define MIN_SANE_AREA 100
  if (work_area.width < MIN_SANE_AREA)
    {
      meta_warning ("struts occupy an unusually large percentage of the screen; "
                    "available remaining width = %d < %d",
                    work_area.width, MIN_SANE_AREA);
      if (work_area.width < 1)
        {
          work_area.x = (workspace->screen->rect.width - MIN_SANE_AREA)/2;
          work_area.width = MIN_SANE_AREA;
        }
      else
        {
          int amount = (MIN_SANE_AREA - work_area.width)/2;
          work_area.x     -=   amount;
          work_area.width += 2*amount;
        }
    }
  if (work_area.height < MIN_SANE_AREA)
    {
      meta_warning ("struts occupy an unusually large percentage of the screen; "
                    "available remaining height = %d < %d",
                    work_area.height, MIN_SANE_AREA);
      if (work_area.height < 1)
        {
          work_area.y = (workspace->screen->rect.height - MIN_SANE_AREA)/2;
          work_area.height = MIN_SANE_AREA;
        }
      else
        {
          int amount = (MIN_SANE_AREA - work_area.height)/2;
          work_area.y      -=   amount;
          work_area.height += 2*amount;
        }
    }
  workspace->work_area_screen = work_area;
  meta_topic (META_DEBUG_WORKAREA,
              "Computed work area for workspace %d: %d,%d %d x %d\n",
              meta_workspace_index (workspace),
              workspace->work_area_screen.x,
              workspace->work_area_screen.y,
              workspace->work_area_screen.width,
              workspace->work_area_screen.height);

  /* Now find the work areas for each monitor */
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      MetaWorkspaceLogicalMonitorData *data;

      data = meta_workspace_get_logical_monitor_data (workspace,
                                                      logical_monitor);
      work_area = logical_monitor->rect;

      if (!data->logical_monitor_region)
        /* FIXME: constraints.c untested with this, but it might be nice for
         * a screen reader or magnifier.
         */
        work_area = meta_rect (work_area.x, work_area.y, -1, -1);
      else
        meta_rectangle_clip_to_region (data->logical_monitor_region,
                                       FIXED_DIRECTION_NONE,
                                       &work_area);

      data->logical_monitor_work_area = work_area;

      meta_topic (META_DEBUG_WORKAREA,
                  "Computed work area for workspace %d "
                  "monitor %d: %d,%d %d x %d\n",
                  meta_workspace_index (workspace),
                  logical_monitor->number,
                  data->logical_monitor_work_area.x,
                  data->logical_monitor_work_area.y,
                  data->logical_monitor_work_area.width,
                  data->logical_monitor_work_area.height);
    }

  /* STEP 4: Make sure the screen_region is nonempty (separate from step 2
   *         since it relies on step 3).
   */
  if (workspace->screen_region == NULL)
    {
      MetaRectangle *nonempty_region;
      nonempty_region = g_new (MetaRectangle, 1);
      *nonempty_region = workspace->work_area_screen;
      workspace->screen_region = g_list_prepend (NULL, nonempty_region);
    }

  /* STEP 5: Cache screen and monitor edges for edge resistance and snapping */
  g_assert (workspace->screen_edges    == NULL);
  g_assert (workspace->monitor_edges  == NULL);
  workspace->screen_edges =
    meta_rectangle_find_onscreen_edges (&workspace->screen->rect,
                                        workspace->all_struts);
  tmp = NULL;
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;

      tmp = g_list_prepend (tmp, &logical_monitor->rect);
    }
  workspace->monitor_edges =
    meta_rectangle_find_nonintersected_monitor_edges (tmp,
                                                       workspace->all_struts);
  g_list_free (tmp);

  /* We're all done, YAAY!  Record that everything has been validated. */
  workspace->work_areas_invalid = FALSE;
}

static gboolean
strut_lists_equal (GSList *l,
                   GSList *m)
{
  for (; l && m; l = l->next, m = m->next)
    {
      MetaStrut *a = l->data;
      MetaStrut *b = m->data;

      if (a->side != b->side ||
          !meta_rectangle_equal (&a->rect, &b->rect))
        return FALSE;
    }

  return l == NULL && m == NULL;
}

/**
 * meta_workspace_set_builtin_struts:
 * @workspace: a #MetaWorkspace
 * @struts: (element-type Meta.Strut) (transfer none): list of #MetaStrut
 *
 * Sets a list of struts that will be used in addition to the struts
 * of the windows in the workspace when computing the work area of
 * the workspace.
 */
void
meta_workspace_set_builtin_struts (MetaWorkspace *workspace,
                                   GSList        *struts)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaScreen *screen = workspace->screen;
  GSList *l;

  for (l = struts; l; l = l->next)
    {
      MetaStrut *strut = l->data;
      MetaLogicalMonitor *logical_monitor;

      logical_monitor =
        meta_monitor_manager_get_logical_monitor_from_rect (monitor_manager,
                                                            &strut->rect);

      switch (strut->side)
        {
        case META_SIDE_TOP:
          if (meta_monitor_manager_get_logical_monitor_neighbor (monitor_manager,
                                                                 logical_monitor,
                                                                 META_SCREEN_UP))
            continue;

          strut->rect.height += strut->rect.y;
          strut->rect.y = 0;
          break;
        case META_SIDE_BOTTOM:
          if (meta_monitor_manager_get_logical_monitor_neighbor (monitor_manager,
                                                                 logical_monitor,
                                                                 META_SCREEN_DOWN))
            continue;

          strut->rect.height = screen->rect.height - strut->rect.y;
          break;
        case META_SIDE_LEFT:
          if (meta_monitor_manager_get_logical_monitor_neighbor (monitor_manager,
                                                                 logical_monitor,
                                                                 META_SCREEN_LEFT))
            continue;

          strut->rect.width += strut->rect.x;
          strut->rect.x = 0;
          break;
        case META_SIDE_RIGHT:
          if (meta_monitor_manager_get_logical_monitor_neighbor (monitor_manager,
                                                                 logical_monitor,
                                                                 META_SCREEN_RIGHT))
            continue;

          strut->rect.width = screen->rect.width - strut->rect.x;
          break;
        }
    }

  /* Reordering doesn't actually matter, so we don't catch all
   * no-impact changes, but this is just a (possibly unnecessary
   * anyways) optimization */
  if (strut_lists_equal (struts, workspace->builtin_struts))
    return;

  workspace_free_builtin_struts (workspace);
  workspace->builtin_struts = copy_strut_list (struts);

  meta_workspace_invalidate_work_area (workspace);
}

void
meta_workspace_get_work_area_for_logical_monitor (MetaWorkspace      *workspace,
                                                  MetaLogicalMonitor *logical_monitor,
                                                  MetaRectangle      *area)
{
  meta_workspace_get_work_area_for_monitor (workspace,
                                            logical_monitor->number,
                                            area);
}

/**
 * meta_workspace_get_work_area_for_monitor:
 * @workspace: a #MetaWorkspace
 * @which_monitor: a monitor index
 * @area: (out): location to store the work area
 *
 * Stores the work area for @which_monitor on @workspace
 * in @area.
 */
void
meta_workspace_get_work_area_for_monitor (MetaWorkspace *workspace,
                                          int            which_monitor,
                                          MetaRectangle *area)
{
  MetaBackend *backend = meta_get_backend();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaLogicalMonitor *logical_monitor;
  MetaWorkspaceLogicalMonitorData *data;

  logical_monitor =
    meta_monitor_manager_get_logical_monitor_from_number (monitor_manager,
                                                          which_monitor);
  g_return_if_fail (logical_monitor != NULL);

  ensure_work_areas_validated (workspace);
  data = meta_workspace_get_logical_monitor_data (workspace, logical_monitor);

  g_return_if_fail (data != NULL);

  *area = data->logical_monitor_work_area;
}

/**
 * meta_workspace_get_work_area_all_monitors:
 * @workspace: a #MetaWorkspace
 * @area: (out): location to store the work area
 *
 * Stores the work area in @area.
 */
void
meta_workspace_get_work_area_all_monitors (MetaWorkspace *workspace,
                                           MetaRectangle *area)
{
  ensure_work_areas_validated (workspace);

  *area = workspace->work_area_screen;
}

GList*
meta_workspace_get_onscreen_region (MetaWorkspace *workspace)
{
  ensure_work_areas_validated (workspace);

  return workspace->screen_region;
}

GList *
meta_workspace_get_onmonitor_region (MetaWorkspace      *workspace,
                                     MetaLogicalMonitor *logical_monitor)
{
  MetaWorkspaceLogicalMonitorData *data;

  ensure_work_areas_validated (workspace);

  data = meta_workspace_get_logical_monitor_data (workspace, logical_monitor);

  return data->logical_monitor_region;
}

#ifdef WITH_VERBOSE_MODE
static const char *
meta_motion_direction_to_string (MetaMotionDirection direction)
{
  switch (direction)
    {
    case META_MOTION_UP:
      return "Up";
    case META_MOTION_DOWN:
      return "Down";
    case META_MOTION_LEFT:
      return "Left";
    case META_MOTION_RIGHT:
      return "Right";
    case META_MOTION_UP_RIGHT:
      return "Up-Right";
    case META_MOTION_DOWN_RIGHT:
      return "Down-Right";
    case META_MOTION_UP_LEFT:
      return "Up-Left";
    case META_MOTION_DOWN_LEFT:
      return "Down-Left";
    }

  return "Unknown";
}
#endif /* WITH_VERBOSE_MODE */

/**
 * meta_workspace_get_neighbor:
 * @workspace: a #MetaWorkspace
 * @direction: a #MetaMotionDirection, relative to @workspace
 *
 * Calculate and retrive the workspace that is next to @workspace,
 * according to @direction and the current workspace layout, as set
 * by meta_screen_override_workspace_layout().
 *
 * Returns: (transfer none): the workspace next to @workspace, or
 *   @workspace itself if the neighbor would be outside the layout
 */
MetaWorkspace*
meta_workspace_get_neighbor (MetaWorkspace      *workspace,
                             MetaMotionDirection direction)
{
  MetaWorkspaceLayout layout;
  int i, current_space, num_workspaces;
  gboolean ltr;

  current_space = meta_workspace_index (workspace);
  num_workspaces = meta_screen_get_n_workspaces (workspace->screen);
  meta_screen_calc_workspace_layout (workspace->screen, num_workspaces,
                                     current_space, &layout);

  meta_verbose ("Getting neighbor of %d in direction %s\n",
                current_space, meta_motion_direction_to_string (direction));

  ltr = (meta_get_locale_direction () == META_LOCALE_DIRECTION_LTR);

  switch (direction)
    {
    case META_MOTION_LEFT:
      layout.current_col -= ltr ? 1 : -1;
      break;
    case META_MOTION_RIGHT:
      layout.current_col += ltr ? 1 : -1;
      break;
    case META_MOTION_UP:
      layout.current_row -= 1;
      break;
    case META_MOTION_DOWN:
      layout.current_row += 1;
      break;
    default:;
    }

  if (layout.current_col < 0)
    layout.current_col = 0;
  if (layout.current_col >= layout.cols)
    layout.current_col = layout.cols - 1;
  if (layout.current_row < 0)
    layout.current_row = 0;
  if (layout.current_row >= layout.rows)
    layout.current_row = layout.rows - 1;

  i = layout.grid[layout.current_row * layout.cols + layout.current_col];

  if (i < 0)
    i = current_space;

  if (i >= num_workspaces)
    meta_bug ("calc_workspace_layout left an invalid (too-high) workspace number %d in the grid\n",
              i);

  meta_verbose ("Neighbor workspace is %d at row %d col %d\n",
                i, layout.current_row, layout.current_col);

  meta_screen_free_workspace_layout (&layout);

  return meta_screen_get_workspace_by_index (workspace->screen, i);
}

const char*
meta_workspace_get_name (MetaWorkspace *workspace)
{
  return meta_prefs_get_workspace_name (meta_workspace_index (workspace));
}

void
meta_workspace_focus_default_window (MetaWorkspace *workspace,
                                     MetaWindow    *not_this_one,
                                     guint32        timestamp)
{
  if (timestamp == CurrentTime)
    meta_warning ("CurrentTime used to choose focus window; "
                  "focus window may not be correct.\n");

  if (meta_prefs_get_focus_mode () == G_DESKTOP_FOCUS_MODE_CLICK ||
      !workspace->screen->display->mouse_mode)
    focus_ancestor_or_top_window (workspace, not_this_one, timestamp);
  else
    {
      MetaWindow * window;
      window = meta_screen_get_mouse_window (workspace->screen, not_this_one);
      if (window &&
          window->type != META_WINDOW_DOCK &&
          window->type != META_WINDOW_DESKTOP)
        {
          if (timestamp == CurrentTime)
            {

              /* We would like for this to never happen.  However, if
               * it does happen then we kludge since using CurrentTime
               * can mean ugly race conditions--and we can avoid these
               * by allowing EnterNotify events (which come with
               * timestamps) to handle focus.
               */

              meta_topic (META_DEBUG_FOCUS,
                          "Not focusing mouse window %s because EnterNotify events should handle that\n", window->desc);
            }
          else
            {
              meta_topic (META_DEBUG_FOCUS,
                          "Focusing mouse window %s\n", window->desc);
              meta_window_focus (window, timestamp);
            }

          if (workspace->screen->display->autoraise_window != window &&
              meta_prefs_get_auto_raise ())
            {
              meta_display_queue_autoraise_callback (workspace->screen->display,
                                                     window);
            }
        }
      else if (meta_prefs_get_focus_mode () == G_DESKTOP_FOCUS_MODE_SLOPPY)
        focus_ancestor_or_top_window (workspace, not_this_one, timestamp);
      else if (meta_prefs_get_focus_mode () == G_DESKTOP_FOCUS_MODE_MOUSE)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Setting focus to no_focus_window, since no valid "
                      "window to focus found.\n");
          meta_display_focus_the_no_focus_window (workspace->screen->display,
                                                  workspace->screen,
                                                  timestamp);
        }
    }
}

static gboolean
record_ancestor (MetaWindow *window,
                 void       *data)
{
  MetaWindow **result = data;

  *result = window;
  return FALSE; /* quit with the first ancestor we find */
}

/* Focus ancestor of not_this_one if there is one */
static void
focus_ancestor_or_top_window (MetaWorkspace *workspace,
                              MetaWindow    *not_this_one,
                              guint32        timestamp)
{
  MetaWindow *window = NULL;

  if (not_this_one)
    meta_topic (META_DEBUG_FOCUS,
                "Focusing MRU window excluding %s\n", not_this_one->desc);
  else
    meta_topic (META_DEBUG_FOCUS,
                "Focusing MRU window\n");

  /* First, check to see if we need to focus an ancestor of a window */
  if (not_this_one)
    {
      MetaWindow *ancestor;
      ancestor = NULL;
      meta_window_foreach_ancestor (not_this_one, record_ancestor, &ancestor);
      if (ancestor != NULL &&
          meta_window_located_on_workspace (ancestor, workspace) &&
          meta_window_showing_on_its_workspace (ancestor))
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Focusing %s, ancestor of %s\n",
                      ancestor->desc, not_this_one->desc);

          meta_window_focus (ancestor, timestamp);

          /* Also raise the window if in click-to-focus */
          if (meta_prefs_get_focus_mode () == G_DESKTOP_FOCUS_MODE_CLICK)
            meta_window_raise (ancestor);

          return;
        }
    }

  window = meta_stack_get_default_focus_window (workspace->screen->stack,
                                                workspace,
                                                not_this_one);

  if (window)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing workspace MRU window %s\n", window->desc);

      meta_window_focus (window, timestamp);

      /* Also raise the window if in click-to-focus */
      if (meta_prefs_get_focus_mode () == G_DESKTOP_FOCUS_MODE_CLICK)
        meta_window_raise (window);
    }
  else
    {
      meta_topic (META_DEBUG_FOCUS, "No MRU window to focus found; focusing no_focus_window.\n");
      meta_display_focus_the_no_focus_window (workspace->screen->display,
                                              workspace->screen,
                                              timestamp);
    }
}

/**
 * meta_workspace_get_screen:
 * @workspace: a #MetaWorkspace
 *
 * Gets the #MetaScreen that the workspace is part of.
 *
 * Return value: (transfer none): the #MetaScreen for the workspace
 */
MetaScreen *
meta_workspace_get_screen (MetaWorkspace *workspace)
{
  return workspace->screen;
}

