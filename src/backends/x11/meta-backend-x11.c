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

#include "meta-backend-x11.h"

#include <clutter/x11/clutter-x11.h>

#include <X11/extensions/sync.h>

#include "meta-idle-monitor-xsync.h"
#include "meta-monitor-manager-xrandr.h"
#include "backends/meta-monitor-manager-dummy.h"

#include "meta-cursor-tracker-private.h"
#include "meta-cursor.h"
#include <meta/util.h>

struct _MetaBackendX11Private
{
  /* The host X11 display */
  Display *xdisplay;
  GSource *source;

  int xsync_event_base;
  int xsync_error_base;
};
typedef struct _MetaBackendX11Private MetaBackendX11Private;

G_DEFINE_TYPE_WITH_PRIVATE (MetaBackendX11, meta_backend_x11, META_TYPE_BACKEND);

static void
handle_alarm_notify (MetaBackend *backend,
                     XEvent      *xevent)
{
  int i;

  for (i = 0; i <= backend->device_id_max; i++)
    if (backend->device_monitors[i])
      meta_idle_monitor_xsync_handle_xevent (backend->device_monitors[i], (XSyncAlarmNotifyEvent*)xevent);
}

static void
handle_host_xevent (MetaBackend *backend,
                    XEvent      *xevent)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  gboolean bypass_clutter = FALSE;

  if (xevent->type == (priv->xsync_event_base + XSyncAlarmNotify))
    handle_alarm_notify (backend, xevent);

  {
    MetaMonitorManager *manager = meta_backend_get_monitor_manager (backend);
    if (META_IS_MONITOR_MANAGER_XRANDR (manager) &&
        meta_monitor_manager_xrandr_handle_xevent (META_MONITOR_MANAGER_XRANDR (manager), xevent))
      {
        bypass_clutter = TRUE;
        goto out;
      }
  }

 out:
  if (!bypass_clutter)
    clutter_x11_handle_event (xevent);
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
      XEvent xev;

      XNextEvent (priv->xdisplay, &xev);

      handle_host_xevent (backend, &xev);
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
meta_backend_x11_post_init (MetaBackend *backend)
{
  MetaBackendX11 *x11 = META_BACKEND_X11 (backend);
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);
  int major, minor;

  priv->xdisplay = clutter_x11_get_default_display ();

  priv->source = x_event_source_new (backend);

  if (!XSyncQueryExtension (priv->xdisplay, &priv->xsync_event_base, &priv->xsync_error_base) ||
      !XSyncInitialize (priv->xdisplay, &major, &minor))
    meta_fatal ("Could not initialize XSync");

  META_BACKEND_CLASS (meta_backend_x11_parent_class)->post_init (backend);
}

static MetaIdleMonitor *
meta_backend_x11_create_idle_monitor (MetaBackend *backend,
                                      int          device_id)
{
  return g_object_new (META_TYPE_IDLE_MONITOR_XSYNC,
                       "device-id", device_id,
                       NULL);
}

static MetaMonitorManager *
meta_backend_x11_create_monitor_manager (MetaBackend *backend)
{
  /* If we're a Wayland compositor using the X11 backend,
   * we're a nested configuration, so return the dummy
   * monitor setup. */
  if (meta_is_wayland_compositor ())
    return g_object_new (META_TYPE_MONITOR_MANAGER_DUMMY, NULL);

  return g_object_new (META_TYPE_MONITOR_MANAGER_XRANDR, NULL);
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

  XISetMask (mask.mask, XI_ButtonPress);
  XISetMask (mask.mask, XI_ButtonRelease);
  XISetMask (mask.mask, XI_Enter);
  XISetMask (mask.mask, XI_Leave);
  XISetMask (mask.mask, XI_Motion);

  MetaCursorTracker *tracker = meta_cursor_tracker_get_for_screen (NULL);
  MetaCursorReference *cursor_ref = meta_cursor_tracker_get_displayed_cursor (tracker);
  MetaCursor cursor = meta_cursor_reference_get_meta_cursor (cursor_ref);

  ret = XIGrabDevice (priv->xdisplay, device_id,
                      DefaultRootWindow (priv->xdisplay),
                      timestamp,
                      meta_cursor_create_x_cursor (priv->xdisplay, cursor),
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
meta_backend_x11_class_init (MetaBackendX11Class *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);

  backend_class->post_init = meta_backend_x11_post_init;
  backend_class->create_idle_monitor = meta_backend_x11_create_idle_monitor;
  backend_class->create_monitor_manager = meta_backend_x11_create_monitor_manager;

  backend_class->grab_device = meta_backend_x11_grab_device;
  backend_class->ungrab_device = meta_backend_x11_ungrab_device;
}

static void
meta_backend_x11_init (MetaBackendX11 *x11)
{
  /* We do X11 event retrieval ourselves */
  clutter_x11_disable_event_retrieval ();
}

Display *
meta_backend_x11_get_xdisplay (MetaBackendX11 *x11)
{
  MetaBackendX11Private *priv = meta_backend_x11_get_instance_private (x11);

  return priv->xdisplay;
}

