/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:screen
 * @title: MetaScreen
 * @short_description: Mutter X screen handler
 */

#include <config.h>
#include "screen-private.h"
#include <meta/main.h>
#include "util-private.h"
#include <meta/errors.h>
#include "window-private.h"
#include "frame.h"
#include <meta/prefs.h>
#include "workspace-private.h"
#include "keybindings-private.h"
#include "stack.h"
#include <meta/compositor.h>
#include <meta/meta-enum-types.h>
#include "core.h"
#include "meta-cursor-tracker-private.h"
#include "boxes-private.h"
#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"

#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xcomposite.h>

#include <X11/Xatom.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "x11/meta-x11-display-private.h"
#include "x11/window-x11.h"
#include "x11/xprops.h"

#include "backends/x11/meta-backend-x11.h"
#include "backends/meta-cursor-sprite-xcursor.h"

static void update_num_workspaces  (MetaScreen *screen,
                                    guint32     timestamp);
static void set_workspace_names    (MetaScreen *screen);
static void prefs_changed_callback (MetaPreference pref,
                                    gpointer       data);

enum
{
  PROP_N_WORKSPACES = 1,
};

enum
{
  WORKSPACE_ADDED,
  WORKSPACE_REMOVED,
  WORKSPACE_SWITCHED,

  LAST_SIGNAL
};

static guint screen_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (MetaScreen, meta_screen, G_TYPE_OBJECT);

static void
meta_screen_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
#if 0
  MetaScreen *screen = META_SCREEN (object);
#endif

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_screen_get_property (GObject      *object,
                          guint         prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  MetaScreen *screen = META_SCREEN (object);

  switch (prop_id)
    {
    case PROP_N_WORKSPACES:
      g_value_set_int (value, meta_screen_get_n_workspaces (screen));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_screen_finalize (GObject *object)
{
  /* Actual freeing done in meta_screen_free() for now */
}

static void
meta_screen_class_init (MetaScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  object_class->get_property = meta_screen_get_property;
  object_class->set_property = meta_screen_set_property;
  object_class->finalize = meta_screen_finalize;

  pspec = g_param_spec_int ("n-workspaces",
                            "N Workspaces",
                            "Number of workspaces",
                            1, G_MAXINT, 1,
                            G_PARAM_READABLE);

  screen_signals[WORKSPACE_ADDED] =
    g_signal_new ("workspace-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  screen_signals[WORKSPACE_REMOVED] =
    g_signal_new ("workspace-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  screen_signals[WORKSPACE_SWITCHED] =
    g_signal_new ("workspace-switched",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  META_TYPE_MOTION_DIRECTION);

  g_object_class_install_property (object_class,
                                   PROP_N_WORKSPACES,
                                   pspec);
}

static void
meta_screen_init (MetaScreen *screen)
{
}

static void
reload_logical_monitors (MetaScreen *screen)
{
  GList *l;

  for (l = screen->workspaces; l != NULL; l = l->next)
    {
      MetaWorkspace *space = l->data;
      meta_workspace_invalidate_work_area (space);
    }
}

MetaScreen*
meta_screen_new (MetaDisplay *display,
                 guint32      timestamp)
{
  MetaScreen *screen;
  int number;
  Window xroot = meta_x11_display_get_xroot (display->x11_display);
  Display *xdisplay = meta_x11_display_get_xdisplay (display->x11_display);

  number = meta_ui_get_screen_number ();

  meta_verbose ("Trying screen %d on display '%s'\n",
                number, display->x11_display->name);

  screen = g_object_new (META_TYPE_SCREEN, NULL);
  screen->closing = 0;

  screen->display = display;

  screen->active_workspace = NULL;
  screen->workspaces = NULL;
  screen->rows_of_workspaces = 1;
  screen->columns_of_workspaces = -1;
  screen->vertical_workspaces = FALSE;
  screen->starting_corner = META_SCREEN_TOPLEFT;

  reload_logical_monitors (screen);

  meta_screen_update_workspace_layout (screen);

  /* Screens must have at least one workspace at all times,
   * so create that required workspace.
   */
  meta_workspace_new (screen);

  screen->keys_grabbed = FALSE;
  meta_screen_grab_keys (screen);

  screen->ui = meta_ui_new (xdisplay);

  meta_prefs_add_listener (prefs_changed_callback, screen);

  meta_verbose ("Added screen %d ('%s') root 0x%lx\n",
                number, display->x11_display->screen_name,
                xroot);

  return screen;
}

void
meta_screen_init_workspaces (MetaScreen *screen)
{
  MetaDisplay *display = screen->display;
  MetaWorkspace *current_workspace;
  uint32_t current_workspace_index = 0;
  guint32 timestamp;

  g_return_if_fail (META_IS_SCREEN (screen));

  timestamp = screen->display->x11_display->wm_sn_timestamp;

  /* Get current workspace */
  if (meta_prop_get_cardinal (display->x11_display,
                              display->x11_display->xroot,
                              display->x11_display->atom__NET_CURRENT_DESKTOP,
                              &current_workspace_index))
    meta_verbose ("Read existing _NET_CURRENT_DESKTOP = %d\n",
                  (int) current_workspace_index);
  else
    meta_verbose ("No _NET_CURRENT_DESKTOP present\n");

  update_num_workspaces (screen, timestamp);

  set_workspace_names (screen);

  /* Switch to the _NET_CURRENT_DESKTOP workspace */
  current_workspace = meta_screen_get_workspace_by_index (screen,
                                                          current_workspace_index);

  if (current_workspace != NULL)
    meta_workspace_activate (current_workspace, timestamp);
  else
    meta_workspace_activate (screen->workspaces->data, timestamp);
}

void
meta_screen_free (MetaScreen *screen,
                  guint32     timestamp)
{
  screen->closing += 1;

  meta_prefs_remove_listener (prefs_changed_callback, screen);

  meta_screen_ungrab_keys (screen);

  meta_ui_free (screen->ui);

  g_object_unref (screen);
}

static void
prefs_changed_callback (MetaPreference pref,
                        gpointer       data)
{
  MetaScreen *screen = data;

  if ((pref == META_PREF_NUM_WORKSPACES ||
       pref == META_PREF_DYNAMIC_WORKSPACES) &&
      !meta_prefs_get_dynamic_workspaces ())
    {
      /* GSettings doesn't provide timestamps, but luckily update_num_workspaces
       * often doesn't need it...
       */
      guint32 timestamp =
        meta_display_get_current_time_roundtrip (screen->display);
      update_num_workspaces (screen, timestamp);
    }
  else if (pref == META_PREF_WORKSPACE_NAMES)
    {
      set_workspace_names (screen);
    }
}

int
meta_screen_get_n_workspaces (MetaScreen *screen)
{
  return g_list_length (screen->workspaces);
}

/**
 * meta_screen_get_workspace_by_index:
 * @screen: a #MetaScreen
 * @index: index of one of the screen's workspaces
 *
 * Gets the workspace object for one of a screen's workspaces given the workspace
 * index. It's valid to call this function with an out-of-range index and it
 * will robustly return %NULL.
 *
 * Return value: (transfer none): the workspace object with specified index, or %NULL
 *   if the index is out of range.
 */
MetaWorkspace*
meta_screen_get_workspace_by_index (MetaScreen  *screen,
                                    int          idx)
{
  return g_list_nth_data (screen->workspaces, idx);
}

static void
set_number_of_spaces_hint (MetaScreen *screen,
                           int         n_spaces)
{
  MetaX11Display *x11_display = screen->display->x11_display;
  unsigned long data[1];

  if (screen->closing > 0)
    return;

  data[0] = n_spaces;

  meta_verbose ("Setting _NET_NUMBER_OF_DESKTOPS to %lu\n", data[0]);

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_NUMBER_OF_DESKTOPS,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (x11_display);
}

void
meta_screen_remove_workspace (MetaScreen *screen, MetaWorkspace *workspace,
                              guint32 timestamp)
{
  GList         *l;
  GList         *next;
  MetaWorkspace *neighbour = NULL;
  int            index;
  gboolean       active_index_changed;
  int            new_num;

  l = g_list_find (screen->workspaces, workspace);
  if (!l)
    return;

  next = l->next;

  if (l->prev)
    neighbour = l->prev->data;
  else if (l->next)
    neighbour = l->next->data;
  else
    {
      /* Cannot remove the only workspace! */
      return;
    }

  meta_workspace_relocate_windows (workspace, neighbour);

  if (workspace == screen->active_workspace)
    meta_workspace_activate (neighbour, timestamp);

  /* To emit the signal after removing the workspace */
  index = meta_workspace_index (workspace);
  active_index_changed = index < meta_screen_get_active_workspace_index (screen);

  /* This also removes the workspace from the screens list */
  meta_workspace_remove (workspace);

  new_num = g_list_length (screen->workspaces);

  set_number_of_spaces_hint (screen, new_num);

  if (!meta_prefs_get_dynamic_workspaces ())
    meta_prefs_set_num_workspaces (new_num);

  /* If deleting a workspace before the current workspace, the active
   * workspace index changes, so we need to update that hint */
  if (active_index_changed)
      meta_screen_set_active_workspace_hint (screen);

  for (l = next; l != NULL; l = l->next)
    {
      MetaWorkspace *w = l->data;
      meta_workspace_index_changed (w);
    }

  meta_display_queue_workarea_recalc (screen->display);

  g_signal_emit (screen, screen_signals[WORKSPACE_REMOVED], 0, index);
  g_object_notify (G_OBJECT (screen), "n-workspaces");
}

/**
 * meta_screen_append_new_workspace:
 * @screen: a #MetaScreen
 * @activate: %TRUE if the workspace should be switched to after creation
 * @timestamp: if switching to a new workspace, timestamp to be used when
 *   focusing a window on the new workspace. (Doesn't hurt to pass a valid
 *   timestamp when available even if not switching workspaces.)
 *
 * Append a new workspace to the screen and (optionally) switch to that
 * screen.
 *
 * Return value: (transfer none): the newly appended workspace.
 */
MetaWorkspace *
meta_screen_append_new_workspace (MetaScreen *screen, gboolean activate,
                                  guint32 timestamp)
{
  MetaWorkspace *w;
  int new_num;

  /* This also adds the workspace to the screen list */
  w = meta_workspace_new (screen);

  if (!w)
    return NULL;

  if (activate)
    meta_workspace_activate (w, timestamp);

  new_num = g_list_length (screen->workspaces);

  set_number_of_spaces_hint (screen, new_num);

  if (!meta_prefs_get_dynamic_workspaces ())
    meta_prefs_set_num_workspaces (new_num);

  meta_display_queue_workarea_recalc (screen->display);

  g_signal_emit (screen, screen_signals[WORKSPACE_ADDED],
                 0, meta_workspace_index (w));
  g_object_notify (G_OBJECT (screen), "n-workspaces");

  return w;
}


static void
update_num_workspaces (MetaScreen *screen,
                       guint32     timestamp)
{
  MetaDisplay *display = screen->display;
  int new_num, old_num;
  GList *l;
  int i;
  GList *extras;
  MetaWorkspace *last_remaining;
  gboolean need_change_space;

  if (meta_prefs_get_dynamic_workspaces ())
    {
      int n_items;
      uint32_t *list;

      n_items = 0;
      list = NULL;

      if (meta_prop_get_cardinal_list (display->x11_display,
                                       display->x11_display->xroot,
                                       display->x11_display->atom__NET_NUMBER_OF_DESKTOPS,
                                       &list, &n_items))
        {
          new_num = list[0];
          meta_XFree (list);
        }
      else
        {
          new_num = 1;
        }
    }
  else
    {
      new_num = meta_prefs_get_num_workspaces ();
    }

  g_assert (new_num > 0);

  if (g_list_length (screen->workspaces) == (guint) new_num)
    {
      if (screen->display->display_opening)
        set_number_of_spaces_hint (screen, new_num);
      return;
    }

  last_remaining = NULL;
  extras = NULL;
  i = 0;
  for (l = screen->workspaces; l != NULL; l = l->next)
    {
      MetaWorkspace *w = l->data;

      if (i >= new_num)
        extras = g_list_prepend (extras, w);
      else
        last_remaining = w;

      ++i;
    }
  old_num = i;

  g_assert (last_remaining);

  /* Get rid of the extra workspaces by moving all their windows
   * to last_remaining, then activating last_remaining if
   * one of the removed workspaces was active. This will be a bit
   * wacky if the config tool for changing number of workspaces
   * is on a removed workspace ;-)
   */
  need_change_space = FALSE;
  for (l = extras; l != NULL; l = l->next)
    {
      MetaWorkspace *w = l->data;

      meta_workspace_relocate_windows (w, last_remaining);

      if (w == screen->active_workspace)
        need_change_space = TRUE;
    }

  if (need_change_space)
    meta_workspace_activate (last_remaining, timestamp);

  /* Should now be safe to free the workspaces */
  for (l = extras; l != NULL; l = l->next)
    {
      MetaWorkspace *w = l->data;

      meta_workspace_remove (w);
    }

  g_list_free (extras);

  for (i = old_num; i < new_num; i++)
    meta_workspace_new (screen);

  set_number_of_spaces_hint (screen, new_num);

  meta_display_queue_workarea_recalc (display);

  for (i = old_num; i < new_num; i++)
    g_signal_emit (screen, screen_signals[WORKSPACE_ADDED], 0, i);

  g_object_notify (G_OBJECT (screen), "n-workspaces");
}

#define _NET_WM_ORIENTATION_HORZ 0
#define _NET_WM_ORIENTATION_VERT 1

#define _NET_WM_TOPLEFT     0
#define _NET_WM_TOPRIGHT    1
#define _NET_WM_BOTTOMRIGHT 2
#define _NET_WM_BOTTOMLEFT  3

void
meta_screen_update_workspace_layout (MetaScreen *screen)
{
  uint32_t *list;
  int n_items;
  MetaDisplay *display = screen->display;

  if (screen->workspace_layout_overridden)
    return;

  list = NULL;
  n_items = 0;

  if (meta_prop_get_cardinal_list (display->x11_display,
                                   display->x11_display->xroot,
                                   display->x11_display->atom__NET_DESKTOP_LAYOUT,
                                   &list, &n_items))
    {
      if (n_items == 3 || n_items == 4)
        {
          int cols, rows;

          switch (list[0])
            {
            case _NET_WM_ORIENTATION_HORZ:
              screen->vertical_workspaces = FALSE;
              break;
            case _NET_WM_ORIENTATION_VERT:
              screen->vertical_workspaces = TRUE;
              break;
            default:
              meta_warning ("Someone set a weird orientation in _NET_DESKTOP_LAYOUT\n");
              break;
            }

          cols = list[1];
          rows = list[2];

          if (rows <= 0 && cols <= 0)
            {
              meta_warning ("Columns = %d rows = %d in _NET_DESKTOP_LAYOUT makes no sense\n", rows, cols);
            }
          else
            {
              if (rows > 0)
                screen->rows_of_workspaces = rows;
              else
                screen->rows_of_workspaces = -1;

              if (cols > 0)
                screen->columns_of_workspaces = cols;
              else
                screen->columns_of_workspaces = -1;
            }

          if (n_items == 4)
            {
              switch (list[3])
                {
                  case _NET_WM_TOPLEFT:
                    screen->starting_corner = META_SCREEN_TOPLEFT;
                    break;
                  case _NET_WM_TOPRIGHT:
                    screen->starting_corner = META_SCREEN_TOPRIGHT;
                    break;
                  case _NET_WM_BOTTOMRIGHT:
                    screen->starting_corner = META_SCREEN_BOTTOMRIGHT;
                    break;
                  case _NET_WM_BOTTOMLEFT:
                    screen->starting_corner = META_SCREEN_BOTTOMLEFT;
                    break;
                  default:
                    meta_warning ("Someone set a weird starting corner in _NET_DESKTOP_LAYOUT\n");
                    break;
                }
            }
          else
            screen->starting_corner = META_SCREEN_TOPLEFT;
        }
      else
        {
          meta_warning ("Someone set _NET_DESKTOP_LAYOUT to %d integers instead of 4 "
                        "(3 is accepted for backwards compat)\n", n_items);
        }

      meta_XFree (list);
    }

  meta_verbose ("Workspace layout rows = %d cols = %d orientation = %d starting corner = %u\n",
                screen->rows_of_workspaces,
                screen->columns_of_workspaces,
                screen->vertical_workspaces,
                screen->starting_corner);
}

/**
 * meta_screen_override_workspace_layout:
 * @screen: a #MetaScreen
 * @starting_corner: the corner at which the first workspace is found
 * @vertical_layout: if %TRUE the workspaces are laid out in columns rather than rows
 * @n_rows: number of rows of workspaces, or -1 to determine the number of rows from
 *   @n_columns and the total number of workspaces
 * @n_columns: number of columns of workspaces, or -1 to determine the number of columns from
 *   @n_rows and the total number of workspaces
 *
 * Explicitly set the layout of workspaces. Once this has been called, the contents of the
 * _NET_DESKTOP_LAYOUT property on the root window are completely ignored.
 */
void
meta_screen_override_workspace_layout (MetaScreen      *screen,
                                       MetaScreenCorner starting_corner,
                                       gboolean         vertical_layout,
                                       int              n_rows,
                                       int              n_columns)
{
  g_return_if_fail (META_IS_SCREEN (screen));
  g_return_if_fail (n_rows > 0 || n_columns > 0);
  g_return_if_fail (n_rows != 0 && n_columns != 0);

  screen->workspace_layout_overridden = TRUE;
  screen->vertical_workspaces = vertical_layout != FALSE;
  screen->starting_corner = starting_corner;
  screen->rows_of_workspaces = n_rows;
  screen->columns_of_workspaces = n_columns;

  /* In theory we should remove _NET_DESKTOP_LAYOUT from _NET_SUPPORTED at this
   * point, but it's unlikely that anybody checks that, and it's unlikely that
   * anybody who checks that handles changes, so we'd probably just create
   * a race condition. And it's hard to implement with the code in set_supported_hint()
   */
}

static void
set_workspace_names (MetaScreen *screen)
{
  /* This updates names on root window when the pref changes,
   * note we only get prefs change notify if things have
   * really changed.
   */
  MetaX11Display *x11_display = screen->display->x11_display;
  GString *flattened;
  int i;
  int n_spaces;

  /* flatten to nul-separated list */
  n_spaces = meta_screen_get_n_workspaces (screen);
  flattened = g_string_new ("");
  i = 0;
  while (i < n_spaces)
    {
      const char *name;

      name = meta_prefs_get_workspace_name (i);

      if (name)
        g_string_append_len (flattened, name,
                             strlen (name) + 1);
      else
        g_string_append_len (flattened, "", 1);

      ++i;
    }

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_DESKTOP_NAMES,
                   x11_display->atom_UTF8_STRING,
                   8, PropModeReplace,
		   (unsigned char *)flattened->str, flattened->len);
  meta_error_trap_pop (x11_display);

  g_string_free (flattened, TRUE);
}

void
meta_screen_update_workspace_names (MetaScreen *screen)
{
  MetaX11Display *x11_display = screen->display->x11_display;
  char **names;
  int n_names;
  int i;

  /* this updates names in prefs when the root window property changes,
   * iff the new property contents don't match what's already in prefs
   */

  names = NULL;
  n_names = 0;
  if (!meta_prop_get_utf8_list (x11_display,
                                x11_display->xroot,
                                x11_display->atom__NET_DESKTOP_NAMES,
                                &names, &n_names))
    {
      meta_verbose ("Failed to get workspace names from root window\n");
      return;
    }

  i = 0;
  while (i < n_names)
    {
      meta_topic (META_DEBUG_PREFS,
                  "Setting workspace %d name to \"%s\" due to _NET_DESKTOP_NAMES change\n",
                  i, names[i] ? names[i] : "null");
      meta_prefs_change_workspace_name (i, names[i]);

      ++i;
    }

  g_strfreev (names);
}

#ifdef WITH_VERBOSE_MODE
static const char *
meta_screen_corner_to_string (MetaScreenCorner corner)
{
  switch (corner)
    {
    case META_SCREEN_TOPLEFT:
      return "TopLeft";
    case META_SCREEN_TOPRIGHT:
      return "TopRight";
    case META_SCREEN_BOTTOMLEFT:
      return "BottomLeft";
    case META_SCREEN_BOTTOMRIGHT:
      return "BottomRight";
    }

  return "Unknown";
}
#endif /* WITH_VERBOSE_MODE */

void
meta_screen_calc_workspace_layout (MetaScreen          *screen,
                                   int                  num_workspaces,
                                   int                  current_space,
                                   MetaWorkspaceLayout *layout)
{
  int rows, cols;
  int grid_area;
  int *grid;
  int i, r, c;
  int current_row, current_col;

  rows = screen->rows_of_workspaces;
  cols = screen->columns_of_workspaces;
  if (rows <= 0 && cols <= 0)
    cols = num_workspaces;

  if (rows <= 0)
    rows = num_workspaces / cols + ((num_workspaces % cols) > 0 ? 1 : 0);
  if (cols <= 0)
    cols = num_workspaces / rows + ((num_workspaces % rows) > 0 ? 1 : 0);

  /* paranoia */
  if (rows < 1)
    rows = 1;
  if (cols < 1)
    cols = 1;

  g_assert (rows != 0 && cols != 0);

  grid_area = rows * cols;

  meta_verbose ("Getting layout rows = %d cols = %d current = %d "
                "num_spaces = %d vertical = %s corner = %s\n",
                rows, cols, current_space, num_workspaces,
                screen->vertical_workspaces ? "(true)" : "(false)",
                meta_screen_corner_to_string (screen->starting_corner));

  /* ok, we want to setup the distances in the workspace array to go
   * in each direction. Remember, there are many ways that a workspace
   * array can be setup.
   * see http://www.freedesktop.org/standards/wm-spec/1.2/html/x109.html
   * and look at the _NET_DESKTOP_LAYOUT section for details.
   * For instance:
   */
  /* starting_corner = META_SCREEN_TOPLEFT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       1234                                    1357
   *       5678                                    2468
   *
   * starting_corner = META_SCREEN_TOPRIGHT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       4321                                    7531
   *       8765                                    8642
   *
   * starting_corner = META_SCREEN_BOTTOMLEFT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       5678                                    2468
   *       1234                                    1357
   *
   * starting_corner = META_SCREEN_BOTTOMRIGHT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       8765                                    8642
   *       4321                                    7531
   *
   */
  /* keep in mind that we could have a ragged layout, e.g. the "8"
   * in the above grids could be missing
   */


  grid = g_new (int, grid_area);

  current_row = -1;
  current_col = -1;
  i = 0;

  switch (screen->starting_corner)
    {
    case META_SCREEN_TOPLEFT:
      if (screen->vertical_workspaces)
        {
          c = 0;
          while (c < cols)
            {
              r = 0;
              while (r < rows)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++r;
                }
              ++c;
            }
        }
      else
        {
          r = 0;
          while (r < rows)
            {
              c = 0;
              while (c < cols)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++c;
                }
              ++r;
            }
        }
      break;
    case META_SCREEN_TOPRIGHT:
      if (screen->vertical_workspaces)
        {
          c = cols - 1;
          while (c >= 0)
            {
              r = 0;
              while (r < rows)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++r;
                }
              --c;
            }
        }
      else
        {
          r = 0;
          while (r < rows)
            {
              c = cols - 1;
              while (c >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --c;
                }
              ++r;
            }
        }
      break;
    case META_SCREEN_BOTTOMLEFT:
      if (screen->vertical_workspaces)
        {
          c = 0;
          while (c < cols)
            {
              r = rows - 1;
              while (r >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --r;
                }
              ++c;
            }
        }
      else
        {
          r = rows - 1;
          while (r >= 0)
            {
              c = 0;
              while (c < cols)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++c;
                }
              --r;
            }
        }
      break;
    case META_SCREEN_BOTTOMRIGHT:
      if (screen->vertical_workspaces)
        {
          c = cols - 1;
          while (c >= 0)
            {
              r = rows - 1;
              while (r >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --r;
                }
              --c;
            }
        }
      else
        {
          r = rows - 1;
          while (r >= 0)
            {
              c = cols - 1;
              while (c >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --c;
                }
              --r;
            }
        }
      break;
    }

  if (i != grid_area)
    meta_bug ("did not fill in the whole workspace grid in %s (%d filled)\n",
              G_STRFUNC, i);

  current_row = 0;
  current_col = 0;
  r = 0;
  while (r < rows)
    {
      c = 0;
      while (c < cols)
        {
          if (grid[r*cols+c] == current_space)
            {
              current_row = r;
              current_col = c;
            }
          else if (grid[r*cols+c] >= num_workspaces)
            {
              /* flag nonexistent spaces with -1 */
              grid[r*cols+c] = -1;
            }
          ++c;
        }
      ++r;
    }

  layout->rows = rows;
  layout->cols = cols;
  layout->grid = grid;
  layout->grid_area = grid_area;
  layout->current_row = current_row;
  layout->current_col = current_col;

#ifdef WITH_VERBOSE_MODE
  if (meta_is_verbose ())
    {
      r = 0;
      while (r < layout->rows)
        {
          meta_verbose (" ");
          meta_push_no_msg_prefix ();
          c = 0;
          while (c < layout->cols)
            {
              if (r == layout->current_row &&
                  c == layout->current_col)
                meta_verbose ("*%2d ", layout->grid[r*layout->cols+c]);
              else
                meta_verbose ("%3d ", layout->grid[r*layout->cols+c]);
              ++c;
            }
          meta_verbose ("\n");
          meta_pop_no_msg_prefix ();
          ++r;
        }
    }
#endif /* WITH_VERBOSE_MODE */
}

void
meta_screen_free_workspace_layout (MetaWorkspaceLayout *layout)
{
  g_free (layout->grid);
}

void
meta_screen_on_monitors_changed (MetaScreen *screen)
{
  reload_logical_monitors (screen);
}

void
meta_screen_update_showing_desktop_hint (MetaScreen *screen)
{
  MetaX11Display *x11_display = screen->display->x11_display;
  unsigned long data[1];

  data[0] = screen->active_workspace->showing_desktop ? 1 : 0;

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_SHOWING_DESKTOP,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (x11_display);
}

static void
queue_windows_showing (MetaScreen *screen)
{
  GSList *windows, *l;

  /* Must operate on all windows on display instead of just on the
   * active_workspace's window list, because the active_workspace's
   * window list may not contain the on_all_workspace windows.
   */
  windows = meta_display_list_windows (screen->display, META_LIST_DEFAULT);

  for (l = windows; l != NULL; l = l->next)
    {
      MetaWindow *w = l->data;
      meta_window_queue (w, META_QUEUE_CALC_SHOWING);
    }

  g_slist_free (windows);
}

void
meta_screen_minimize_all_on_active_workspace_except (MetaScreen *screen,
                                                     MetaWindow *keep)
{
  GList *l;

  for (l = screen->active_workspace->windows; l != NULL; l = l->next)
    {
      MetaWindow *w = l->data;

      if (w->has_minimize_func && w != keep)
	meta_window_minimize (w);
    }
}

void
meta_screen_show_desktop (MetaScreen *screen,
                          guint32     timestamp)
{
  GList *l;

  if (screen->active_workspace->showing_desktop)
    return;

  screen->active_workspace->showing_desktop = TRUE;

  queue_windows_showing (screen);

  /* Focus the most recently used META_WINDOW_DESKTOP window, if there is one;
   * see bug 159257.
   */
  for (l = screen->active_workspace->mru_list; l != NULL; l = l->next)
    {
      MetaWindow *w = l->data;

      if (w->type == META_WINDOW_DESKTOP)
        {
          meta_window_focus (w, timestamp);
          break;
        }
    }

  meta_screen_update_showing_desktop_hint (screen);
}

void
meta_screen_unshow_desktop (MetaScreen *screen)
{
  if (!screen->active_workspace->showing_desktop)
    return;

  screen->active_workspace->showing_desktop = FALSE;

  queue_windows_showing (screen);

  meta_screen_update_showing_desktop_hint (screen);
}

/**
 * meta_screen_get_display:
 * @screen: A #MetaScreen
 *
 * Retrieve the display associated with screen.
 *
 * Returns: (transfer none): Display
 */
MetaDisplay *
meta_screen_get_display (MetaScreen *screen)
{
  return screen->display;
}

/**
 * meta_screen_get_workspaces: (skip)
 * @screen: a #MetaScreen
 *
 * Returns: (transfer none) (element-type Meta.Workspace): The workspaces for @screen
 */
GList *
meta_screen_get_workspaces (MetaScreen *screen)
{
  return screen->workspaces;
}

int
meta_screen_get_active_workspace_index (MetaScreen *screen)
{
  MetaWorkspace *active = screen->active_workspace;

  if (!active)
    return -1;

  return meta_workspace_index (active);
}

/**
 * meta_screen_get_active_workspace:
 * @screen: A #MetaScreen
 *
 * Returns: (transfer none): The current workspace
 */
MetaWorkspace *
meta_screen_get_active_workspace (MetaScreen *screen)
{
  return screen->active_workspace;
}

void
meta_screen_focus_default_window (MetaScreen *screen,
                                  guint32     timestamp)
{
  meta_workspace_focus_default_window (screen->active_workspace,
                                       NULL,
                                       timestamp);
}

void
meta_screen_workspace_switched (MetaScreen         *screen,
                                int                 from,
                                int                 to,
                                MetaMotionDirection direction)
{
  g_signal_emit (screen, screen_signals[WORKSPACE_SWITCHED], 0,
                 from, to, direction);
}

void
meta_screen_set_active_workspace_hint (MetaScreen *screen)
{
  MetaX11Display *x11_display = screen->display->x11_display;

  unsigned long data[1];

  /* this is because we destroy the spaces in order,
   * so we always end up setting a current desktop of
   * 0 when closing a screen, so lose the current desktop
   * on restart. By doing this we keep the current
   * desktop on restart.
   */
  if (screen->closing > 0)
    return;

  data[0] = meta_workspace_index (screen->active_workspace);

  meta_verbose ("Setting _NET_CURRENT_DESKTOP to %lu\n", data[0]);

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_CURRENT_DESKTOP,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (x11_display);
}
