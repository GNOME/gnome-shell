/* MetaWindow property handling */

/* 
 * Copyright (C) 2001, 2002 Red Hat, Inc.
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
#include "xprops.h"
#include "frame.h"
#include <X11/Xatom.h>

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
        meta_warning (_("Application set a bogus _NET_WM_PID %ld\n"),
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
set_window_title (MetaWindow *window,
                  const char *title)
{
  char *str;
  
  g_free (window->title);
  
  if (title == NULL)
    window->title = g_strdup ("");
  else
    window->title = g_strdup (title);
  
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
  value->type = META_PROP_VALUE_STRING_AS_UTF8;
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
  g_free (window->icon_name);
  
  if (title == NULL)
    window->icon_name = g_strdup ("");
  else
    window->icon_name = g_strdup (title);
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
  value->type = META_PROP_VALUE_STRING_AS_UTF8;
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
init_win_workspace (MetaDisplay   *display,
                    Atom           property,
                    MetaPropValue *value)
{
  value->type = META_PROP_VALUE_CARDINAL;
  value->atom = display->atom_win_workspace;
}

static void
reload_win_workspace (MetaWindow    *window,
                      MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      meta_topic (META_DEBUG_PLACEMENT,
                  "Read legacy GNOME workspace prop %d for %s\n",
                  (int) value->v.cardinal, window->desc);

      if (window->initial_workspace_set)
        {
          meta_topic (META_DEBUG_PLACEMENT,
                      "Ignoring legacy GNOME workspace prop %d for %s as we already set initial workspace\n",
                      (int) value->v.cardinal, window->desc);          
        }
      else
        {
          window->initial_workspace_set = TRUE;
          window->initial_workspace = value->v.cardinal;
        }
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
  g_free (window->startup_id);
  
  if (value->type != META_PROP_VALUE_INVALID)
    window->startup_id = g_strdup (value->v.str);
  else
    window->startup_id = NULL;
  
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
  value->atom = display->atom_metacity_update_counter;
}

static void
reload_update_counter (MetaWindow    *window,
                       MetaPropValue *value)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
#ifdef HAVE_XSYNC
      XSyncCounter counter = value->v.xcounter;

      window->update_counter = counter;
      meta_verbose ("Window has _METACITY_UPDATE_COUNTER 0x%lx\n",
                    window->update_counter);
#endif
    }
}

#define N_HOOKS 24

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

  hooks[i].property = XA_WM_NORMAL_HINTS;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
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
  
  hooks[i].property = XA_WM_HINTS;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_net_wm_state;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;
  
  hooks[i].property = display->atom_motif_wm_hints;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_net_wm_icon_geometry;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = XA_WM_CLASS;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
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

  hooks[i].property = display->atom_win_layer;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_net_wm_desktop;
  hooks[i].init_func = init_net_wm_desktop;
  hooks[i].reload_func = reload_net_wm_desktop;
  ++i;

  hooks[i].property = display->atom_win_workspace;
  hooks[i].init_func = init_win_workspace;
  hooks[i].reload_func = reload_win_workspace;
  ++i;

  hooks[i].property = display->atom_net_wm_strut;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_win_hints;
  hooks[i].init_func = NULL;
  hooks[i].reload_func = NULL;
  ++i;

  hooks[i].property = display->atom_net_startup_id;
  hooks[i].init_func = init_net_startup_id;
  hooks[i].reload_func = reload_net_startup_id;
  ++i;

  hooks[i].property = display->atom_metacity_update_counter;
  hooks[i].init_func = init_update_counter;
  hooks[i].reload_func = reload_update_counter;
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
