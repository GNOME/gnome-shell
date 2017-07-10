/*
 * Copyright (C) 2017 Red Hat
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

#include "config.h"

#include "backends/x11/cm/meta-backend-x11-cm.h"

#include <stdlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "backends/meta-backend-private.h"
#include "backends/x11/meta-cursor-renderer-x11.h"
#include "backends/x11/meta-input-settings-x11.h"
#include "backends/x11/meta-monitor-manager-xrandr.h"
#include "backends/x11/cm/meta-renderer-x11-cm.h"

struct _MetaBackendX11Cm
{
  MetaBackendX11 parent;

  char *keymap_layouts;
  char *keymap_variants;
  char *keymap_options;
  int locked_group;
};

G_DEFINE_TYPE (MetaBackendX11Cm, meta_backend_x11_cm, META_TYPE_BACKEND_X11)

static void
apply_keymap (MetaBackendX11 *x11);

static void
take_touch_grab (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  Display *xdisplay = meta_backend_x11_get_xdisplay (x11);
  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { META_VIRTUAL_CORE_POINTER_ID, sizeof (mask_bits), mask_bits };
  XIGrabModifiers mods = { XIAnyModifier, 0 };

  XISetMask (mask.mask, XI_TouchBegin);
  XISetMask (mask.mask, XI_TouchUpdate);
  XISetMask (mask.mask, XI_TouchEnd);

  XIGrabTouchBegin (xdisplay, META_VIRTUAL_CORE_POINTER_ID,
                    DefaultRootWindow (xdisplay),
                    False, &mask, 1, &mods);
}

static void
on_device_added (ClutterDeviceManager *device_manager,
                 ClutterInputDevice   *device,
                 gpointer              user_data)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (user_data);

  if (clutter_input_device_get_device_type (device) == CLUTTER_KEYBOARD_DEVICE)
    apply_keymap (x11);
}

static void
meta_backend_x11_cm_post_init (MetaBackend *backend)
{
  MetaBackendClass *parent_backend_class =
    META_BACKEND_CLASS (meta_backend_x11_cm_parent_class);

  parent_backend_class->post_init (backend);

  g_signal_connect_object (clutter_device_manager_get_default (),
                           "device-added",
                           G_CALLBACK (on_device_added), backend, 0);

  take_touch_grab (backend);
}

static MetaRenderer *
meta_backend_x11_cm_create_renderer (MetaBackend *backend,
                                     GError     **error)
{
  return g_object_new (META_TYPE_RENDERER_X11_CM, NULL);
}

static MetaMonitorManager *
meta_backend_x11_cm_create_monitor_manager (MetaBackend *backend,
                                            GError     **error)
{
  return g_object_new (META_TYPE_MONITOR_MANAGER_XRANDR,
                       "backend", backend,
                       NULL);
}

static MetaCursorRenderer *
meta_backend_x11_cm_create_cursor_renderer (MetaBackend *backend)
{
  return g_object_new (META_TYPE_CURSOR_RENDERER_X11, NULL);
}

static MetaInputSettings *
meta_backend_x11_cm_create_input_settings (MetaBackend *backend)
{
  return g_object_new (META_TYPE_INPUT_SETTINGS_X11, NULL);
}

static void
meta_backend_x11_cm_update_screen_size (MetaBackend *backend,
                                        int          width,
                                        int          height)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  Display *xdisplay = meta_backend_x11_get_xdisplay (x11);
  Window xwin = meta_backend_x11_get_xwindow (x11);

  XResizeWindow (xdisplay, xwin, width, height);
}

static void
meta_backend_x11_cm_select_stage_events (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  Display *xdisplay = meta_backend_x11_get_xdisplay (x11);
  Window xwin = meta_backend_x11_get_xwindow (x11);
  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

  XISetMask (mask.mask, XI_KeyPress);
  XISetMask (mask.mask, XI_KeyRelease);
  XISetMask (mask.mask, XI_ButtonPress);
  XISetMask (mask.mask, XI_ButtonRelease);
  XISetMask (mask.mask, XI_Enter);
  XISetMask (mask.mask, XI_Leave);
  XISetMask (mask.mask, XI_FocusIn);
  XISetMask (mask.mask, XI_FocusOut);
  XISetMask (mask.mask, XI_Motion);

  XISelectEvents (xdisplay, xwin, &mask, 1);
}

static void
get_xkbrf_var_defs (Display           *xdisplay,
                    const char        *layouts,
                    const char        *variants,
                    const char        *options,
                    char             **rules_p,
                    XkbRF_VarDefsRec  *var_defs)
{
  char *rules = NULL;

  /* Get it from the X property or fallback on defaults */
  if (!XkbRF_GetNamesProp (xdisplay, &rules, var_defs) || !rules)
    {
      rules = strdup (DEFAULT_XKB_RULES_FILE);
      var_defs->model = strdup (DEFAULT_XKB_MODEL);
      var_defs->layout = NULL;
      var_defs->variant = NULL;
      var_defs->options = NULL;
    }

  /* Swap in our new options... */
  free (var_defs->layout);
  var_defs->layout = strdup (layouts);
  free (var_defs->variant);
  var_defs->variant = strdup (variants);
  free (var_defs->options);
  var_defs->options = strdup (options);

  /* Sometimes, the property is a file path, and sometimes it's
     not. Normalize it so it's always a file path. */
  if (rules[0] == '/')
    *rules_p = g_strdup (rules);
  else
    *rules_p = g_build_filename (XKB_BASE, "rules", rules, NULL);

  free (rules);
}

static void
free_xkbrf_var_defs (XkbRF_VarDefsRec *var_defs)
{
  free (var_defs->model);
  free (var_defs->layout);
  free (var_defs->variant);
  free (var_defs->options);
}

static void
free_xkb_component_names (XkbComponentNamesRec *p)
{
  free (p->keymap);
  free (p->keycodes);
  free (p->types);
  free (p->compat);
  free (p->symbols);
  free (p->geometry);
}

static void
upload_xkb_description (Display              *xdisplay,
                        const gchar          *rules_file_path,
                        XkbRF_VarDefsRec     *var_defs,
                        XkbComponentNamesRec *comp_names)
{
  XkbDescRec *xkb_desc;
  gchar *rules_file;

  /* Upload it to the X server using the same method as setxkbmap */
  xkb_desc = XkbGetKeyboardByName (xdisplay,
                                   XkbUseCoreKbd,
                                   comp_names,
                                   XkbGBN_AllComponentsMask,
                                   XkbGBN_AllComponentsMask &
                                   (~XkbGBN_GeometryMask), True);
  if (!xkb_desc)
    {
      g_warning ("Couldn't upload new XKB keyboard description");
      return;
    }

  XkbFreeKeyboard (xkb_desc, 0, True);

  rules_file = g_path_get_basename (rules_file_path);

  if (!XkbRF_SetNamesProp (xdisplay, rules_file, var_defs))
    g_warning ("Couldn't update the XKB root window property");

  g_free (rules_file);
}

static void
apply_keymap (MetaBackendX11 *x11)
{
  MetaBackendX11Cm *x11_cm = META_BACKEND_X11_CM (x11);
  Display *xdisplay = meta_backend_x11_get_xdisplay (x11);
  XkbRF_RulesRec *xkb_rules;
  XkbRF_VarDefsRec xkb_var_defs = { 0 };
  char *rules_file_path;

  if (!x11_cm->keymap_layouts ||
      !x11_cm->keymap_variants ||
      !x11_cm->keymap_options)
    return;

  get_xkbrf_var_defs (xdisplay,
                      x11_cm->keymap_layouts,
                      x11_cm->keymap_variants,
                      x11_cm->keymap_options,
                      &rules_file_path,
                      &xkb_var_defs);

  xkb_rules = XkbRF_Load (rules_file_path, NULL, True, True);
  if (xkb_rules)
    {
      XkbComponentNamesRec xkb_comp_names = { 0 };

      XkbRF_GetComponents (xkb_rules, &xkb_var_defs, &xkb_comp_names);
      upload_xkb_description (xdisplay, rules_file_path, &xkb_var_defs, &xkb_comp_names);

      free_xkb_component_names (&xkb_comp_names);
      XkbRF_Free (xkb_rules, True);
    }
  else
    {
      g_warning ("Couldn't load XKB rules");
    }

  free_xkbrf_var_defs (&xkb_var_defs);
  g_free (rules_file_path);
}

static void
meta_backend_x11_cm_set_keymap (MetaBackend *backend,
                                const char  *layouts,
                                const char  *variants,
                                const char  *options)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Cm *x11_cm = META_BACKEND_X11_CM (x11);

  g_free (x11_cm->keymap_layouts);
  x11_cm->keymap_layouts = g_strdup (layouts);
  g_free (x11_cm->keymap_variants);
  x11_cm->keymap_variants = g_strdup (variants);
  g_free (x11_cm->keymap_options);
  x11_cm->keymap_options = g_strdup (options);

  apply_keymap (x11);
}

static void
meta_backend_x11_cm_lock_layout_group (MetaBackend *backend,
                                       guint        idx)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Cm *x11_cm = META_BACKEND_X11_CM (x11);
  Display *xdisplay = meta_backend_x11_get_xdisplay (x11);

  x11_cm->locked_group = idx;
  XkbLockGroup (xdisplay, XkbUseCoreKbd, idx);
}

static gboolean
meta_backend_x11_cm_handle_host_xevent (MetaBackendX11 *backend_x11,
                                        XEvent         *event)
{
  MetaBackend *backend = META_BACKEND (backend_x11);
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Cm *x11_cm = META_BACKEND_X11_CM (x11);
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerXrandr *monitor_manager_xrandr =
    META_MONITOR_MANAGER_XRANDR (monitor_manager);
  Display *xdisplay = meta_backend_x11_get_xdisplay (x11);

  if (event->type == meta_backend_x11_get_xkb_event_base (x11))
    {
      XkbEvent *xkb_ev = (XkbEvent *) event;

      if (xkb_ev->any.device == META_VIRTUAL_CORE_KEYBOARD_ID)
        {
          switch (xkb_ev->any.xkb_type)
            {
            case XkbStateNotify:
              if (xkb_ev->state.changed & XkbGroupLockMask)
                {
                  if (x11_cm->locked_group != xkb_ev->state.locked_group)
                    XkbLockGroup (xdisplay, XkbUseCoreKbd,
                                  x11_cm->locked_group);
                }
              break;
            default:
              break;
            }
        }
    }

  return meta_monitor_manager_xrandr_handle_xevent (monitor_manager_xrandr,
                                                    event);
}

static void
meta_backend_x11_cm_translate_device_event (MetaBackendX11 *x11,
                                            XIDeviceEvent  *device_event)
{
  Window stage_window = meta_backend_x11_get_xwindow (x11);

  if (device_event->event != stage_window)
    {
      device_event->event = stage_window;

      /* As an X11 compositor, the stage window is always at 0,0, so
       * using root coordinates will give us correct stage coordinates
       * as well... */
      device_event->event_x = device_event->root_x;
      device_event->event_y = device_event->root_y;
    }
}

static void
meta_backend_x11_cm_translate_crossing_event (MetaBackendX11 *x11,
                                              XIEnterEvent   *enter_event)
{
  Window stage_window = meta_backend_x11_get_xwindow (x11);

  if (enter_event->event != stage_window)
    {
      enter_event->event = stage_window;
      enter_event->event_x = enter_event->root_x;
      enter_event->event_y = enter_event->root_y;
    }
}

static void
meta_backend_x11_cm_init (MetaBackendX11Cm *backend_x11_cm)
{
}

static void
meta_backend_x11_cm_class_init (MetaBackendX11CmClass *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);
  MetaBackendX11Class *backend_x11_class = META_BACKEND_X11_CLASS (klass);

  backend_class->post_init = meta_backend_x11_cm_post_init;
  backend_class->create_renderer = meta_backend_x11_cm_create_renderer;
  backend_class->create_monitor_manager = meta_backend_x11_cm_create_monitor_manager;
  backend_class->create_cursor_renderer = meta_backend_x11_cm_create_cursor_renderer;
  backend_class->create_input_settings = meta_backend_x11_cm_create_input_settings;
  backend_class->update_screen_size = meta_backend_x11_cm_update_screen_size;
  backend_class->select_stage_events = meta_backend_x11_cm_select_stage_events;
  backend_class->lock_layout_group = meta_backend_x11_cm_lock_layout_group;
  backend_class->set_keymap = meta_backend_x11_cm_set_keymap;

  backend_x11_class->handle_host_xevent = meta_backend_x11_cm_handle_host_xevent;
  backend_x11_class->translate_device_event = meta_backend_x11_cm_translate_device_event;
  backend_x11_class->translate_crossing_event = meta_backend_x11_cm_translate_crossing_event;
}

