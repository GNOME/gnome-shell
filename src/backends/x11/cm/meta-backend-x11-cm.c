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

#include "backends/meta-backend-private.h"
#include "backends/x11/meta-cursor-renderer-x11.h"
#include "backends/x11/meta-monitor-manager-xrandr.h"

struct _MetaBackendX11Cm
{
  MetaBackendX11 parent;
};

G_DEFINE_TYPE (MetaBackendX11Cm, meta_backend_x11_cm, META_TYPE_BACKEND_X11)

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
meta_backend_x11_cm_post_init (MetaBackend *backend)
{
  MetaBackendClass *parent_backend_class =
    META_BACKEND_CLASS (meta_backend_x11_cm_parent_class);

  parent_backend_class->post_init (backend);

  take_touch_grab (backend);
}

static MetaMonitorManager *
meta_backend_x11_cm_create_monitor_manager (MetaBackend *backend)
{
  return g_object_new (META_TYPE_MONITOR_MANAGER_XRANDR, NULL);
}

static MetaCursorRenderer *
meta_backend_x11_cm_create_cursor_renderer (MetaBackend *backend)
{
  return g_object_new (META_TYPE_CURSOR_RENDERER_X11, NULL);
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

static gboolean
meta_backend_x11_cm_handle_host_xevent (MetaBackendX11 *backend_x11,
                                        XEvent         *event)
{
  MetaBackend *backend = META_BACKEND (backend_x11);
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerXrandr *monitor_manager_xrandr =
    META_MONITOR_MANAGER_XRANDR (monitor_manager);

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
  backend_class->create_monitor_manager = meta_backend_x11_cm_create_monitor_manager;
  backend_class->create_cursor_renderer = meta_backend_x11_cm_create_cursor_renderer;
  backend_class->update_screen_size = meta_backend_x11_cm_update_screen_size;
  backend_class->select_stage_events = meta_backend_x11_cm_select_stage_events;

  backend_x11_class->handle_host_xevent = meta_backend_x11_cm_handle_host_xevent;
  backend_x11_class->translate_device_event = meta_backend_x11_cm_translate_device_event;
  backend_x11_class->translate_crossing_event = meta_backend_x11_cm_translate_crossing_event;
}

