/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "meta-backend-x11.h"

#include <clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <X11/extensions/sync.h>
#include <X11/XKBlib.h>
#include <X11/Xlib-xcb.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "backends/meta-stage-private.h"
#include "backends/x11/meta-clutter-backend-x11.h"
#include "backends/x11/meta-renderer-x11.h"
#include "meta/meta-cursor-tracker.h"

#include <meta/util.h>
#include "display-private.h"
#include "compositor/compositor-private.h"
#include "backends/meta-dnd-private.h"
#include "backends/meta-idle-monitor-private.h"

struct _MetaBackendX11Private
{
  /* The host X11 display */
  Display *xdisplay;
  xcb_connection_t *xcb;
  GSource *source;

  int xsync_event_base;
  int xsync_error_base;
  XSyncAlarm user_active_alarm;
  XSyncCounter counter;

  int xinput_opcode;
  int xinput_event_base;
  int xinput_error_base;
  Time latest_evtime;

  uint8_t xkb_event_base;
  uint8_t xkb_error_base;

  struct xkb_keymap *keymap;
  xkb_layout_index_t keymap_layout_group;

  MetaLogicalMonitor *cached_current_logical_monitor;
};
typedef struct _MetaBackendX11Private MetaBackendX11Private;

static GInitableIface *initable_parent_iface;

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (MetaBackendX11, meta_backend_x11, META_TYPE_BACKEND,
                         G_ADD_PRIVATE (MetaBackendX11)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init));


static void
uint64_to_xsync_value (uint64_t    value,
                       XSyncValue *xsync_value)
{
  XSyncIntsToValue (xsync_value, value & 0xffffffff, value >> 32);
}

static XSyncAlarm
xsync_user_active_alarm_set (MetaBackendX11Private *priv)
{
  XSyncAlarmAttributes attr;
  XSyncValue delta;
  unsigned long flags;

  flags = (XSyncCACounter | XSyncCAValueType | XSyncCATestType |
           XSyncCAValue | XSyncCADelta | XSyncCAEvents);

  XSyncIntToValue (&delta, 0);
  attr.trigger.counter = priv->counter;
  attr.trigger.value_type = XSyncAbsolute;
  attr.delta = delta;
  attr.events = TRUE;

  uint64_to_xsync_value (1, &attr.trigger.wait_value);

  attr.trigger.test_type = XSyncNegativeTransition;
  return XSyncCreateAlarm (priv->xdisplay, flags, &attr);
}

static XSyncCounter
find_idletime_counter (MetaBackendX11Private *priv)
{
  int i;
  int n_counters;
  XSyncSystemCounter *counters;
  XSyncCounter counter = None;

  counters = XSyncListSystemCounters (priv->xdisplay, &n_counters);
  for (i = 0; i < n_counters; i++)
    {
      if (g_strcmp0 (counters[i].name, "IDLETIME") == 0)
        {
          counter = counters[i].counter;
          break;
        }
    }
  XSyncFreeSystemCounterList (counters);

  return counter;
}

static void
handle_alarm_notify (MetaBackend           *backend,
                     XSyncAlarmNotifyEvent *alarm_event)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  MetaIdleMonitor *idle_monitor;
  XSyncAlarmAttributes attr;

  if (alarm_event->state != XSyncAlarmActive ||
      alarm_event->alarm != priv->user_active_alarm)
    return;

  attr.events = TRUE;
  XSyncChangeAlarm (priv->xdisplay, priv->user_active_alarm,
                    XSyncCAEvents, &attr);

  idle_monitor = meta_backend_get_idle_monitor (backend, 0);
  meta_idle_monitor_reset_idletime (idle_monitor);
}

static void
meta_backend_x11_translate_device_event (MetaBackendX11 *x11,
                                         XIDeviceEvent  *device_event)
{
  MetaBackendX11Class *backend_x11_class =
    META_BACKEND_X11_GET_CLASS (x11);

  backend_x11_class->translate_device_event (x11, device_event);
}

static void
translate_device_event (MetaBackendX11 *x11,
                        XIDeviceEvent  *device_event)
{
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  meta_backend_x11_translate_device_event (x11, device_event);

  if (!device_event->send_event && device_event->time != META_CURRENT_TIME)
    {
      if (XSERVER_TIME_IS_BEFORE (device_event->time, priv->latest_evtime))
        {
          /* Emulated pointer events received after XIRejectTouch is received
           * on a passive touch grab will contain older timestamps, update those
           * so we dont get InvalidTime at grabs.
           */
          device_event->time = priv->latest_evtime;
        }

      /* Update the internal latest evtime, for any possible later use */
      priv->latest_evtime = device_event->time;
    }
}

static void
meta_backend_x11_translate_crossing_event (MetaBackendX11 *x11,
                                           XIEnterEvent   *enter_event)
{
  MetaBackendX11Class *backend_x11_class =
    META_BACKEND_X11_GET_CLASS (x11);

  if (backend_x11_class->translate_crossing_event)
    backend_x11_class->translate_crossing_event (x11, enter_event);
}

static void
translate_crossing_event (MetaBackendX11 *x11,
                          XIEnterEvent   *enter_event)
{
  /* Throw out weird events generated by grabs. */
  if (enter_event->mode == XINotifyGrab ||
      enter_event->mode == XINotifyUngrab)
    {
      enter_event->event = None;
      return;
    }

  meta_backend_x11_translate_crossing_event (x11, enter_event);
}

static void
handle_device_change (MetaBackendX11 *x11,
                      XIEvent        *event)
{
  XIDeviceChangedEvent *device_changed;

  if (event->evtype != XI_DeviceChanged)
    return;

  device_changed = (XIDeviceChangedEvent *) event;

  if (device_changed->reason != XISlaveSwitch)
    return;

  meta_backend_update_last_device (META_BACKEND (x11),
                                   device_changed->sourceid);
}

/* Clutter makes the assumption that there is only one X window
 * per stage, which is a valid assumption to make for a generic
 * application toolkit. As such, it will ignore any events sent
 * to the a stage that isn't its X window.
 *
 * When running as an X window manager, we need to respond to
 * events from lots of windows. Trick Clutter into translating
 * these events by pretending we got an event on the stage window.
 */
static void
maybe_spoof_event_as_stage_event (MetaBackendX11 *x11,
                                  XIEvent        *input_event)
{
  switch (input_event->evtype)
    {
    case XI_Motion:
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_KeyPress:
    case XI_KeyRelease:
    case XI_TouchBegin:
    case XI_TouchUpdate:
    case XI_TouchEnd:
      translate_device_event (x11, (XIDeviceEvent *) input_event);
      break;
    case XI_Enter:
    case XI_Leave:
      translate_crossing_event (x11, (XIEnterEvent *) input_event);
      break;
    default:
      break;
    }
}

static void
handle_input_event (MetaBackendX11 *x11,
                    XEvent         *event)
{
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  if (event->type == GenericEvent &&
      event->xcookie.extension == priv->xinput_opcode)
    {
      XIEvent *input_event = (XIEvent *) event->xcookie.data;

      if (input_event->evtype == XI_DeviceChanged)
        handle_device_change (x11, input_event);
      else
        maybe_spoof_event_as_stage_event (x11, input_event);
    }
}

static void
keymap_changed (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  if (priv->keymap)
    {
      xkb_keymap_unref (priv->keymap);
      priv->keymap = NULL;
    }

  g_signal_emit_by_name (backend, "keymap-changed", 0);
}

static gboolean
meta_backend_x11_handle_host_xevent (MetaBackendX11 *backend_x11,
                                     XEvent         *event)
{
  MetaBackendX11Class *backend_x11_class =
    META_BACKEND_X11_GET_CLASS (backend_x11);

  return backend_x11_class->handle_host_xevent (backend_x11, event);
}

static void
handle_host_xevent (MetaBackend *backend,
                    XEvent      *event)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  gboolean bypass_clutter = FALSE;

  XGetEventData (priv->xdisplay, &event->xcookie);

  {
    MetaDisplay *display = meta_get_display ();

    if (display)
      {
        MetaCompositor *compositor = display->compositor;
        if (meta_plugin_manager_xevent_filter (compositor->plugin_mgr, event))
          bypass_clutter = TRUE;

        if (meta_dnd_handle_xdnd_event (backend, compositor, priv->xdisplay, event))
          bypass_clutter = TRUE;
      }
  }

  bypass_clutter = (meta_backend_x11_handle_host_xevent (x11, event) ||
                    bypass_clutter);

  if (event->type == (priv->xsync_event_base + XSyncAlarmNotify))
    handle_alarm_notify (backend, (XSyncAlarmNotifyEvent *) event);

  if (event->type == priv->xkb_event_base)
    {
      XkbEvent *xkb_ev = (XkbEvent *) event;

      if (xkb_ev->any.device == META_VIRTUAL_CORE_KEYBOARD_ID)
        {
          switch (xkb_ev->any.xkb_type)
            {
            case XkbNewKeyboardNotify:
            case XkbMapNotify:
              keymap_changed (backend);
              break;
            case XkbStateNotify:
              if (xkb_ev->state.changed & XkbGroupLockMask)
                {
                  int layout_group;
                  gboolean layout_group_changed;

                  layout_group = xkb_ev->state.locked_group;
                  layout_group_changed =
                    (int) priv->keymap_layout_group != layout_group;
                  priv->keymap_layout_group = layout_group;

                  if (layout_group_changed)
                    meta_backend_notify_keymap_layout_group_changed (backend,
                                                                     layout_group);
                }
              break;
            default:
              break;
            }
        }
    }

  if (!bypass_clutter)
    {
      handle_input_event (x11, event);
      clutter_x11_handle_event (event);
    }

  XFreeEventData (priv->xdisplay, &event->xcookie);
}

typedef struct {
  GSource base;
  GPollFD event_poll_fd;
  MetaBackend *backend;
} XEventSource;

static gboolean
x_event_source_prepare (GSource *source,
                        int     *timeout)
{
  XEventSource *x_source = (XEventSource *) source;
  MetaBackend *backend = x_source->backend;
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  *timeout = -1;

  return XPending (priv->xdisplay);
}

static gboolean
x_event_source_check (GSource *source)
{
  XEventSource *x_source = (XEventSource *) source;
  MetaBackend *backend = x_source->backend;
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  return XPending (priv->xdisplay);
}

static gboolean
x_event_source_dispatch (GSource     *source,
                         GSourceFunc  callback,
                         gpointer     user_data)
{
  XEventSource *x_source = (XEventSource *) source;
  MetaBackend *backend = x_source->backend;
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  while (XPending (priv->xdisplay))
    {
      XEvent event;

      XNextEvent (priv->xdisplay, &event);

      handle_host_xevent (backend, &event);
    }

  return TRUE;
}

static GSourceFuncs x_event_funcs = {
  x_event_source_prepare,
  x_event_source_check,
  x_event_source_dispatch,
};

static GSource *
x_event_source_new (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  GSource *source;
  XEventSource *x_source;

  source = g_source_new (&x_event_funcs, sizeof (XEventSource));
  x_source = (XEventSource *) source;
  x_source->backend = backend;
  x_source->event_poll_fd.fd = ConnectionNumber (priv->xdisplay);
  x_source->event_poll_fd.events = G_IO_IN;
  g_source_add_poll (source, &x_source->event_poll_fd);

  g_source_attach (source, NULL);
  return source;
}

static void
on_monitors_changed (MetaMonitorManager *manager,
                     MetaBackend        *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  priv->cached_current_logical_monitor = NULL;
}

static void
meta_backend_x11_post_init (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  MetaMonitorManager *monitor_manager;
  int major, minor;
  gboolean has_xi = FALSE;

  priv->source = x_event_source_new (backend);

  if (!XSyncQueryExtension (priv->xdisplay, &priv->xsync_event_base, &priv->xsync_error_base) ||
      !XSyncInitialize (priv->xdisplay, &major, &minor))
    meta_fatal ("Could not initialize XSync");

  priv->counter = find_idletime_counter (priv);
  if (priv->counter == None)
    meta_fatal ("Could not initialize XSync counter");

  priv->user_active_alarm = xsync_user_active_alarm_set (priv);

  if (XQueryExtension (priv->xdisplay,
                       "XInputExtension",
                       &priv->xinput_opcode,
                       &priv->xinput_error_base,
                       &priv->xinput_event_base))
    {
      major = 2; minor = 3;
      if (XIQueryVersion (priv->xdisplay, &major, &minor) == Success)
        {
          int version = (major * 10) + minor;
          if (version >= 22)
            has_xi = TRUE;
        }
    }

  if (!has_xi)
    meta_fatal ("X server doesn't have the XInput extension, version 2.2 or newer\n");

  if (!xkb_x11_setup_xkb_extension (priv->xcb,
                                    XKB_X11_MIN_MAJOR_XKB_VERSION,
                                    XKB_X11_MIN_MINOR_XKB_VERSION,
                                    XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
                                    NULL, NULL,
                                    &priv->xkb_event_base,
                                    &priv->xkb_error_base))
    meta_fatal ("X server doesn't have the XKB extension, version %d.%d or newer\n",
                XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION);

  META_BACKEND_CLASS (meta_backend_x11_parent_class)->post_init (backend);

  monitor_manager = meta_backend_get_monitor_manager (backend);
  g_signal_connect (monitor_manager, "monitors-changed-internal",
                    G_CALLBACK (on_monitors_changed), backend);
}

static ClutterBackend *
meta_backend_x11_create_clutter_backend (MetaBackend *backend)
{
  return g_object_new (META_TYPE_CLUTTER_BACKEND_X11, NULL);
}

static gboolean
meta_backend_x11_grab_device (MetaBackend *backend,
                              int          device_id,
                              uint32_t     timestamp)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };
  int ret;

  if (timestamp != META_CURRENT_TIME)
    timestamp = MAX (timestamp, priv->latest_evtime);

  XISetMask (mask.mask, XI_ButtonPress);
  XISetMask (mask.mask, XI_ButtonRelease);
  XISetMask (mask.mask, XI_Enter);
  XISetMask (mask.mask, XI_Leave);
  XISetMask (mask.mask, XI_Motion);
  XISetMask (mask.mask, XI_KeyPress);
  XISetMask (mask.mask, XI_KeyRelease);

  ret = XIGrabDevice (priv->xdisplay, device_id,
                      meta_backend_x11_get_xwindow (x11),
                      timestamp,
                      None,
                      XIGrabModeAsync, XIGrabModeAsync,
                      False, /* owner_events */
                      &mask);

  return (ret == Success);
}

static gboolean
meta_backend_x11_ungrab_device (MetaBackend *backend,
                                int          device_id,
                                uint32_t     timestamp)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  int ret;

  ret = XIUngrabDevice (priv->xdisplay, device_id, timestamp);

  return (ret == Success);
}

static void
meta_backend_x11_warp_pointer (MetaBackend *backend,
                               int          x,
                               int          y)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  XIWarpPointer (priv->xdisplay,
                 META_VIRTUAL_CORE_POINTER_ID,
                 None,
                 meta_backend_x11_get_xwindow (x11),
                 0, 0, 0, 0,
                 x, y);
}

static MetaLogicalMonitor *
meta_backend_x11_get_current_logical_monitor (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  MetaCursorTracker *cursor_tracker;
  int x, y;
  MetaMonitorManager *monitor_manager;
  MetaLogicalMonitor *logical_monitor;

  if (priv->cached_current_logical_monitor)
    return priv->cached_current_logical_monitor;

  cursor_tracker = meta_backend_get_cursor_tracker (backend);
  meta_cursor_tracker_get_pointer (cursor_tracker, &x, &y, NULL);
  monitor_manager = meta_backend_get_monitor_manager (backend);
  logical_monitor =
    meta_monitor_manager_get_logical_monitor_at (monitor_manager, x, y);

  if (!logical_monitor && monitor_manager->logical_monitors)
    logical_monitor = monitor_manager->logical_monitors->data;

  priv->cached_current_logical_monitor = logical_monitor;
  return priv->cached_current_logical_monitor;
}

static struct xkb_keymap *
meta_backend_x11_get_keymap (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  if (priv->keymap == NULL)
    {
      struct xkb_context *context = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
      priv->keymap = xkb_x11_keymap_new_from_device (context,
                                                     priv->xcb,
                                                     xkb_x11_get_core_keyboard_device_id (priv->xcb),
                                                     XKB_KEYMAP_COMPILE_NO_FLAGS);
      if (priv->keymap == NULL)
        priv->keymap = xkb_keymap_new_from_names (context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

      xkb_context_unref (context);
    }

  return priv->keymap;
}

static xkb_layout_index_t
meta_backend_x11_get_keymap_layout_group (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  return priv->keymap_layout_group;
}

static void
meta_backend_x11_set_numlock (MetaBackend *backend,
                              gboolean     numlock_state)
{
  /* TODO: Currently handled by gnome-settings-deamon */
}

void
meta_backend_x11_handle_event (MetaBackendX11 *x11,
                               XEvent      *xevent)
{
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  priv->cached_current_logical_monitor = NULL;
}

uint8_t
meta_backend_x11_get_xkb_event_base (MetaBackendX11 *x11)
{
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  return priv->xkb_event_base;
}

static void
init_xkb_state (MetaBackendX11 *x11)
{
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  struct xkb_keymap *keymap;
  int32_t device_id;
  struct xkb_state *state;

  keymap = meta_backend_get_keymap (META_BACKEND (x11));

  device_id = xkb_x11_get_core_keyboard_device_id (priv->xcb);
  state = xkb_x11_state_new_from_device (keymap, priv->xcb, device_id);

  priv->keymap_layout_group =
    xkb_state_serialize_layout (state, XKB_STATE_LAYOUT_LOCKED);

  xkb_state_unref (state);
}

static gboolean
meta_backend_x11_initable_init (GInitable    *initable,
                                GCancellable *cancellable,
                                GError      **error)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (initable);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  Display *xdisplay;
  const char *xdisplay_name;

  xdisplay_name = g_getenv ("DISPLAY");
  if (!xdisplay_name)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Unable to open display, DISPLAY not set");
      return FALSE;
    }

  xdisplay = XOpenDisplay (xdisplay_name);
  if (!xdisplay)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Unable to open display '%s'", xdisplay_name);
      return FALSE;
    }

  priv->xdisplay = xdisplay;
  priv->xcb = XGetXCBConnection (priv->xdisplay);
  clutter_x11_set_display (xdisplay);

  init_xkb_state (x11);

  return initable_parent_iface->init (initable, cancellable, error);
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_parent_iface = g_type_interface_peek_parent (initable_iface);

  initable_iface->init = meta_backend_x11_initable_init;
}

static void
meta_backend_x11_finalize (GObject *object)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (object);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  if (priv->user_active_alarm != None)
    {
      XSyncDestroyAlarm (priv->xdisplay, priv->user_active_alarm);
      priv->user_active_alarm = None;
    }

  G_OBJECT_CLASS (meta_backend_x11_parent_class)->finalize (object);
}

static void
meta_backend_x11_class_init (MetaBackendX11Class *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_backend_x11_finalize;
  backend_class->create_clutter_backend = meta_backend_x11_create_clutter_backend;
  backend_class->post_init = meta_backend_x11_post_init;
  backend_class->grab_device = meta_backend_x11_grab_device;
  backend_class->ungrab_device = meta_backend_x11_ungrab_device;
  backend_class->warp_pointer = meta_backend_x11_warp_pointer;
  backend_class->get_current_logical_monitor = meta_backend_x11_get_current_logical_monitor;
  backend_class->get_keymap = meta_backend_x11_get_keymap;
  backend_class->get_keymap_layout_group = meta_backend_x11_get_keymap_layout_group;
  backend_class->set_numlock = meta_backend_x11_set_numlock;
}

static void
meta_backend_x11_init (MetaBackendX11 *x11)
{
  /* XInitThreads() is needed to use the "threaded swap wait" functionality
   * in Cogl - see meta_renderer_x11_create_cogl_renderer(). We call it here
   * to hopefully call it before any other use of XLib.
   */
  XInitThreads();

  /* We do X11 event retrieval ourselves */
  clutter_x11_disable_event_retrieval ();
}

Display *
meta_backend_x11_get_xdisplay (MetaBackendX11 *x11)
{
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  return priv->xdisplay;
}

Window
meta_backend_x11_get_xwindow (MetaBackendX11 *x11)
{
  ClutterActor *stage = meta_backend_get_stage (META_BACKEND (x11));
  return clutter_x11_get_stage_window (CLUTTER_STAGE (stage));
}

void
meta_backend_x11_reload_cursor (MetaBackendX11 *x11)
{
  MetaBackend *backend = META_BACKEND (x11);
  MetaCursorRenderer *cursor_renderer =
    meta_backend_get_cursor_renderer (backend);

  meta_cursor_renderer_force_update (cursor_renderer);
}
