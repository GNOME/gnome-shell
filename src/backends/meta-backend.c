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

#include "meta-backend.h"
#include "meta-backend-private.h"

#include <clutter/clutter.h>

#include "backends/x11/meta-backend-x11.h"

#ifdef HAVE_NATIVE_BACKEND
#include "backends/native/meta-backend-native.h"
#endif

static MetaBackend *_backend;

MetaBackend *
meta_get_backend (void)
{
  return _backend;
}

struct _MetaBackendPrivate
{
  MetaMonitorManager *monitor_manager;
  MetaCursorRenderer *cursor_renderer;
};
typedef struct _MetaBackendPrivate MetaBackendPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MetaBackend, meta_backend, G_TYPE_OBJECT);

static void
meta_backend_finalize (GObject *object)
{
  MetaBackend *backend = META_BACKEND (object);
  MetaBackendPrivate *priv = meta_backend_get_instance_private (backend);
  int i;

  g_clear_object (&priv->monitor_manager);

  for (i = 0; i <= backend->device_id_max; i++)
    {
      if (backend->device_monitors[i])
        g_object_unref (backend->device_monitors[i]);
    }

  G_OBJECT_CLASS (meta_backend_parent_class)->finalize (object);
}

static void
meta_backend_real_post_init (MetaBackend *backend)
{
  MetaBackendPrivate *priv = meta_backend_get_instance_private (backend);

  priv->monitor_manager = META_BACKEND_GET_CLASS (backend)->create_monitor_manager (backend);
  priv->cursor_renderer = META_BACKEND_GET_CLASS (backend)->create_cursor_renderer (backend);
}

static MetaCursorRenderer *
meta_backend_real_create_cursor_renderer (MetaBackend *backend)
{
  return meta_cursor_renderer_new ();
}

static gboolean
meta_backend_real_grab_device (MetaBackend *backend,
                               int          device_id,
                               uint32_t     timestamp)
{
  /* Do nothing */
  return TRUE;
}

static gboolean
meta_backend_real_ungrab_device (MetaBackend *backend,
                                 int          device_id,
                                 uint32_t     timestamp)
{
  /* Do nothing */
  return TRUE;
}

static void
meta_backend_real_warp_pointer (MetaBackend *backend,
                                int          x,
                                int          y)
{
  /* Do nothing */
}

static void
meta_backend_class_init (MetaBackendClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_backend_finalize;

  klass->post_init = meta_backend_real_post_init;
  klass->create_cursor_renderer = meta_backend_real_create_cursor_renderer;
  klass->grab_device = meta_backend_real_grab_device;
  klass->ungrab_device = meta_backend_real_ungrab_device;
  klass->warp_pointer = meta_backend_real_warp_pointer;
}

static void
meta_backend_init (MetaBackend *backend)
{
  _backend = backend;
}

/* FIXME -- destroy device monitors at some point */
G_GNUC_UNUSED static void
destroy_device_monitor (MetaBackend *backend,
                        int          device_id)
{
  g_clear_object (&backend->device_monitors[device_id]);
  if (device_id == backend->device_id_max)
    backend->device_id_max--;
}

static MetaIdleMonitor *
meta_backend_create_idle_monitor (MetaBackend *backend,
                                  int          device_id)
{
  return META_BACKEND_GET_CLASS (backend)->create_idle_monitor (backend, device_id);
}

static void
meta_backend_post_init (MetaBackend *backend)
{
  META_BACKEND_GET_CLASS (backend)->post_init (backend);
}

MetaIdleMonitor *
meta_backend_get_idle_monitor (MetaBackend *backend,
                               int          device_id)
{
  g_return_val_if_fail (device_id >= 0 && device_id < 256, NULL);

  if (!backend->device_monitors[device_id])
    {
      backend->device_monitors[device_id] = meta_backend_create_idle_monitor (backend, device_id);
      backend->device_id_max = MAX (backend->device_id_max, device_id);
    }

  return backend->device_monitors[device_id];
}

MetaMonitorManager *
meta_backend_get_monitor_manager (MetaBackend *backend)
{
  MetaBackendPrivate *priv = meta_backend_get_instance_private (backend);

  return priv->monitor_manager;
}

MetaCursorRenderer *
meta_backend_get_cursor_renderer (MetaBackend *backend)
{
  MetaBackendPrivate *priv = meta_backend_get_instance_private (backend);

  return priv->cursor_renderer;
}

gboolean
meta_backend_grab_device (MetaBackend *backend,
                          int          device_id,
                          uint32_t     timestamp)
{
  return META_BACKEND_GET_CLASS (backend)->grab_device (backend, device_id, timestamp);
}

gboolean
meta_backend_ungrab_device (MetaBackend *backend,
                            int          device_id,
                            uint32_t     timestamp)
{
  return META_BACKEND_GET_CLASS (backend)->ungrab_device (backend, device_id, timestamp);
}

void
meta_backend_warp_pointer (MetaBackend *backend,
                           int          x,
                           int          y)
{
  META_BACKEND_GET_CLASS (backend)->warp_pointer (backend, x, y);
}

static GType
get_backend_type (void)
{
#if defined(CLUTTER_WINDOWING_X11)
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_X11))
    return META_TYPE_BACKEND_X11;
#endif

#if defined(CLUTTER_WINDOWING_EGL) && defined(HAVE_NATIVE_BACKEND)
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_EGL))
    return META_TYPE_BACKEND_NATIVE;
#endif

  g_assert_not_reached ();
}

static void
meta_create_backend (void)
{
  /* meta_backend_init() above install the backend globally so
   * so meta_get_backend() works even during initialization. */
  g_object_new (get_backend_type (), NULL);
}

/* Mutter is responsible for pulling events off the X queue, so Clutter
 * doesn't need (and shouldn't) run its normal event source which polls
 * the X fd, but we do have to deal with dispatching events that accumulate
 * in the clutter queue. This happens, for example, when clutter generate
 * enter/leave events on mouse motion - several events are queued in the
 * clutter queue but only one dispatched. It could also happen because of
 * explicit calls to clutter_event_put(). We add a very simple custom
 * event loop source which is simply responsible for pulling events off
 * of the queue and dispatching them before we block for new events.
 */

static gboolean
event_prepare (GSource    *source,
               gint       *timeout_)
{
  *timeout_ = -1;

  return clutter_events_pending ();
}

static gboolean
event_check (GSource *source)
{
  return clutter_events_pending ();
}

static gboolean
event_dispatch (GSource    *source,
                GSourceFunc callback,
                gpointer    user_data)
{
  ClutterEvent *event = clutter_event_get ();

  if (event)
    {
      clutter_do_event (event);
      clutter_event_free (event);
    }

  return TRUE;
}

static GSourceFuncs event_funcs = {
  event_prepare,
  event_check,
  event_dispatch
};

void
meta_clutter_init (void)
{
  GSource *source;

  meta_create_backend ();

  if (clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    g_error ("Unable to initialize Clutter.\n");

  source = g_source_new (&event_funcs, sizeof (GSource));
  g_source_attach (source, NULL);
  g_source_unref (source);

  meta_backend_post_init (_backend);
}
