/* MetaWindow property handling */

/* 
 * Copyright (C) 2001, 2002, 2003 Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include "window-props.h"
#include "errors.h"
#include "xprops.h"
#include "frame.h"
#include "group.h"
#include <X11/Xatom.h>
#include <unistd.h>
#include <string.h>
#ifndef HOST_NAME_MAX
/* Solaris headers apparently don't define this so do so manually; #326745 */
#define HOST_NAME_MAX 255
#endif

typedef void (* InitValueFunc)   (MetaDisplay   *display,
                                  Atom           property,
                                  MetaPropValue *value);
typedef void (* ReloadValueFunc) (MetaWindow    *window,
                                  MetaPropValue *value);

struct _MetaWindowPropHooks
{
  Atom property;
  InitValueFunc   init_func;
  ReloadValueFunc reload_func;
};

static void init_prop_value            (MetaDisplay   *display,
                                        Atom           property,
                                        MetaPropValue *value);
static void reload_prop_value          (MetaWindow    *window,
                                        MetaPropValue *value);
static MetaWindowPropHooks* find_hooks (MetaDisplay *display,
                                        Atom         property);


void
meta_window_reload_property (MetaWindow *window,
                             Atom        property)
{
  meta_window_reload_properties (window, &property, 1);
}

void
meta_window_reload_properties (MetaWindow *window,
                               const Atom *properties,
                               int         n_properties)
{
  int i;
  MetaPropValue *values;

  g_return_if_fail (properties != NULL);
  g_return_if_fail (n_properties > 0);
  
  values = g_new0 (MetaPropValue, n_properties);
  
  i = 0;
  while (i < n_properties)
    {
      init_prop_value (window->display, properties[i], &values[i]);
      ++i;
    }
  
  meta_prop_get_values (window->display, window->xwindow,
                        values, n_properties);

  i = 0;
  while (i < n_properties)
    {
      reload_prop_value (window, &values[i]);
      
      ++i;
    }

  meta_prop_free_values (values, n_properties);
  
  g_free (values);
}

/* Fill in the MetaPropValue used to get the value of "property" */
static void
init_prop_value (MetaDisplay   *display,
                 Atom           property,
                 MetaPropValue *value)
{
  MetaWindowPropHooks *hooks;  

  value->type = META_PROP_VALUE_INVALID;
  value->atom = None;
  
  hooks = find_hooks (display, property);
  if (hooks && hooks->init_func != NULL)
    (* hooks->init_func) (display, property, value);
}

static void
reload_prop_value (MetaWindow    *window,
                   MetaPropValue *value)
{
  MetaWindowPropHooks *hooks;  
  
  hooks = find_hooks (window->display, value->atom);
  if (hooks && hooks->reload_func != NULL)
    (* hooks->reload_func) (window, value);
}

static void
init_wm_client_machine (MetaDisplay   *display,
                        Atom           property,
                        MetaPropValue *value)
{
  value->type = META_PROP_VALUE_STRING;
  value->atom = display->atom_wm_client_machine;
}

static void
reload_wm_client_machine (MetaWindow    *window,
                          MetaPropValue *value)
{
  g_free (window->wm_client_machine);
  window->wm_client_machine = NULL;
  
  if (value->type != META_PROP_VALUE_INVALID)
    window->wm_client_machine = g_strdup (value->v.str);

  meta_verbose ("Window has client machine \"%s\"\n",
                window->wm_client_machine ? window->wm_client_machine : "unset");
}

static void
init_net_wm_pid (MetaDisplay   *display,
                 Atom           property,
                 MetaPropValue *value)
{
  value->type = META_PROP_VALUE_CARDINAL;
  value->atom = display->atom_net_wm_pid;
}

static void
reload_net_wm_pid (MetaWindow    *window,
                   MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      gulong cardinal = (int) value->v.cardinal;
      
      if (cardinal <= 0)
        meta_warning (_("Application set a bogus _NET_WM_PID %lu\n"),
                      cardinal);
      else
        {
          window->net_wm_pid = cardinal;
          meta_verbose ("Window has _NET_WM_PID %d\n",
                        window->net_wm_pid);
        }
    }
}

static void
init_net_wm_user_time (MetaDisplay   *display,
                       Atom           property,
                       MetaPropValue *value)
{
  value->type = META_PROP_VALUE_CARDINAL;
  value->atom = display->atom_net_wm_user_time;
}

static void
reload_net_wm_user_time (MetaWindow    *window,
                         MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      gulong cardinal = value->v.cardinal;
      meta_window_set_user_time (window, cardinal);
    }
}

#define MAX_TITLE_LENGTH 512

/**
 * Called by set_window_title and set_icon_title to set the value of
 * *target to title. It required and atom is set, it will update the
 * appropriate property.
 *
 * Returns TRUE if a new title was set.
 */
static gboolean
set_title_text (MetaWindow  *window,
                gboolean     previous_was_modified,
                const char  *title,
                Atom         atom,
                char       **target)
{
  char hostname[HOST_NAME_MAX + 1];
  gboolean modified = FALSE;
  
  if (!target)
    return FALSE;
  
  g_free (*target);
  
  if (!title)
    *target = g_strdup ("");
  else if (g_utf8_strlen (title, MAX_TITLE_LENGTH + 1) > MAX_TITLE_LENGTH)
    {
      *target = meta_g_utf8_strndup (title, MAX_TITLE_LENGTH);
      modified = TRUE;
    }
  /* if WM_CLIENT_MACHINE indicates this machine is on a remote host
   * lets place that hostname in the title */
  else if (window->wm_client_machine &&
           !gethostname (hostname, HOST_NAME_MAX + 1) &&
           strcmp (hostname, window->wm_client_machine))
    {
      *target = g_strdup_printf (_("%s (on %s)"),
                      title, window->wm_client_machine);
      modified = TRUE;
    }
  else
    *target = g_strdup (title);

  if (modified && atom != None)
    meta_prop_set_utf8_string_hint (window->display,
                                    window->xwindow,
                                    atom, *target);

  /* Bug 330671 -- Don't forget to clear _NET_WM_VISIBLE_(ICON_)NAME */
  if (!modified && previous_was_modified)
    {
      meta_error_trap_push (window->display);
      XDeleteProperty (window->display->xdisplay,
                       window->xwindow,
                       atom);
      meta_error_trap_pop (window->display, FALSE);
    }

  return modified;
}

static void
set_window_title (MetaWindow *window,
                  const char *title)
{
  char *str;
 
  gboolean modified =
    set_title_text (window,
                    window->using_net_wm_visible_name,
                    title,
                    window->display->atom_net_wm_visible_name,
                    &window->title);
  window->using_net_wm_visible_name = modified;
  
  /* strndup is a hack since GNU libc has broken %.10s */
  str = g_strndup (window->title, 10);
  g_free (window->desc);
  window->desc = g_strdup_printf ("0x%lx (%s)", window->xwindow, str);
  g_free (str);

  if (window->frame)
    meta_ui_set_frame_title (window->screen->ui,
                             window->frame->xwindow,
                             window->title);
}

static void
init_net_wm_name (MetaDisplay   *display,
                  Atom           property,
                  MetaPropValue *value)
{
  value->type = META_PROP_VALUE_UTF8;
  value->atom = display->atom_net_wm_name;
}

static void
reload_net_wm_name (MetaWindow    *window,
                    MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      set_window_title (window, value->v.str);
      window->using_net_wm_name = TRUE;

      meta_verbose ("Using _NET_WM_NAME for new title of %s: \"%s\"\n",
                    window->desc, window->title);
    }
  else
    {
      set_window_title (window, NULL);
      window->using_net_wm_name = FALSE;
    }
}


static void
init_wm_name (MetaDisplay   *display,
              Atom           property,
              MetaPropValue *value)
{
  value->type = META_PROP_VALUE_TEXT_PROPERTY;
  value->atom = XA_WM_NAME;
}

static void
reload_wm_name (MetaWindow    *window,
                MetaPropValue *value)
{
  if (window->using_net_wm_name)
    {
      meta_verbose ("Ignoring WM_NAME \"%s\" as _NET_WM_NAME is set\n",
                    value->v.str);
      return;
    }
  
  if (value->type != META_PROP_VALUE_INVALID)
    {
      set_window_title (window, value->v.str);

      meta_verbose ("Using WM_NAME for new title of %s: \"%s\"\n",
                    window->desc, window->title);
    }
  else
    {
      set_window_title (window, NULL);
    }
}

static void
set_icon_title (MetaWindow *window,
                const char *title)
{
  gboolean modified =
    set_title_text (window,
                    window->using_net_wm_visible_icon_name,
                    title,
                    window->display->atom_net_wm_visible_icon_name,
                    &window->icon_name);
  window->using_net_wm_visible_icon_name = modified;
}

static void
init_net_wm_icon_name (MetaDisplay   *display,
                  Atom           property,
                  MetaPropValue *value)
{
  value->type = META_PROP_VALUE_UTF8;
  value->atom = display->atom_net_wm_icon_name;
}

static void
reload_net_wm_icon_name (MetaWindow    *window,
                    MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      set_icon_title (window, value->v.str);
      window->using_net_wm_icon_name = TRUE;

      meta_verbose ("Using _NET_WM_ICON_NAME for new title of %s: \"%s\"\n",
                    window->desc, window->title);
    }
  else
    {
      set_icon_title (window, NULL);
      window->using_net_wm_icon_name = FALSE;
    }
}


static void
init_wm_icon_name (MetaDisplay   *display,
                  Atom           property,
                  MetaPropValue *value)
{
  value->type = META_PROP_VALUE_TEXT_PROPERTY;
  value->atom = XA_WM_ICON_NAME;
}

static void
reload_wm_icon_name (MetaWindow    *window,
                     MetaPropValue *value)
{
  if (window->using_net_wm_icon_name)
    {
      meta_verbose ("Ignoring WM_ICON_NAME \"%s\" as _NET_WM_ICON_NAME is set\n",
                    value->v.str);
      return;
    }
  
  if (value->type != META_PROP_VALUE_INVALID)
    {
      set_icon_title (window, value->v.str);
      
      meta_verbose ("Using WM_ICON_NAME for new title of %s: \"%s\"\n",
                    window->desc, window->title);
    }
  else
    {
      set_icon_title (window, NULL);
    }
}

static void
init_net_wm_state (MetaDisplay    *display,
                   Atom            property,
                   MetaPropValue  *value)
{
  value->type = META_PROP_VALUE_ATOM_LIST;
  value->atom = display->atom_net_wm_state;
}

static void
reload_net_wm_state (MetaWindow    *window,
                     MetaPropValue *value)
{
  int i;

  /* We know this is only an initial window creation,
   * clients don't change the property.
   */

  window->shaded = FALSE;
  window->maximized_horizontally = FALSE;
  window->maximized_vertically = FALSE;
  window->wm_state_modal = FALSE;
  window->wm_state_skip_taskbar = FALSE;
  window->wm_state_skip_pager = FALSE;
  window->wm_state_above = FALSE;
  window->wm_state_below = FALSE;
  window->wm_state_demands_attention = FALSE;

  if (value->type == META_PROP_VALUE_INVALID)
    return;

  i = 0;
  while (i < value->v.atom_list.n_atoms)
    {
      if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_shaded)
        window->shaded = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_maximized_horz)
        window->maximize_horizontally_after_placement = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_maximized_vert)
        window->maximize_vertically_after_placement = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_modal)
        window->wm_state_modal = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_skip_taskbar)
        window->wm_state_skip_taskbar = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_skip_pager)
        window->wm_state_skip_pager = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_fullscreen)
        window->fullscreen = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_above)
        window->wm_state_above = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_below)
        window->wm_state_below = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom_net_wm_state_demands_attention)
        window->wm_state_demands_attention = TRUE;

      ++i;
    }

  meta_verbose ("Reloaded _NET_WM_STATE for %s\n",
                window->desc);

  meta_window_recalc_window_type (window);
}

static void
init_mwm_hints (MetaDisplay    *display,
                Atom            property,
                MetaPropValue  *value)
{
  value->type = META_PROP_VALUE_MOTIF_HINTS;
  value->atom = display->atom_motif_wm_hints;
}

static void
reload_mwm_hints (MetaWindow    *window,
                  MetaPropValue *value)
{
  MotifWmHints *hints;

  window->mwm_decorated = TRUE;
  window->mwm_border_only = FALSE;
  window->mwm_has_close_func = TRUE;
  window->mwm_has_minimize_func = TRUE;
  window->mwm_has_maximize_func = TRUE;
  window->mwm_has_move_func = TRUE;
  window->mwm_has_resize_func = TRUE;

  if (value->type == META_PROP_VALUE_INVALID)
    {
      meta_verbose ("Window %s has no MWM hints\n", window->desc);
      meta_window_recalc_features (window);
      return;
    }

  hints = value->v.motif_hints;

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

  meta_window_recalc_features (window);
  
  /* We do all this anyhow at the end of meta_window_new() */
  if (!window->constructing)
    {
      if (window->decorated)
        meta_window_ensure_frame (window);
      else
        meta_window_destroy_frame (window);
      
      meta_window_queue_move_resize (window);
      /* because ensure/destroy frame may unmap */
      meta_window_queue_calc_showing (window);
    }
}

static void
init_wm_class (MetaDisplay    *display,
               Atom            property,
               MetaPropValue  *value)
{
  value->type = META_PROP_VALUE_CLASS_HINT;
  value->atom = XA_WM_CLASS;
}

static void
reload_wm_class (MetaWindow    *window,
                 MetaPropValue *value)
{
  if (window->res_class)
    g_free (window->res_class);
  if (window->res_name)
    g_free (window->res_name);

  window->res_class = NULL;
  window->res_name = NULL;

  if (value->type != META_PROP_VALUE_INVALID)
    { 
      if (value->v.class_hint.res_name)
        window->res_name = g_strdup (value->v.class_hint.res_name);

      if (value->v.class_hint.res_class)
        window->res_class = g_strdup (value->v.class_hint.res_class);
    }

  meta_verbose ("Window %s class: '%s' name: '%s'\n",
      window->desc,
      window->res_class ? window->res_class : "none",
      window->res_name ? window->res_name : "none");
}

static void
init_net_wm_desktop (MetaDisplay   *display,
                     Atom           property,
                     MetaPropValue *value)
{
  value->type = META_PROP_VALUE_CARDINAL;
  value->atom = display->atom_net_wm_desktop;
}

static void
reload_net_wm_desktop (MetaWindow    *window,
                       MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      window->initial_workspace_set = TRUE;
      window->initial_workspace = value->v.cardinal;
      meta_topic (META_DEBUG_PLACEMENT,
                  "Read initial workspace prop %d for %s\n",
                  window->initial_workspace, window->desc);
    }
}

static void
init_net_startup_id (MetaDisplay   *display,
                     Atom           property,
                     MetaPropValue *value)
{
  value->type = META_PROP_VALUE_UTF8;
  value->atom = display->atom_net_startup_id;
}

static void
reload_net_startup_id (MetaWindow    *window,
                       MetaPropValue *value)
{
  guint32 timestamp = window->net_wm_user_time;
  MetaWorkspace *workspace = NULL;
  
  g_free (window->startup_id);
  
  if (value->type != META_PROP_VALUE_INVALID)
    window->startup_id = g_strdup (value->v.str);
  else
    window->startup_id = NULL;
    
  /* Update timestamp and workspace on a running window */
  if (!window->constructing)
  {
    window->initial_timestamp_set = 0;  
    window->initial_workspace_set = 0;
    
    meta_screen_apply_startup_properties (window->screen, window);
  
    if (window->initial_timestamp_set)
      timestamp = window->initial_timestamp;
    if (window->initial_workspace_set)
      workspace = meta_screen_get_workspace_by_index (window->screen, window->initial_workspace);
    
    meta_window_activate_with_workspace (window, timestamp, workspace);
  }
  
  meta_verbose ("New _NET_STARTUP_ID \"%s\" for %s\n",
                window->startup_id ? window->startup_id : "unset",
                window->desc);
}

static void
init_update_counter (MetaDisplay   *display,
                     Atom           property,
                     MetaPropValue *value)
{
  value->type = META_PROP_VALUE_SYNC_COUNTER;
  value->atom = display->atom_net_wm_sync_request_counter;
}

static void
reload_update_counter (MetaWindow    *window,
                       MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
#ifdef HAVE_XSYNC
      XSyncCounter counter = value->v.xcounter;

      window->sync_request_counter = counter;
      meta_verbose ("Window has _NET_WM_SYNC_REQUEST_COUNTER 0x%lx\n",
                    window->sync_request_counter);
#endif
    }
}


static void
init_normal_hints (MetaDisplay   *display,
                   Atom           property,
                   MetaPropValue *value)
{
  value->type = META_PROP_VALUE_SIZE_HINTS;
  value->atom = XA_WM_NORMAL_HINTS;
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

void
meta_set_normal_hints (MetaWindow *window,
		       XSizeHints *hints)
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

  /* as far as I can tell, value->v.size_hints.flags is just to
   * check whether we had old-style normal hints without gravity,
   * base size as returned by XGetNormalHints(), so we don't
   * really use it as we fixup window->size_hints to have those
   * fields if they're missing.
   */

  /*
   * When the window is first created, NULL hints will
   * be passed in which will initialize all of the fields
   * as if flags were zero
   */
  if (hints)
    window->size_hints = *hints;
  else
    window->size_hints.flags = 0;

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

  if (window->size_hints.min_width < 1)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
		  "Window %s sets min width to 0, which makes no sense\n",
		  window->desc);
      window->size_hints.min_width = 1;
    }
  if (window->size_hints.max_width < 1)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
		  "Window %s sets max width to 0, which makes no sense\n",
		  window->desc);
      window->size_hints.max_width = 1;
    }
  if (window->size_hints.min_height < 1)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
		  "Window %s sets min height to 0, which makes no sense\n",
		  window->desc);
      window->size_hints.min_height = 1;
    }
  if (window->size_hints.max_height < 1)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
		  "Window %s sets max height to 0, which makes no sense\n",
		  window->desc);
      window->size_hints.max_height = 1;
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
}

static void
reload_normal_hints (MetaWindow    *window,
                     MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      XSizeHints old_hints;
  
      meta_topic (META_DEBUG_GEOMETRY, "Updating WM_NORMAL_HINTS for %s\n", window->desc);

      old_hints = window->size_hints;
  
      meta_set_normal_hints (window, value->v.size_hints.hints);
      
      spew_size_hints_differences (&old_hints, &window->size_hints);
      
      meta_window_recalc_features (window);
    }
}


static void
init_wm_protocols (MetaDisplay   *display,
                   Atom           property,
                   MetaPropValue *value)
{
  value->type = META_PROP_VALUE_ATOM_LIST;
  value->atom = display->atom_wm_protocols;
}

static void
reload_wm_protocols (MetaWindow    *window,
                     MetaPropValue *value)
{
  int i;
  
  window->take_focus = FALSE;
  window->delete_window = FALSE;
  window->net_wm_ping = FALSE;
  
  if (value->type == META_PROP_VALUE_INVALID)    
    return;

  i = 0;
  while (i < value->v.atom_list.n_atoms)
    {
      if (value->v.atom_list.atoms[i] ==
          window->display->atom_wm_take_focus)
        window->take_focus = TRUE;
      else if (value->v.atom_list.atoms[i] ==
               window->display->atom_wm_delete_window)
        window->delete_window = TRUE;
      else if (value->v.atom_list.atoms[i] ==
               window->display->atom_net_wm_ping)
        window->net_wm_ping = TRUE;
      ++i;
    }
  
  meta_verbose ("New _NET_STARTUP_ID \"%s\" for %s\n",
                window->startup_id ? window->startup_id : "unset",
                window->desc);
}

static void
init_wm_hints (MetaDisplay   *display,
               Atom           property,
               MetaPropValue *value)
{
  value->type = META_PROP_VALUE_WM_HINTS;
  value->atom = XA_WM_HINTS;
}

static void
reload_wm_hints (MetaWindow    *window,
                 MetaPropValue *value)
{
  Window old_group_leader;
  
  old_group_leader = window->xgroup_leader;
  
  /* Fill in defaults */
  window->input = TRUE;
  window->initially_iconic = FALSE;
  window->xgroup_leader = None;
  window->wm_hints_pixmap = None;
  window->wm_hints_mask = None;
  
  if (value->type != META_PROP_VALUE_INVALID)
    {
      const XWMHints *hints = value->v.wm_hints;
      
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
    }

  if (window->xgroup_leader != old_group_leader)
    {
      meta_verbose ("Window %s changed its group leader to 0x%lx\n",
                    window->desc, window->xgroup_leader);
      
      meta_window_group_leader_changed (window);
    }

  meta_icon_cache_property_changed (&window->icon_cache,
                                    window->display,
                                    XA_WM_HINTS);

  meta_window_queue_update_icon (window);
      
  meta_window_queue_move_resize (window);  
}

static void
init_transient_for (MetaDisplay   *display,
                    Atom           property,
                    MetaPropValue *value)
{
  value->type = META_PROP_VALUE_WINDOW;
  value->atom = XA_WM_TRANSIENT_FOR;
}

static void
reload_transient_for (MetaWindow    *window,
                      MetaPropValue *value)
{
  window->xtransient_for = None;
  
  if (value->type != META_PROP_VALUE_INVALID)
    window->xtransient_for = value->v.xwindow;

  /* Make sure transient_for is valid */
  if (window->xtransient_for != None &&
      meta_display_lookup_x_window (window->display, 
                                    window->xtransient_for) == NULL)
    {
      meta_warning (_("Invalid WM_TRANSIENT_FOR window 0x%lx specified "
                      "for %s.\n"),
                    window->xtransient_for, window->desc);
      window->xtransient_for = None;
    }

  window->transient_parent_is_root_window =
    window->xtransient_for == window->screen->xroot;

  if (window->xtransient_for != None)
    meta_verbose ("Window %s transient for 0x%lx (root = %d)\n", window->desc,
        window->xtransient_for, window->transient_parent_is_root_window);
  else
    meta_verbose ("Window %s is not transient\n", window->desc);

  /* may now be a dialog */
  meta_window_recalc_window_type (window);

  /* update stacking constraints */
  meta_stack_update_transient (window->screen->stack, window);

  /* possibly change its group. We treat being a window's transient as
   * equivalent to making it your group leader, to work around shortcomings
   * in programs such as xmms-- see #328211.
   */
  if (window->xtransient_for != None &&
      window->xgroup_leader != None &&
      window->xtransient_for != window->xgroup_leader)
    meta_window_group_leader_changed (window);

  if (!window->constructing)
    meta_window_queue_move_resize (window);
}

#define N_HOOKS 25

void
meta_display_init_window_prop_hooks (MetaDisplay *display)
{
  int i;
  MetaWindowPropHooks *hooks;
  
  g_assert (display->prop_hooks == NULL);

  display->prop_hooks = g_new0 (MetaWindowPropHooks, N_HOOKS); 
  hooks = display->prop_hooks;
  
  i = 0;

  hooks[i].property = display->atom_wm_state;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_wm_client_machine;
  hooks[i].init_func = init_wm_client_machine;
  hooks[i].reload_func = reload_wm_client_machine;
  ++i;

  hooks[i].property = display->atom_net_wm_pid;
  hooks[i].init_func = init_net_wm_pid;
  hooks[i].reload_func = reload_net_wm_pid;
  ++i;

  hooks[i].property = display->atom_net_wm_user_time;
  hooks[i].init_func = init_net_wm_user_time;
  hooks[i].reload_func = reload_net_wm_user_time;
  ++i;

  hooks[i].property = display->atom_net_wm_name;
  hooks[i].init_func = init_net_wm_name;
  hooks[i].reload_func = reload_net_wm_name;
  ++i;

  hooks[i].property = XA_WM_NAME;
  hooks[i].init_func = init_wm_name;
  hooks[i].reload_func = reload_wm_name;
  ++i;

  hooks[i].property = display->atom_net_wm_icon_name;
  hooks[i].init_func = init_net_wm_icon_name;
  hooks[i].reload_func = reload_net_wm_icon_name;
  ++i;

  hooks[i].property = XA_WM_ICON_NAME;
  hooks[i].init_func = init_wm_icon_name;
  hooks[i].reload_func = reload_wm_icon_name;
  ++i;

  hooks[i].property = display->atom_net_wm_state;
  hooks[i].init_func = init_net_wm_state;
  hooks[i].reload_func = reload_net_wm_state;
  ++i;
  
  hooks[i].property = display->atom_motif_wm_hints;
  hooks[i].init_func = init_mwm_hints;
  hooks[i].reload_func = reload_mwm_hints;
  ++i;

  hooks[i].property = display->atom_net_wm_icon_geometry;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = XA_WM_CLASS;
  hooks[i].init_func = init_wm_class;
  hooks[i].reload_func = reload_wm_class;
  ++i;

  hooks[i].property = display->atom_wm_client_leader;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_sm_client_id;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_wm_window_role;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_net_wm_window_type;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_net_wm_desktop;
  hooks[i].init_func = init_net_wm_desktop;
  hooks[i].reload_func = reload_net_wm_desktop;
  ++i;

  hooks[i].property = display->atom_net_wm_strut;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_net_wm_strut_partial;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_net_startup_id;
  hooks[i].init_func = init_net_startup_id;
  hooks[i].reload_func = reload_net_startup_id;
  ++i;

  hooks[i].property = display->atom_net_wm_sync_request_counter;
  hooks[i].init_func = init_update_counter;
  hooks[i].reload_func = reload_update_counter;
  ++i;

  hooks[i].property = XA_WM_NORMAL_HINTS;
  hooks[i].init_func = init_normal_hints;
  hooks[i].reload_func = reload_normal_hints;
  ++i;

  hooks[i].property = display->atom_wm_protocols;
  hooks[i].init_func = init_wm_protocols;
  hooks[i].reload_func = reload_wm_protocols;
  ++i;

  hooks[i].property = XA_WM_HINTS;
  hooks[i].init_func = init_wm_hints;
  hooks[i].reload_func = reload_wm_hints;
  ++i;
  
  hooks[i].property = XA_WM_TRANSIENT_FOR;
  hooks[i].init_func = init_transient_for;
  hooks[i].reload_func = reload_transient_for;
  ++i;

  if (i != N_HOOKS)
    g_error ("Initialized %d hooks should have been %d\n", i, N_HOOKS);
}

void
meta_display_free_window_prop_hooks (MetaDisplay *display)
{
  g_assert (display->prop_hooks != NULL);
  
  g_free (display->prop_hooks);
  display->prop_hooks = NULL;
}

static MetaWindowPropHooks*
find_hooks (MetaDisplay *display,
            Atom         property)
{
  int i;

  /* FIXME we could sort the array and do binary search or
   * something
   */
  
  i = 0;
  while (i < N_HOOKS)
    {
      if (display->prop_hooks[i].property == property)
        return &display->prop_hooks[i];
      
      ++i;
    }

  return NULL;
}
