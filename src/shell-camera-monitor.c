/*
 * Shell camera monitor
 *
 * Copyright (C) 2023 Collabora Ltd.
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

#include "shell-camera-monitor.h"

#ifdef HAVE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/utils/result.h>
#endif

#define RECONNECT_DELAY_MS 5000
#define DISABLE_DELAY_MS 500

enum {
  PROP_0,
  PROP_CAMERAS_IN_USE,
  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

struct _ShellCameraMonitor {
  GObject parent;

  gboolean cameras_in_use;

#ifdef HAVE_PIPEWIRE
  GPtrArray *node_list;
  guint reconnect_id;
  guint delayed_disable_id;

  GSource *pipewire_source;
  struct pw_context *context;
  struct pw_core *core;
  struct pw_registry *registry;
  struct spa_hook core_listener;
  struct spa_hook registry_listener;
#endif
};

G_DEFINE_TYPE (ShellCameraMonitor, shell_camera_monitor, G_TYPE_OBJECT);

#ifdef HAVE_PIPEWIRE
typedef struct _Node {
  ShellCameraMonitor *monitor;
  gboolean running;
  struct spa_hook proxy_listener;
  struct spa_hook object_listener;
} Node;

typedef struct _MetaPipeWireSource
{
  GSource source;

  struct pw_loop *pipewire_loop;
} MetaPipeWireSource;

static gboolean shell_camera_monitor_connect_core (ShellCameraMonitor *monitor);
static void shell_camera_monitor_disconnect_core (ShellCameraMonitor *monitor);

static void
delayed_disable_state (gpointer data)
{
  ShellCameraMonitor *monitor = SHELL_CAMERA_MONITOR (data);

  monitor->cameras_in_use = FALSE;
  g_object_notify_by_pspec (G_OBJECT (monitor),
                            obj_props[PROP_CAMERAS_IN_USE]);
}

static void
shell_camera_monitor_update_state (ShellCameraMonitor *monitor)
{
  gboolean new_cameras_in_use = FALSE;
  int i;

  for (i = 0; i < monitor->node_list->len; i++)
    {
      struct pw_proxy *proxy;
      Node *node;

      proxy = g_ptr_array_index (monitor->node_list, i);
      node = pw_proxy_get_user_data (proxy);

      if (node->running)
        {
          new_cameras_in_use = TRUE;
          break;
        }
    }

  if (new_cameras_in_use)
    g_clear_handle_id (&monitor->delayed_disable_id, g_source_remove);

  if (new_cameras_in_use && !monitor->cameras_in_use)
    {
      monitor->cameras_in_use = new_cameras_in_use;
      g_object_notify_by_pspec (G_OBJECT (monitor),
                                obj_props[PROP_CAMERAS_IN_USE]);
    }
  else if (!new_cameras_in_use && monitor->cameras_in_use &&
           monitor->delayed_disable_id == 0)
    {
      monitor->delayed_disable_id =
        g_timeout_add_once (DISABLE_DELAY_MS, delayed_disable_state, monitor);
    }
}

static void
proxy_destroy (void *data)
{
  Node *node = data;

  spa_hook_remove (&node->proxy_listener);
  spa_hook_remove (&node->object_listener);
}

static const struct pw_proxy_events proxy_events = {
  PW_VERSION_PROXY_EVENTS,
  .destroy = proxy_destroy,
};

static void
node_event_info (void                      *data,
                 const struct pw_node_info *info)
{
  Node *node = data;

  node->running = (info->state == PW_NODE_STATE_RUNNING);

  shell_camera_monitor_update_state (node->monitor);
}

static const struct pw_node_events node_events = {
  PW_VERSION_NODE_EVENTS,
  .info = node_event_info,
};

static void
registry_event_global (void                  *data,
                       uint32_t               id,
                       uint32_t               permissions,
                       const char            *type,
                       uint32_t               version,
                       const struct spa_dict *props)
{
  ShellCameraMonitor *monitor = SHELL_CAMERA_MONITOR (data);
  struct pw_proxy *proxy;
  const char *prop_str;
  Node *node;

  if (!props || !(spa_streq (type, PW_TYPE_INTERFACE_Node)))
    return;

  prop_str = spa_dict_lookup (props, PW_KEY_MEDIA_ROLE);
  if (!prop_str || (strcmp (prop_str, "Camera") != 0))
    return;

  proxy = pw_registry_bind (monitor->registry,
                            id,
                            PW_TYPE_INTERFACE_Node,
                            PW_VERSION_NODE,
                            sizeof (Node));
  node = pw_proxy_get_user_data (proxy);
  node->monitor = monitor;

  pw_proxy_add_listener (proxy,
                         &node->proxy_listener,
                         &proxy_events,
                         node);

  pw_proxy_add_object_listener (proxy,
                                &node->object_listener,
                                &node_events,
                                node);

  g_ptr_array_add (monitor->node_list, proxy);
}

static void
registry_event_global_remove (void     *data,
                              uint32_t  id)
{
  ShellCameraMonitor *monitor = SHELL_CAMERA_MONITOR (data);
  struct pw_proxy *proxy_to_remove = NULL;
  int i;

  for (i = 0; i < monitor->node_list->len; i++)
    {
      struct pw_proxy *proxy;

      proxy = g_ptr_array_index (monitor->node_list, i);
      if (pw_proxy_get_bound_id (proxy) == id)
        {
          proxy_to_remove = proxy;
          break;
        }
    }

  if (proxy_to_remove)
    g_ptr_array_remove (monitor->node_list, proxy_to_remove);
}

static const struct pw_registry_events registry_events = {
  PW_VERSION_REGISTRY_EVENTS,
  .global = registry_event_global,
  .global_remove = registry_event_global_remove,
};

static void
idle_reconnect (gpointer data)
{
  ShellCameraMonitor *monitor = SHELL_CAMERA_MONITOR (data);

  if (shell_camera_monitor_connect_core (monitor))
    monitor->reconnect_id = 0;
  else
    monitor->reconnect_id =
      g_timeout_add_once (RECONNECT_DELAY_MS, idle_reconnect, monitor);
}

static void
on_core_error (void       *data,
               uint32_t    id,
               int         seq,
               int         res,
               const char *message)
{
  if (id == 0 && res == -EPIPE)
    {
      ShellCameraMonitor *monitor = SHELL_CAMERA_MONITOR (data);

      shell_camera_monitor_disconnect_core (monitor);
      if (monitor->cameras_in_use)
        {
          monitor->cameras_in_use = FALSE;
          g_object_notify_by_pspec (G_OBJECT (monitor),
                                    obj_props[PROP_CAMERAS_IN_USE]);
        }

      if (monitor->reconnect_id == 0)
        monitor->reconnect_id =
          g_timeout_add_once (RECONNECT_DELAY_MS, idle_reconnect, monitor);
    }
}

static const struct pw_core_events core_events = {
  PW_VERSION_CORE_EVENTS,
  .error = on_core_error,
};

static gboolean
shell_camera_monitor_connect_core (ShellCameraMonitor *monitor)
{
  monitor->core = pw_context_connect (monitor->context, NULL, 0);
  if (!monitor->core)
    return FALSE;

  pw_core_add_listener (monitor->core,
                        &monitor->core_listener,
                        &core_events,
                        monitor);

  monitor->registry = pw_core_get_registry (monitor->core,
                                            PW_VERSION_REGISTRY,
                                            0);
  pw_registry_add_listener (monitor->registry,
                            &monitor->registry_listener,
                            &registry_events,
                            monitor);
  return TRUE;
}

static void
shell_camera_monitor_disconnect_core (ShellCameraMonitor *monitor)
{
  g_ptr_array_set_size (monitor->node_list, 0);
  g_clear_handle_id (&monitor->delayed_disable_id, g_source_remove);

  spa_hook_remove (&monitor->registry_listener);
  if (monitor->registry != NULL)
    {
      pw_proxy_destroy ((struct pw_proxy *) monitor->registry);
      monitor->registry = NULL;
    }
  spa_hook_remove (&monitor->core_listener);
  g_clear_pointer (&monitor->core, pw_core_disconnect);
}

static gboolean
pipewire_loop_source_prepare (GSource *source,
                              int     *timeout)
{
  *timeout = -1;
  return FALSE;
}

static gboolean
pipewire_loop_source_dispatch (GSource     *source,
                               GSourceFunc  callback,
                               gpointer     user_data)
{
  MetaPipeWireSource *pipewire_source = (MetaPipeWireSource *) source;
  int result;

  result = pw_loop_iterate (pipewire_source->pipewire_loop, 0);
  if (result < 0)
    g_warning ("pipewire_loop_iterate failed: %s", spa_strerror (result));

  return TRUE;
}

static void
pipewire_loop_source_finalize (GSource *source)
{
  MetaPipeWireSource *pipewire_source = (MetaPipeWireSource *) source;

  pw_loop_leave (pipewire_source->pipewire_loop);
  pw_loop_destroy (pipewire_source->pipewire_loop);
}

static GSourceFuncs pipewire_source_funcs =
{
  pipewire_loop_source_prepare,
  NULL,
  pipewire_loop_source_dispatch,
  pipewire_loop_source_finalize
};

static GSource *
create_pipewire_source (struct pw_loop *pipewire_loop)
{
  GSource *source;
  MetaPipeWireSource *pipewire_source;

  source = g_source_new (&pipewire_source_funcs, sizeof (MetaPipeWireSource));
  g_source_set_name (source, "[gnome-shell] PipeWire");

  pipewire_source = (MetaPipeWireSource *) source;
  pipewire_source->pipewire_loop = pipewire_loop;

  g_source_add_unix_fd (source,
                        pw_loop_get_fd (pipewire_source->pipewire_loop),
                        G_IO_IN | G_IO_ERR);

  pw_loop_enter (pipewire_source->pipewire_loop);
  g_source_attach (source, NULL);
  g_source_unref (source);

  return source;
}

#endif

static void
shell_camera_monitor_finalize (GObject *object)
{
#ifdef HAVE_PIPEWIRE
  ShellCameraMonitor *monitor = SHELL_CAMERA_MONITOR (object);

  shell_camera_monitor_disconnect_core (monitor);
  g_clear_pointer (&monitor->node_list, g_ptr_array_unref);
  g_clear_pointer (&monitor->context, pw_context_destroy);
  g_clear_pointer (&monitor->pipewire_source, g_source_destroy);
  g_clear_handle_id (&monitor->reconnect_id, g_source_remove);
  pw_deinit ();
#endif

  G_OBJECT_CLASS (shell_camera_monitor_parent_class)->finalize (object);
}

static void
shell_camera_monitor_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ShellCameraMonitor *monitor = SHELL_CAMERA_MONITOR (object);

  switch (prop_id)
    {
    case PROP_CAMERAS_IN_USE:
      g_value_set_boolean (value, monitor->cameras_in_use);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_camera_monitor_class_init (ShellCameraMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = shell_camera_monitor_finalize;
  object_class->get_property = shell_camera_monitor_get_property;

  obj_props[PROP_CAMERAS_IN_USE] =
    g_param_spec_boolean ("cameras-in-use", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
shell_camera_monitor_init (ShellCameraMonitor *monitor)
{
#ifdef HAVE_PIPEWIRE
  struct pw_loop *pipewire_loop;

  monitor->node_list =
    g_ptr_array_new_full (5, (GDestroyNotify) pw_proxy_destroy);

  pw_init (NULL, NULL);

  pipewire_loop = pw_loop_new (NULL);
  if (!pipewire_loop)
    goto error;

  monitor->pipewire_source = create_pipewire_source (pipewire_loop);
  if (!monitor->pipewire_source)
    goto error;

  monitor->context = pw_context_new (pipewire_loop, NULL, 0);
  if (!monitor->context)
    goto error;

  if (!shell_camera_monitor_connect_core (monitor))
    goto error;

  return;

error:
    g_message ("Failed to start camera monitor");
#endif
}

gboolean
shell_camera_monitor_get_cameras_in_use (ShellCameraMonitor *monitor)
{
  g_return_val_if_fail (SHELL_IS_CAMERA_MONITOR (monitor), FALSE);
  return monitor->cameras_in_use;
}
